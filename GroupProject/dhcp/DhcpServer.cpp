// DHCP server (RFC 2131): hands out addresses from a pool and remembers leases by MAC.
//   DISCOVER -> OFFER    (reserve an address for this MAC for 30 s)
//   REQUEST  -> ACK/NAK  (confirm the reservation, start the lease)
//   RELEASE  -> free the address        DECLINE -> mark the address as unusable
//
// Build:  g++ -std=c++17 -o dhcp_server DhcpServer.cpp      (Linux / macOS)
// Run:    sudo ./dhcp_server <interface> <server-ip> <pool-first> <pool-last> [lease-seconds]
// e.g.    sudo ./dhcp_server br0 192.168.50.1 192.168.50.100 192.168.50.102 120
//
// Never run it on a real shared network (college/office Wi-Fi): a second DHCP server
// there would hand out wrong addresses to other people. Use the virtual LAN in demo.sh.

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <unistd.h>
#include "dhcp_packet.h"

// ---------------------------------------------------------------- Configuration
struct Config {
    std::string iface;
    uint32_t serverIp{};      // all addresses here in HOST byte order (easy to count with ++)
    uint32_t poolFirst{};
    uint32_t poolLast{};
    uint32_t subnetMask = 0xFFFFFF00;   // 255.255.255.0
    uint32_t dns = 0x08080808;          // 8.8.8.8
    uint32_t leaseSeconds = 120;
};
constexpr int OFFER_HOLD_SECONDS = 30;  // how long an OFFER reserves its address
Config cfg;

// ---------------------------------------------------------------- Lease table
enum class State { Offered, Bound, Declined };
struct Lease {
    uint8_t mac[6];
    uint32_t ip;          // host byte order
    State state;
    time_t expires;
};
std::vector<Lease> leases;

std::string ipStr(uint32_t hostOrderIp) {
    in_addr a{};
    a.s_addr = htonl(hostOrderIp);
    return inet_ntoa(a);
}
std::string macStr(const uint8_t* m) {
    char buf[18];
    std::snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x",
                  m[0], m[1], m[2], m[3], m[4], m[5]);
    return buf;
}

void printLeases() {
    time_t now = std::time(nullptr);
    std::printf("  +-------------------+-----------------+----------+---------+\n");
    std::printf("  | MAC               | IP              | state    | expires |\n");
    std::printf("  +-------------------+-----------------+----------+---------+\n");
    for (const auto& l : leases) {
        const char* state = l.state == State::Bound ? "BOUND"
                          : l.state == State::Offered ? "OFFERED" : "DECLINED";
        std::printf("  | %s | %-15s | %-8s | %5lds  |\n", macStr(l.mac).c_str(),
                    ipStr(l.ip).c_str(), state, long(l.expires - now));
    }
    if (leases.empty()) std::printf("  | (no leases)                                              |\n");
    std::printf("  +-------------------+-----------------+----------+---------+\n");
}

void removeExpired() {
    time_t now = std::time(nullptr);
    for (size_t i = 0; i < leases.size();) {
        if (leases[i].expires <= now) {
            std::printf("  lease expired: %s (%s)\n", ipStr(leases[i].ip).c_str(),
                        macStr(leases[i].mac).c_str());
            leases.erase(leases.begin() + i);
        } else {
            ++i;
        }
    }
}

Lease* findByMac(const uint8_t* mac) {
    for (auto& l : leases)
        if (l.state != State::Declined && std::memcmp(l.mac, mac, 6) == 0) return &l;
    return nullptr;
}

bool ipInUse(uint32_t ip) {
    for (const auto& l : leases)
        if (l.ip == ip) return true;
    return ip == cfg.serverIp;
}

// Same MAC gets the same address back; otherwise the first free one in the pool.
Lease* allocate(const uint8_t* mac) {
    removeExpired();
    if (Lease* existing = findByMac(mac)) return existing;
    for (uint32_t ip = cfg.poolFirst; ip <= cfg.poolLast; ++ip) {
        if (ipInUse(ip)) continue;
        Lease l{};
        std::memcpy(l.mac, mac, 6);
        l.ip = ip;
        l.state = State::Offered;
        l.expires = std::time(nullptr) + OFFER_HOLD_SECONDS;
        leases.push_back(l);
        return &leases.back();
    }
    return nullptr;   // pool exhausted
}

