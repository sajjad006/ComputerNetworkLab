// DHCP client: DISCOVER -> OFFER -> REQUEST -> ACK (RFC 2131).
// It only obtains a lease and prints it; it does not change the OS network settings.
//
// Build:  g++ -std=c++17 -o dhcp_client Dhcp.cpp                       (Linux / macOS)
//         g++ -std=c++17 -o dhcp_client Dhcp.cpp -lws2_32 -liphlpapi   (Windows MinGW)
// Run:    sudo ./dhcp_client [interface] [MAC] [--apply]
//         --apply (Linux only): put the leased IP and default route on the interface.

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <cerrno>
#include <chrono>
#include <random>
#include <string>
#include <vector>
#include <cstdlib>
#include <cctype>

// ---------------------------------------------------------------- OS headers
#ifdef _WIN32
    #ifndef _WIN32_WINNT
        #define _WIN32_WINNT 0x0600
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <iphlpapi.h>
    #ifdef _MSC_VER
        #pragma comment(lib, "Ws2_32.lib")
        #pragma comment(lib, "Iphlpapi.lib")
    #endif
    using SocketHandle = SOCKET;
    using SocketLength = int;
    constexpr SocketHandle INVALID_SOCKET_HANDLE = INVALID_SOCKET;
#else
    #include <sys/socket.h>
    #include <sys/time.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <ifaddrs.h>
    #include <net/if.h>
    #ifdef __APPLE__
        #include <net/if_dl.h>
    #elif defined(__linux__)
        #include <netpacket/packet.h>
    #endif
    using SocketHandle = int;
    using SocketLength = socklen_t;
    constexpr SocketHandle INVALID_SOCKET_HANDLE = -1;
#endif

#include "dhcp_packet.h"

// State kept across the exchange.
struct ClientContext {
    uint8_t  mac[6]{};
    uint32_t transactionId{};  // host byte order
    uint32_t offeredIp{};      // network byte order, from OFFER.yiaddr
    uint32_t serverIp{};       // network byte order, from OFFER option 54
};

// Values extracted from an OFFER / ACK / NAK. IPs are in network byte order.
struct ParsedReply {
    uint8_t  messageType{};
    uint32_t offeredIp{};
    uint32_t serverIp{};
    uint32_t leaseSeconds{};   // host byte order
    uint32_t subnetMask{};
    uint32_t router{};
    uint32_t dns{};
};

// ---------------------------------------------------------------- Small helpers
void printSocketError(const char* operation) {
#ifdef _WIN32
    std::fprintf(stderr, "%s failed: %d\n", operation, WSAGetLastError());
#else
    int error = errno;
    std::perror(operation);
    #ifdef __APPLE__
    if (error == EHOSTUNREACH || error == EACCES)
        std::fprintf(stderr, "Check System Settings > Privacy & Security > Local Network "
                             "for this terminal.\n");
    #endif
    (void)error;
#endif
}

void closeSocket(SocketHandle sock) {
#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif
}

