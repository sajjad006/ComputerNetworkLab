#include <iostream>
#include <string>
#include <map>
#include <cstring>
#include <ctime>
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>
#include "portable_net.h"
#include "arp_packet.h"

using namespace std;
using namespace std::chrono;

struct CacheEntry {
    unsigned char mac[6];
    time_t expires;
};

mutex cacheMu;
mutex sendMu;   // main thread and receiver thread both send on gSock
SOCKET gSock = INVALID_SOCKET;
unsigned char gMyIp[4], gMyMac[6];
atomic<bool> running{true};

class ArpCache {
    map<string, CacheEntry> table;
    int timeoutSec;
public:
    explicit ArpCache(int t = 60) : timeoutSec(t) {}

    bool lookup(const unsigned char ip[4], unsigned char macOut[6]) {
        lock_guard<mutex> lk(cacheMu);
        string k = ipToStr(ip);
        auto it = table.find(k);
        if (it == table.end()) return false;
        if (time(nullptr) > it->second.expires) {
            table.erase(it);
            return false;
        }
        setMac(macOut, it->second.mac);
        return true;
    }

    // RFC 826 "merge": if we already know the sender, refresh its MAC.
    bool refreshIfPresent(const unsigned char ip[4], const unsigned char mac[6]) {
        {
            lock_guard<mutex> lk(cacheMu);
            if (table.find(ipToStr(ip)) == table.end()) return false;
        }
        update(ip, mac);
        return true;
    }

    void update(const unsigned char ip[4], const unsigned char mac[6]) {
        lock_guard<mutex> lk(cacheMu);
        CacheEntry e;
        setMac(e.mac, mac);
        e.expires = time(nullptr) + timeoutSec;
        table[ipToStr(ip)] = e;
        cout << "  [cache] " << ipToStr(ip) << " -> " << macToStr(mac)
             << "  (ttl " << timeoutSec << "s)\n";
    }

    void show() {
        lock_guard<mutex> lk(cacheMu);
        cout << "  --- ARP Cache ---\n";
        if (table.empty()) { cout << "  (empty)\n"; return; }
        time_t now = time(nullptr);
        for (const auto& kv : table) {
            if (kv.second.expires < now) continue;   // expired, removed on next lookup
            cout << "  " << kv.first << "  " << macToStr(kv.second.mac)
                 << "  remaining=" << (kv.second.expires - now) << "s\n";
        }
    }
};

ArpCache cache(60);

void sendPacket(const ArpPacket& p) {
    lock_guard<mutex> lk(sendMu);
    send(gSock, (const char*)&p, sizeof(p), 0);
}

// Receive logic from RFC 826:
//   1. If someone claims MY IP with another MAC -> IP conflict.
//   2. If the sender is already in my cache -> refresh it (this is how
//      gratuitous ARP updates other hosts).
//   3. If the packet is a request for MY IP -> add sender to cache, send a reply.
//   4. If it is a reply -> add the sender to the cache.
void handleIncoming(const ArpPacket& pkt) {
    unsigned short oper = ntohs(pkt.oper);
    bool gratuitous = (oper == ARP_REQUEST && memcmp(pkt.spa, pkt.tpa, 4) == 0);

    if (memcmp(pkt.spa, gMyIp, 4) == 0 && memcmp(pkt.sha, gMyMac, 6) != 0) {
        cout << "  [WARNING] IP conflict: " << macToStr(pkt.sha)
             << " also claims " << ipToStr(gMyIp) << "\n";
        return;
    }

    if (gratuitous) {
        cout << "  [event] Gratuitous ARP: " << ipToStr(pkt.spa)
             << " is-at " << macToStr(pkt.sha) << "\n";
        if (!cache.refreshIfPresent(pkt.spa, pkt.sha))
            cout << "  (not in my cache, nothing to update)\n";
        return;
    }

    if (oper == ARP_REQUEST) {
        if (memcmp(pkt.tpa, gMyIp, 4) != 0) {
            cache.refreshIfPresent(pkt.spa, pkt.sha);   // not for me, only merge
            return;
        }
        cout << "  [event] ARP REQUEST who-has ME from " << ipToStr(pkt.spa)
             << " (" << macToStr(pkt.sha) << ")\n";
        cache.update(pkt.spa, pkt.sha);   // the asker will talk to us next
        ArpPacket rep;
        makeReply(rep, gMyMac, gMyIp, pkt.sha, pkt.spa);
        sendPacket(rep);
        cout << "  [event] ARP REPLY sent: " << ipToStr(gMyIp)
             << " is-at " << macToStr(gMyMac) << "\n";
    } else if (oper == ARP_REPLY) {
        cout << "  [event] ARP REPLY " << ipToStr(pkt.spa)
             << " is-at " << macToStr(pkt.sha) << "\n";
        cache.update(pkt.spa, pkt.sha);
    }
}