void forget(const uint8_t* mac) {
    for (size_t i = 0; i < leases.size(); ++i)
        if (leases[i].state != State::Declined && std::memcmp(leases[i].mac, mac, 6) == 0) {
            leases.erase(leases.begin() + i);
            return;
        }
}

// ---------------------------------------------------------------- Replies
// Builds OFFER / ACK / NAK from the client's message and broadcasts it to port 68.
// Broadcast always works, even though the client has no IP yet.
void sendReply(int sock, const Dhcp_packet_t& request, uint8_t type, uint32_t yourIp) {
    Dhcp_packet_t p{};
    p.op = BOOTREPLY;
    p.htype = 1;
    p.hlen = 6;
    p.xid = request.xid;            // copy the client's transaction ID
    p.flags = request.flags;
    p.giaddr = request.giaddr;
    std::memcpy(p.chaddr, request.chaddr, sizeof(p.chaddr));
    p.yiaddr = type == DHCPNAK ? 0 : htonl(yourIp);
    p.magic_cookie = htonl(DHCP_MAGIC_COOKIE);

    std::size_t i = 0;
    uint32_t serverId = htonl(cfg.serverIp);
    addOption(p, i, OPT_MSG_TYPE, &type, 1);
    addOption(p, i, OPT_SERVER_ID, &serverId, 4);
    if (type != DHCPNAK) {
        uint32_t lease = htonl(cfg.leaseSeconds);
        uint32_t mask = htonl(cfg.subnetMask);
        uint32_t dns = htonl(cfg.dns);
        addOption(p, i, OPT_LEASE_TIME, &lease, 4);
        addOption(p, i, OPT_SUBNET_MASK, &mask, 4);
        addOption(p, i, OPT_ROUTER, &serverId, 4);   // this server is also the gateway
        addOption(p, i, OPT_DNS_SERVER, &dns, 4);
    }
    std::size_t length = finishOptions(p, i);

    sockaddr_in to{};
    to.sin_family = AF_INET;
    to.sin_port = htons(CLIENT_PORT);
    to.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    if (::sendto(sock, &p, length, 0, reinterpret_cast<sockaddr*>(&to), sizeof(to)) < 0)
        std::perror("sendto");
    else
        std::printf("  -> DHCP%s%s to %s\n", messageName(type),
                    type == DHCPNAK ? "" : (" " + ipStr(yourIp)).c_str(), macStr(p.chaddr).c_str());
}

// ---------------------------------------------------------------- Message handling
void handle(int sock, const Dhcp_packet_t& p, std::size_t length) {
    DhcpOptions o;
    if (p.op != BOOTREQUEST || p.htype != 1 || p.hlen != 6 || !parseOptions(p, length, o))
        return;   // not a valid DHCP request from an Ethernet client
    uint8_t type = o.byte(OPT_MSG_TYPE);
    const uint8_t* mac = p.chaddr;
    std::printf("\n<- DHCP%s from %s (xid 0x%08x)\n", messageName(type), macStr(mac).c_str(),
                unsigned(ntohl(p.xid)));

    switch (type) {
    case DHCPDISCOVER: {
        Lease* l = allocate(mac);
        if (!l) {
            std::printf("  pool exhausted, no OFFER sent\n");
            break;
        }
        if (l->state == State::Offered) l->expires = std::time(nullptr) + OFFER_HOLD_SECONDS;
        sendReply(sock, p, DHCPOFFER, l->ip);
        break;
    }
    case DHCPREQUEST: {
        uint32_t serverId = ntohl(o.ip(OPT_SERVER_ID));
        if (o.has(OPT_SERVER_ID) && serverId != cfg.serverIp) {
            // The client accepted another server's offer: drop our reservation.
            std::printf("  client chose server %s, withdrawing our offer\n",
                        ipStr(serverId).c_str());
            Lease* l = findByMac(mac);
            if (l && l->state == State::Offered) forget(mac);
            break;
        }
        // The address asked for: option 50 (new lease) or ciaddr (renewing).
        uint32_t wanted = ntohl(o.has(OPT_REQUESTED_IP) ? o.ip(OPT_REQUESTED_IP) : p.ciaddr);
        removeExpired();
        Lease* l = findByMac(mac);
        if (l && l->ip == wanted) {
            l->state = State::Bound;
            l->expires = std::time(nullptr) + cfg.leaseSeconds;
            sendReply(sock, p, DHCPACK, l->ip);
        } else if (!l && !o.has(OPT_SERVER_ID)) {
            // Client reusing an old address we know nothing about: RFC 2131 says stay silent.
            std::printf("  no record of this client, staying silent\n");
            return;
        } else {
            std::printf("  %s is not reserved for this MAC\n", ipStr(wanted).c_str());
            sendReply(sock, p, DHCPNAK, 0);
        }
        break;
    }
    case DHCPRELEASE:
        std::printf("  client released %s\n", ipStr(ntohl(p.ciaddr)).c_str());
        forget(mac);
        break;
    case DHCPDECLINE: {
        // The client found (via ARP) that someone already uses this IP: stop handing it out.
        Lease* l = findByMac(mac);
        if (l) {
            std::printf("  %s is in use by another host, marking it DECLINED\n",
                        ipStr(l->ip).c_str());
            l->state = State::Declined;
            l->expires = std::time(nullptr) + 300;
        }
        break;
    }
    default:
        std::printf("  ignored\n");
        return;
    }
    printLeases();
}