void printMac(const uint8_t* mac) {
    std::printf("%02x:%02x:%02x:%02x:%02x:%02x",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

const char* ipText(uint32_t networkOrderIp) {
    in_addr address{};
    address.s_addr = networkOrderIp;
    return inet_ntoa(address);   // static buffer: print before the next call
}

// "aa:bb:cc:dd:ee:ff" -> 6 bytes. Rejects zero, multicast and the macOS placeholder.
bool parseMac(const char* text, uint8_t mac[6]) {
    unsigned v[6];
    int used = 0;
    if (std::sscanf(text, "%2x:%2x:%2x:%2x:%2x:%2x%n",
                    &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &used) != 6 ||
        used != 17 || text[used] != '\0') return false;
    for (int i = 0; i < 6; ++i) mac[i] = static_cast<uint8_t>(v[i]);
    const uint8_t zero[6]{}, placeholder[6] = {2, 0, 0, 0, 0, 0};
    return !(mac[0] & 1) && std::memcmp(mac, zero, 6) && std::memcmp(mac, placeholder, 6);
}

// ---------------------------------------------------------------- Network interfaces
struct ClientInterface {
    std::string name;        // name the user types (e.g. en0, wlan0, "Wi-Fi")
    std::string systemName;  // name the OS APIs need
    uint8_t mac[6]{};
    unsigned index{};
    bool macHidden{};        // macOS can report 02:00:00:00:00:00 instead of the real MAC
};

// List active, non-loopback interfaces that have a 6-byte MAC.
std::vector<ClientInterface> listInterfaces() {
    std::vector<ClientInterface> list;
#ifdef _WIN32
    std::vector<unsigned char> storage(15000);
    ULONG size = static_cast<ULONG>(storage.size());
    auto* adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(storage.data());
    ULONG result = GetAdaptersAddresses(AF_INET, 0, nullptr, adapters, &size);
    if (result == ERROR_BUFFER_OVERFLOW) {
        storage.resize(size);
        adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(storage.data());
        result = GetAdaptersAddresses(AF_INET, 0, nullptr, adapters, &size);
    }
    if (result != NO_ERROR) {
        std::fprintf(stderr, "GetAdaptersAddresses failed: %lu\n", result);
        return list;
    }
    for (auto* a = adapters; a; a = a->Next) {
        if (a->OperStatus != IfOperStatusUp || a->PhysicalAddressLength != 6 || !a->IfIndex)
            continue;
        char friendly[512]{};
        if (a->FriendlyName)
            WideCharToMultiByte(CP_UTF8, 0, a->FriendlyName, -1, friendly, sizeof(friendly),
                                nullptr, nullptr);
        ClientInterface ci;
        ci.systemName = a->AdapterName;
        ci.name = friendly[0] ? friendly : a->AdapterName;
        ci.index = a->IfIndex;
        std::memcpy(ci.mac, a->PhysicalAddress, 6);
        list.push_back(ci);
    }
#else
    ifaddrs* addresses = nullptr;
    if (getifaddrs(&addresses) != 0) {
        std::perror("getifaddrs");
        return list;
    }
    for (auto* e = addresses; e; e = e->ifa_next) {
        if (!e->ifa_addr || !(e->ifa_flags & IFF_UP) || (e->ifa_flags & IFF_LOOPBACK)) continue;
        const void* mac = nullptr;
    #ifdef __APPLE__
        if (e->ifa_addr->sa_family == AF_LINK) {
            auto* link = reinterpret_cast<const sockaddr_dl*>(e->ifa_addr);
            if (link->sdl_alen == 6) mac = LLADDR(link);
        }
    #elif defined(__linux__)
        if (e->ifa_addr->sa_family == AF_PACKET) {
            auto* link = reinterpret_cast<const sockaddr_ll*>(e->ifa_addr);
            if (link->sll_halen == 6) mac = link->sll_addr;
        }
    #endif
        if (!mac) continue;
        ClientInterface ci;
        ci.name = ci.systemName = e->ifa_name;
        ci.index = if_nametoindex(e->ifa_name);
        std::memcpy(ci.mac, mac, 6);
        const uint8_t placeholder[6] = {2, 0, 0, 0, 0, 0};
        ci.macHidden = std::memcmp(ci.mac, placeholder, 6) == 0;
        if (ci.index != 0) list.push_back(ci);
    }
    freeifaddrs(addresses);
#endif
    return list;
}

// ---------------------------------------------------------------- Socket
// UDP socket: broadcast allowed, tied to one interface, 1 s receive timeout, bound to port 68.
SocketHandle createClientSocket(const ClientInterface& iface) {
    SocketHandle sock = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET_HANDLE) {
        printSocketError("socket");
        return INVALID_SOCKET_HANDLE;
    }

    int on = 1;
#ifdef _WIN32
    DWORD timeout = 1000;  // ms
#else
    timeval timeout{1, 0};
#endif
    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_port = htons(CLIENT_PORT);
    local.sin_addr.s_addr = htonl(INADDR_ANY);

    bool ok = ::setsockopt(sock, SOL_SOCKET, SO_BROADCAST,
                           reinterpret_cast<const char*>(&on), sizeof(on)) == 0;
    if (!ok) printSocketError("SO_BROADCAST");

    // Make the broadcast leave through the chosen interface.
#ifdef __APPLE__
    if (ok && !(ok = ::setsockopt(sock, IPPROTO_IP, IP_BOUND_IF,
                                  &iface.index, sizeof(iface.index)) == 0))
        printSocketError("IP_BOUND_IF");
#elif defined(__linux__)
    if (ok && !(ok = ::setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, iface.systemName.c_str(),
                                  static_cast<socklen_t>(iface.systemName.size() + 1)) == 0))
        printSocketError("SO_BINDTODEVICE");
#else
    (void)iface;  // Windows picks the adapter per packet in sendPacket (IP_PKTINFO).
#endif

    if (ok && !(ok = ::setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,
                                  reinterpret_cast<const char*>(&timeout), sizeof(timeout)) == 0))
        printSocketError("SO_RCVTIMEO");
    if (ok && !(ok = ::bind(sock, reinterpret_cast<const sockaddr*>(&local), sizeof(local)) == 0))
        printSocketError("bind port 68 (run with sudo; stop any other DHCP client)");

    if (!ok) {
        closeSocket(sock);
        return INVALID_SOCKET_HANDLE;
    }
    return sock;
}

