#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include "portable_net.h"
#include "arp_packet.h"

using namespace std;

struct Client {
    SOCKET sock;
    unsigned char ip[4];
    unsigned char mac[6];
    bool registered;
};

void sendAll(const vector<Client>& clients, const char* data, int len, SOCKET except = INVALID_SOCKET) {
    for (const auto& c : clients) {
        if (!c.registered) continue;
        if (c.sock == except) continue;
        send(c.sock, data, len, 0);
    }
}

// Like a switch: deliver a unicast frame to the port that owns the destination MAC.
void sendToMac(const vector<Client>& clients, const unsigned char mac[6],
               const char* data, int len) {
    for (const auto& c : clients) {
        if (!c.registered) continue;
        if (memcmp(c.mac, mac, 6) == 0) {
            send(c.sock, data, len, 0);
            return;
        }
    }
}

int main() {
    if (net_init() != 0) {
        cout << "Network init failed\n";
        return 1;
    }

    SOCKET server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server == INVALID_SOCKET) {
        cout << "socket failed\n";
        return 1;
    }
    set_reuse(server);

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(ARP_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(server, (sockaddr*)&addr, sizeof(addr)) == SOCK_ERR) {
        cout << "bind failed (port " << ARP_PORT << " in use?)\n";
        return 1;
    }
    listen(server, 16);

    cout << "=== ARP LAN Hub (Windows / Linux / macOS) ===\n";
    cout << "Port " << ARP_PORT << "\n";
    cout << "Broadcast ARP Request | Unicast ARP Reply\n\n";

    vector<Client> clients;
    fd_set master;
    FD_ZERO(&master);
    FD_SET(server, &master);
    int maxfd = (int)server;

    while (true) {
        fd_set readfds = master;
        timeval tv = { 1, 0 };
#ifdef _WIN32
        int ready = select(0, &readfds, NULL, NULL, &tv);
#else
        int ready = select(maxfd + 1, &readfds, NULL, NULL, &tv);
#endif

        if (ready > 0 && FD_ISSET(server, &readfds)) {
            SOCKET c = accept(server, NULL, NULL);
            if (c != INVALID_SOCKET) {
                FD_SET(c, &master);
#ifndef _WIN32
                if ((int)c > maxfd) maxfd = (int)c;
#endif
                Client cl = {};
                cl.sock = c;
                cl.registered = false;
                clients.push_back(cl);
                cout << "[+] host socket connected\n";
            }
        }

        for (size_t i = 0; i < clients.size(); ) {
            SOCKET s = clients[i].sock;
            if (!(ready > 0 && FD_ISSET(s, &readfds))) { i++; continue; }

            // Unregistered: read the text line "REG <ip> <mac>".
            // Registered:   read exactly one 28-byte ARP packet.
            char buf[512];
            int n = clients[i].registered
                        ? recvAll(s, buf, (int)sizeof(ArpPacket))
                        : recv(s, buf, sizeof(buf) - 1, 0);
            if (n <= 0) {
                if (clients[i].registered)
                    cout << "[-] host " << ipToStr(clients[i].ip) << " left\n";
                FD_CLR(s, &master);
                CLOSESOCK(s);
                clients.erase(clients.begin() + i);
                continue;
            }

            if (!clients[i].registered) {
                if (n >= 7 && strncmp(buf, "REG ", 4) == 0) {
                    string line(buf, n);
                    size_t sp = line.find(' ', 4);
                    if (sp != string::npos) {
                        string ipS = line.substr(4, sp - 4);
                        string macS = line.substr(sp + 1);
                        while (!macS.empty() && (macS.back() == '\n' || macS.back() == '\r' || macS.back() == ' '))
                            macS.pop_back();
                        if (parseIp(ipS.c_str(), clients[i].ip) &&
                            parseMac(macS.c_str(), clients[i].mac)) {
                            clients[i].registered = true;
                            const char* ok = "OK\n";
                            send(s, ok, 3, 0);
                            cout << "Host registered IP=" << ipToStr(clients[i].ip)
                                 << " MAC=" << macToStr(clients[i].mac) << "\n";
                        }
                    }
                }
                i++;
                continue;
            }

            {   // A registered host sent one ARP packet.
                ArpPacket pkt;
                memcpy(&pkt, buf, sizeof(pkt));
                unsigned short oper = ntohs(pkt.oper);
                if (oper == ARP_REQUEST) {
                    cout << "ARP REQUEST who-has " << ipToStr(pkt.tpa)
                         << "  tell " << ipToStr(pkt.spa)
                         << " (" << macToStr(pkt.sha) << ")  [BROADCAST]"
                         << (memcmp(pkt.spa, pkt.tpa, 4) == 0 ? " (gratuitous)" : "") << "\n";
                    sendAll(clients, buf, (int)sizeof(ArpPacket), s);
                } else if (oper == ARP_REPLY) {
                    cout << "ARP REPLY " << ipToStr(pkt.spa) << " is-at "
                         << macToStr(pkt.sha) << "  -> " << macToStr(pkt.tha)
                         << "  [UNICAST]\n";
                    sendToMac(clients, pkt.tha, buf, (int)sizeof(ArpPacket));
                }
            }
            i++;
        }
    }

    CLOSESOCK(server);
    net_cleanup();
    return 0;
}