// ---------------------------------------------------------------- main
bool parseIp(const char* text, uint32_t& out) {
    in_addr a{};
    if (inet_pton(AF_INET, text, &a) != 1) return false;
    out = ntohl(a.s_addr);
    return true;
}

int main(int argc, char* argv[]) {
    if (argc < 5 || argc > 6) {
        std::printf("Usage: sudo %s <interface> <server-ip> <pool-first> <pool-last> "
                    "[lease-seconds]\n"
                    "  e.g. sudo %s br0 192.168.50.1 192.168.50.100 192.168.50.102 120\n",
                    argv[0], argv[0]);
        return 1;
    }
    cfg.iface = argv[1];
    if (!parseIp(argv[2], cfg.serverIp) || !parseIp(argv[3], cfg.poolFirst) ||
        !parseIp(argv[4], cfg.poolLast) || cfg.poolFirst > cfg.poolLast) {
        std::fprintf(stderr, "Invalid IP address or pool range.\n");
        return 1;
    }
    if ((cfg.poolFirst & cfg.subnetMask) != (cfg.serverIp & cfg.subnetMask) ||
        (cfg.poolLast & cfg.subnetMask) != (cfg.serverIp & cfg.subnetMask)) {
        std::fprintf(stderr, "The pool must be in the server's /24 subnet.\n");
        return 1;
    }
    if (argc == 6) cfg.leaseSeconds = static_cast<uint32_t>(std::atoi(argv[5]));
    if (cfg.leaseSeconds == 0) cfg.leaseSeconds = 120;

    // UDP socket on port 67, allowed to broadcast, tied to one interface.
    int sock = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    int on = 1;
    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_port = htons(SERVER_PORT);
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    if (sock < 0 ||
        ::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) != 0 ||
        ::setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on)) != 0 ||
#ifdef __linux__
        ::setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, cfg.iface.c_str(),
                     static_cast<socklen_t>(cfg.iface.size() + 1)) != 0 ||
#elif defined(__APPLE__)
        [&] { unsigned idx = if_nametoindex(cfg.iface.c_str());
              return ::setsockopt(sock, IPPROTO_IP, IP_BOUND_IF, &idx, sizeof(idx)); }() != 0 ||
#endif
        ::bind(sock, reinterpret_cast<sockaddr*>(&local), sizeof(local)) != 0) {
        std::perror("socket setup (run with sudo, check the interface name)");
        return 1;
    }

    std::printf("DHCP server %s on %s\n", ipStr(cfg.serverIp).c_str(), cfg.iface.c_str());
    std::printf("  pool %s - %s, mask %s, router %s, DNS %s, lease %us\n",
                ipStr(cfg.poolFirst).c_str(), ipStr(cfg.poolLast).c_str(),
                ipStr(cfg.subnetMask).c_str(), ipStr(cfg.serverIp).c_str(),
                ipStr(cfg.dns).c_str(), unsigned(cfg.leaseSeconds));
    std::printf("Waiting for clients on UDP port 67...\n");

    while (true) {
        Dhcp_packet_t packet{};
        sockaddr_in from{};
        socklen_t fromLength = sizeof(from);
        ssize_t n = ::recvfrom(sock, &packet, sizeof(packet), 0,
                               reinterpret_cast<sockaddr*>(&from), &fromLength);
        if (n < 0) {
            std::perror("recvfrom");
            break;
        }
        handle(sock, packet, static_cast<std::size_t>(n));
        std::fflush(stdout);
    }
    close(sock);
    return 0;
}