// Broadcast a packet to 255.255.255.255:67.
bool sendPacket(SocketHandle sock, const Dhcp_packet_t& packet, std::size_t length,
                unsigned interfaceIndex) {
    sockaddr_in to{};
    to.sin_family = AF_INET;
    to.sin_port = htons(SERVER_PORT);
    to.sin_addr.s_addr = htonl(INADDR_BROADCAST);

#ifdef _WIN32
    // Windows has no SO_BINDTODEVICE: attach IP_PKTINFO to choose the outgoing adapter.
    WSABUF buffer{};
    buffer.buf = const_cast<char*>(reinterpret_cast<const char*>(&packet));
    buffer.len = static_cast<ULONG>(length);
    alignas(WSACMSGHDR) char control[WSA_CMSG_SPACE(sizeof(IN_PKTINFO))]{};
    WSAMSG message{};
    message.name = reinterpret_cast<sockaddr*>(&to);
    message.namelen = sizeof(to);
    message.lpBuffers = &buffer;
    message.dwBufferCount = 1;
    message.Control.buf = control;
    message.Control.len = sizeof(control);
    auto* header = WSA_CMSG_FIRSTHDR(&message);
    header->cmsg_level = IPPROTO_IP;
    header->cmsg_type = IP_PKTINFO;
    header->cmsg_len = WSA_CMSG_LEN(sizeof(IN_PKTINFO));
    reinterpret_cast<IN_PKTINFO*>(WSA_CMSG_DATA(header))->ipi_ifindex = interfaceIndex;
    DWORD sent = 0;
    if (WSASendMsg(sock, &message, 0, &sent, nullptr, nullptr) != 0) {
        printSocketError("WSASendMsg");
        return false;
    }
#else
    (void)interfaceIndex;  // the socket is already tied to the interface
    long sent = ::sendto(sock, &packet, length, 0, reinterpret_cast<const sockaddr*>(&to),
                         sizeof(to));
    if (sent < 0) {
        printSocketError("sendto");
        return false;
    }
#endif
    return static_cast<std::size_t>(sent) == length;
}