void receiverThread() {
    while (running) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(gSock, &fds);
        timeval tv = { 0, 200000 };
#ifdef _WIN32
        int r = select(0, &fds, NULL, NULL, &tv);
#else
        int r = select((int)gSock + 1, &fds, NULL, NULL, &tv);
#endif
        if (r > 0) {
            ArpPacket pkt;
            int n = recvAll(gSock, (char*)&pkt, sizeof(pkt));
            if (n <= 0) { running = false; break; }
            handleIncoming(pkt);
        }
    }
}

bool resolve(const unsigned char targetIp[4]) {
    unsigned char mac[6];
    if (cache.lookup(targetIp, mac)) {
        cout << "  Resolved (cache hit): " << ipToStr(targetIp)
             << " -> " << macToStr(mac) << "\n";
        return true;
    }

    cout << "  Cache miss for " << ipToStr(targetIp) << "\n";
    cout << "  Sending ARP REQUEST (broadcast): who-has " << ipToStr(targetIp) << "?\n";

    ArpPacket req;
    makeRequest(req, gMyMac, gMyIp, targetIp);
    sendPacket(req);

    auto deadline = steady_clock::now() + seconds(3);
    while (steady_clock::now() < deadline) {
        if (cache.lookup(targetIp, mac)) {
            cout << "  Resolved (after reply): " << ipToStr(targetIp)
                 << " -> " << macToStr(mac) << "\n";
            return true;
        }
        sleep_ms(50);
    }
    cout << "  ARP timeout — no reply\n";
    return false;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        cout << "Usage: host <myIP> <myMAC> [peerIP]\n";
        cout << "  ./host 192.168.1.1 02:00:00:00:00:01\n";
        cout << "  ./host 192.168.1.2 02:00:00:00:00:02 192.168.1.1\n";
        cout << "Commands: resolve <ip> | cache | grat | quit\n";
        return 1;
    }
    if (!parseIp(argv[1], gMyIp) || !parseMac(argv[2], gMyMac)) {
        cout << "Bad IP or MAC\n";
        return 1;
    }

    unsigned char peerIp[4];
    bool hasPeer = (argc >= 4 && parseIp(argv[3], peerIp));

    if (net_init() != 0) {
        cout << "Network init failed\n";
        return 1;
    }

    gSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (gSock == INVALID_SOCKET) {
        cout << "socket failed\n";
        return 1;
    }
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(ARP_PORT);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    cout << "Connecting to LAN hub on port " << ARP_PORT << "...\n";
    if (connect(gSock, (sockaddr*)&addr, sizeof(addr)) == SOCK_ERR) {
        cout << "Connect failed. Start lan_hub first!\n";
        return 1;
    }

    string reg = string("REG ") + argv[1] + " " + argv[2] + "\n";
    send(gSock, reg.c_str(), (int)reg.size(), 0);
    char okbuf[32];
    recv(gSock, okbuf, sizeof(okbuf), 0);

    cout << "Host UP  IP=" << ipToStr(gMyIp) << "  MAC=" << macToStr(gMyMac) << "\n";

    thread rx(receiverThread);

    if (hasPeer) {
        sleep_ms(800);
        cout << "\n=== Resolve peer " << ipToStr(peerIp) << " ===\n";
        resolve(peerIp);
        cache.show();
    }

    cout << "\nCommands: resolve <ip> | cache | grat | quit\n";
    string line;
    while (running && getline(cin, line)) {
        if (line.empty()) continue;
        if (line == "quit" || line == "exit") break;
        if (line == "cache") { cache.show(); continue; }
        if (line == "grat") {
            ArpPacket g;
            makeGratuitous(g, gMyMac, gMyIp);
            sendPacket(g);
            cout << "  Gratuitous ARP announced for " << ipToStr(gMyIp) << "\n";
            continue;
        }
        if (line.rfind("resolve ", 0) == 0) {
            unsigned char tip[4];
            if (!parseIp(line.c_str() + 8, tip)) { cout << "  Bad IP\n"; continue; }
            resolve(tip);
            continue;
        }
        cout << "  Unknown. Use: resolve <ip> | cache | grat | quit\n";
    }

    running = false;
    if (rx.joinable()) rx.join();
    CLOSESOCK(gSock);
    net_cleanup();
    cout << "Host offline.\n";
    return 0;
}
