#ifndef SOCKET_UTIL_H
#define SOCKET_UTIL_H

// Linux/POSIX UDP sockets, loopback only.
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <string>
using namespace std;

// One port pair per protocol so the three demos never clash.
const int SW_SENDER_PORT  = 9001, SW_RECEIVER_PORT  = 9002;
const int GBN_SENDER_PORT = 9101, GBN_RECEIVER_PORT = 9102;
const int SR_SENDER_PORT  = 9201, SR_RECEIVER_PORT  = 9202;

// 4-byte magic + 4-byte frame count: sent a few times, unimpaired, so the
// receiver learns how many unique frames make up the transfer before the
// (lossy/delayed/corrupting) data channel simulation kicks in.
const char SETUP_MAGIC[4] = {'S','E','T','1'};

inline int makeUdpSocket(int bindPort) {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(bindPort);
    bind(fd, (sockaddr*)&addr, sizeof(addr));
    return fd;
}

inline sockaddr_in loopbackAddr(int port) {
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
    return a;
}

inline void setRecvTimeoutMs(int fd, double ms) {
    if (ms < 1) ms = 1;
    timeval tv{};
    tv.tv_sec  = (long)(ms / 1000);
    tv.tv_usec = (long)((ms - tv.tv_sec * 1000) * 1000);
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
}

inline void sendSetup(int fd, const sockaddr_in& to, unsigned int totalFrames) {
    // Sent once, unimpaired: loopback UDP between two local processes is an
    // in-kernel copy that doesn't drop, so no retry is needed here. This
    // bootstrap step is not part of the flow-control mechanism under test.
    string msg(SETUP_MAGIC, 4);
    for (int i = 3; i >= 0; i--) msg.push_back((char)((totalFrames >> (i * 8)) & 0xFF));
    sendto(fd, msg.data(), msg.size(), 0, (const sockaddr*)&to, sizeof(to));
}

// Blocks (no timeout) until a valid SETUP datagram is received; returns frame count.
inline unsigned int recvSetup(int fd) {
    char buf[16];
    while (true) {
        sockaddr_in from{}; socklen_t flen = sizeof(from);
        ssize_t n = recvfrom(fd, buf, sizeof(buf), 0, (sockaddr*)&from, &flen);
        if (n == 8 && memcmp(buf, SETUP_MAGIC, 4) == 0) {
            unsigned int total = 0;
            for (int i = 0; i < 4; i++) total = (total << 8) | (unsigned char)buf[4 + i];
            return total;
        }
    }
}

#endif