// Returns bytes received, 0 if nothing usable arrived within the timeout, -1 on error.
int receivePacket(SocketHandle sock, Dhcp_packet_t& packet) {
    char buffer[2048];
    sockaddr_in from{};
    SocketLength fromLength = sizeof(from);
    int n = static_cast<int>(::recvfrom(sock, buffer, sizeof(buffer), 0,
                                        reinterpret_cast<sockaddr*>(&from), &fromLength));
    if (n < 0) {
#ifdef _WIN32
        int error = WSAGetLastError();
        if (error == WSAETIMEDOUT || error == WSAEWOULDBLOCK || error == WSAEMSGSIZE) return 0;
#else
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
#endif
        printSocketError("recvfrom");
        return -1;
    }
    // Only accept datagrams from a server port that fit our structure.
    if (ntohs(from.sin_port) != SERVER_PORT || n < static_cast<int>(HEADER_LEN) ||
        n > static_cast<int>(sizeof(packet))) return 0;

    packet = {};
    std::memcpy(&packet, buffer, n);
    std::printf("Received %d bytes from %s:%u\n", n, inet_ntoa(from.sin_addr),
                unsigned(ntohs(from.sin_port)));
    return n;
}

// ---------------------------------------------------------------- Build packets
// Builds DISCOVER or REQUEST. Returns the number of bytes to send.
std::size_t buildPacket(Dhcp_packet_t& p, uint8_t messageType, const ClientContext& client) {
    p = {};
    p.op = BOOTREQUEST;
    p.htype = 1;                       // Ethernet
    p.hlen = 6;                        // MAC length
    p.xid = htonl(client.transactionId);
    p.flags = htons(0x8000);           // ask the server to broadcast its reply
    std::memcpy(p.chaddr, client.mac, 6);
    p.magic_cookie = htonl(DHCP_MAGIC_COOKIE);

    std::size_t i = 0;
    addOption(p, i, OPT_MSG_TYPE, &messageType, 1);

    if (messageType == DHCPREQUEST) {
        // Tell every server which offer we picked (option 54) and which IP we want (50).
        addOption(p, i, OPT_REQUESTED_IP, &client.offeredIp, 4);
        addOption(p, i, OPT_SERVER_ID, &client.serverIp, 4);
    }

    const uint8_t wanted[] = {OPT_SUBNET_MASK, OPT_ROUTER, OPT_DNS_SERVER,
                              OPT_LEASE_TIME, OPT_SERVER_ID};
    addOption(p, i, OPT_PARAM_LIST, wanted, sizeof(wanted));

    uint8_t clientId[7] = {1};         // type 1 (Ethernet) + MAC
    std::memcpy(clientId + 1, client.mac, 6);
    addOption(p, i, OPT_CLIENT_ID, clientId, sizeof(clientId));

    return finishOptions(p, i);
}

// ---------------------------------------------------------------- Parse replies
// Check the header belongs to us, parse the options, and pick out what we need.
bool parseReply(const Dhcp_packet_t& p, std::size_t length, const ClientContext& client,
                ParsedReply& reply) {
    DhcpOptions o;
    if (p.op != BOOTREPLY || p.htype != 1 || p.hlen != 6 ||
        ntohl(p.xid) != client.transactionId ||
        std::memcmp(p.chaddr, client.mac, 6) != 0 ||
        !parseOptions(p, length, o)) return false;

    ParsedReply r{};
    r.messageType = o.byte(OPT_MSG_TYPE);
    r.offeredIp   = p.yiaddr;
    r.serverIp    = o.ip(OPT_SERVER_ID);
    r.subnetMask  = o.ip(OPT_SUBNET_MASK);
    r.router      = o.ip(OPT_ROUTER);         // first router if several
    r.dns         = o.ip(OPT_DNS_SERVER);     // first DNS server if several
    if (o.len[OPT_LEASE_TIME] == 4) r.leaseSeconds = ntohl(o.ip(OPT_LEASE_TIME));

    // OFFER/ACK must carry a server ID, an address and a lease; NAK only a server ID.
    if (r.serverIp == 0) return false;
    if (r.messageType == DHCPOFFER || r.messageType == DHCPACK) {
        if (r.offeredIp == 0 || r.leaseSeconds == 0) return false;
    } else if (r.messageType != DHCPNAK) {
        return false;
    }
    reply = r;
    return true;
}

void printReply(const ParsedReply& r) {
    std::printf("---- DHCP%s ----\n", messageName(r.messageType));
    std::printf("  Server ID   : %s\n", ipText(r.serverIp));
    std::printf("  Your IP     : %s\n", ipText(r.offeredIp));
    if (r.subnetMask) std::printf("  Subnet mask : %s\n", ipText(r.subnetMask));
    if (r.router)     std::printf("  Router      : %s\n", ipText(r.router));
    if (r.dns)        std::printf("  DNS server  : %s\n", ipText(r.dns));
    std::printf("  Lease       : %u seconds\n", unsigned(r.leaseSeconds));
}

// ---------------------------------------------------------------- One request/reply step
// Send `packet`, wait up to 4 s for the wanted reply, retransmit up to 3 times.
// When waiting for ACK, a NAK from the chosen server also ends the wait.
bool exchange(SocketHandle sock, const Dhcp_packet_t& packet, std::size_t length,
              unsigned ifIndex, const ClientContext& client, uint8_t wantedType,
              ParsedReply& reply) {
    using Clock = std::chrono::steady_clock;
    for (int attempt = 1; attempt <= 3; ++attempt) {
        if (!sendPacket(sock, packet, length, ifIndex)) return false;
        std::printf("Sent DHCP%s (%zu bytes, XID 0x%08x), attempt %d/3\n",
                    wantedType == DHCPOFFER ? "DISCOVER" : "REQUEST", length,
                    unsigned(client.transactionId), attempt);

        auto deadline = Clock::now() + std::chrono::seconds(4);
        while (Clock::now() < deadline) {
            Dhcp_packet_t response{};
            int n = receivePacket(sock, response);
            if (n < 0) return false;
            if (n == 0) continue;
            if (!parseReply(response, n, client, reply)) {
                std::printf("  ignored: not a valid reply to our XID/MAC\n");
                continue;
            }
            if (wantedType == DHCPOFFER && reply.messageType == DHCPOFFER) return true;
            if (wantedType == DHCPACK && reply.serverIp == client.serverIp) {
                if (reply.messageType == DHCPNAK) return true;
                if (reply.messageType == DHCPACK && reply.offeredIp == client.offeredIp)
                    return true;
            }
            std::printf("  ignored: message type %u\n", unsigned(reply.messageType));
        }
        std::printf("  no reply, retrying\n");
    }
    return false;
}

// ---------------------------------------------------------------- Apply the lease (Linux)
// A real DHCP client finishes by configuring the interface. We do it with the `ip` tool:
//   ip addr flush dev X; ip addr add IP/prefix dev X; ip route replace default via R
bool applyLease(const std::string& ifname, const ParsedReply& r) {
#ifdef __linux__
    for (char c : ifname)   // the name goes into a shell command: allow only safe characters
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '_' && c != '.')
            return false;
    int prefix = 0;
    for (uint32_t m = ntohl(r.subnetMask); m & 0x80000000u; m <<= 1) ++prefix;
    std::string ip = ipText(r.offeredIp), router = r.router ? ipText(r.router) : "";
    std::string cmd = "ip addr flush dev " + ifname + " && ip addr add " + ip + "/" +
                      std::to_string(prefix ? prefix : 24) + " dev " + ifname;
    if (!router.empty()) cmd += " && ip route replace default via " + router + " dev " + ifname;
    std::printf("Applying: %s\n", cmd.c_str());
    return std::system(cmd.c_str()) == 0;
#else
    (void)ifname; (void)r;
    std::fprintf(stderr, "--apply is only supported on Linux.\n");
    return false;
#endif
}

// ---------------------------------------------------------------- main
int main(int argc, char* argv[]) {
    std::vector<std::string> args;
    bool apply = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--apply") == 0) apply = true;
        else args.push_back(argv[i]);
    }
    std::string name = args.size() > 0 ? args[0] : "";
    std::string macText = args.size() > 1 ? args[1] : "";
    if (args.size() > 2 || name == "--help" || name == "-h") {
        std::printf("Usage: sudo %s [interface] [MAC] [--apply]\n"
                    "  interface: en0 (macOS), wlan0/eth0 (Linux), \"Wi-Fi\" (Windows)\n"
                    "  MAC:       only needed when macOS hides the interface's MAC\n"
                    "  --apply:   Linux only, configure the interface with the leased IP\n",
                    argv[0]);
        return args.size() > 2;
    }

#ifdef _WIN32
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::fprintf(stderr, "WSAStartup failed\n");
        return 1;
    }
#endif

    // 1. Pick the interface (ask if not given on the command line).
    auto interfaces = listInterfaces();
    if (name.empty()) {
        std::printf("Available interfaces:");
        for (const auto& ci : interfaces) std::printf(" %s", ci.name.c_str());
        std::printf("\nNetwork interface: ");
        char line[512]{};
        if (!std::fgets(line, sizeof(line), stdin)) return 1;
        line[std::strcspn(line, "\r\n")] = '\0';
        name = line;
    }
    const ClientInterface* found = nullptr;
    for (const auto& ci : interfaces)
        if (ci.name == name || ci.systemName == name) found = &ci;
    if (!found) {
        std::fprintf(stderr, "Interface '%s' not found or not up.\n", name.c_str());
        return 1;
    }
    ClientInterface iface = *found;

    // 2. Pick the MAC. macOS may hide it, then the user supplies it (see: ifconfig <name>).
    if (macText.empty() && iface.macHidden) {
        std::printf("macOS hides the MAC of %s. Enter it (from ifconfig %s): ",
                    name.c_str(), name.c_str());
        char line[64]{};
        if (!std::fgets(line, sizeof(line), stdin)) return 1;
        line[std::strcspn(line, "\r\n")] = '\0';
        macText = line;
    }
    if (!macText.empty() && !parseMac(macText.c_str(), iface.mac)) {
        std::fprintf(stderr, "Invalid MAC '%s' (use aa:bb:cc:dd:ee:ff, unicast).\n",
                     macText.c_str());
        return 1;
    }
    std::printf("Using interface %s, MAC ", iface.name.c_str());
    printMac(iface.mac);
    std::printf("\n");

    ClientContext client{};
    std::memcpy(client.mac, iface.mac, 6);
    std::random_device rd;
    client.transactionId = std::uniform_int_distribution<uint32_t>(1, UINT32_MAX)(rd);

    SocketHandle sock = createClientSocket(iface);
    if (sock == INVALID_SOCKET_HANDLE) return 1;

    bool leased = false;
    Dhcp_packet_t packet{};
    ParsedReply reply{};

    // 3. DISCOVER -> OFFER
    std::size_t length = buildPacket(packet, DHCPDISCOVER, client);
    if (exchange(sock, packet, length, iface.index, client, DHCPOFFER, reply)) {
        printReply(reply);
        client.offeredIp = reply.offeredIp;
        client.serverIp = reply.serverIp;

        // 4. REQUEST -> ACK / NAK (same XID)
        length = buildPacket(packet, DHCPREQUEST, client);
        if (exchange(sock, packet, length, iface.index, client, DHCPACK, reply)) {
            printReply(reply);
            leased = reply.messageType == DHCPACK;
            if (!leased) std::fprintf(stderr, "Server sent NAK: request rejected.\n");
        } else {
            std::fprintf(stderr, "No ACK/NAK received.\n");
        }
    } else {
        std::fprintf(stderr, "No OFFER received.\n");
    }

    if (leased && apply) {
        if (!applyLease(iface.systemName, reply)) {
            std::fprintf(stderr, "Could not apply the lease to %s.\n", iface.systemName.c_str());
            leased = false;
        } else {
            std::printf("Interface %s is now configured.\n", iface.systemName.c_str());
        }
    } else if (leased) {
        std::printf("Lease acknowledged. (Run with --apply to configure the interface.)\n");
    }
    closeSocket(sock);
#ifdef _WIN32
    WSACleanup();
#endif
    return leased ? 0 : 1;
}
