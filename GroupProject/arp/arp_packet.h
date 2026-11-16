#ifndef ARP_PACKET_H
#define ARP_PACKET_H

#include <cstring>
#include <string>
#include <cstdio>
#include "portable_net.h"  // htons / ntohs

// ARP packet for Ethernet + IPv4 (RFC 826): 28 bytes, same layout as on a real wire.
// 16-bit fields are stored in network byte order (big-endian): use htons/ntohs.
#pragma pack(push, 1)
struct ArpPacket {
    unsigned short htype;   // 1 = Ethernet
    unsigned short ptype;   // 0x0800 = IPv4
    unsigned char  hlen;    // 6
    unsigned char  plen;    // 4
    unsigned short oper;    // 1 = request, 2 = reply
    unsigned char  sha[6];  // sender MAC
    unsigned char  spa[4];  // sender IP
    unsigned char  tha[6];  // target MAC
    unsigned char  tpa[4];  // target IP
};
#pragma pack(pop)
static_assert(sizeof(ArpPacket) == 28, "ARP packet must be 28 bytes");

const unsigned short ARP_REQUEST = 1;
const unsigned short ARP_REPLY   = 2;
const int ARP_PORT = 9200;

inline void setMac(unsigned char* dst, const unsigned char* src) {
    memcpy(dst, src, 6);
}
inline void setIp(unsigned char* dst, const unsigned char* src) {
    memcpy(dst, src, 4);
}
inline void zeroMac(unsigned char* m) { memset(m, 0, 6); }

inline std::string macToStr(const unsigned char m[6]) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
             m[0], m[1], m[2], m[3], m[4], m[5]);
    return buf;
}

inline std::string ipToStr(const unsigned char ip[4]) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
    return buf;
}

// Parse "AA:BB:CC:DD:EE:FF"
inline bool parseMac(const char* s, unsigned char out[6]) {
    unsigned int v[6];
    if (sscanf(s, "%x:%x:%x:%x:%x:%x", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) != 6)
        return false;
    for (int i = 0; i < 6; i++) out[i] = (unsigned char)v[i];
    return true;
}

// Parse "192.168.1.10"
inline bool parseIp(const char* s, unsigned char out[4]) {
    unsigned int v[4];
    if (sscanf(s, "%u.%u.%u.%u", &v[0], &v[1], &v[2], &v[3]) != 4)
        return false;
    for (int i = 0; i < 4; i++) out[i] = (unsigned char)v[i];
    return true;
}

inline void makeRequest(ArpPacket& p,
                        const unsigned char sha[6], const unsigned char spa[4],
                        const unsigned char tpa[4]) {
    memset(&p, 0, sizeof(p));
    p.htype = htons(1);
    p.ptype = htons(0x0800);
    p.hlen = 6;
    p.plen = 4;
    p.oper = htons(ARP_REQUEST);
    setMac(p.sha, sha);
    setIp(p.spa, spa);
    zeroMac(p.tha);
    setIp(p.tpa, tpa);
}

inline void makeReply(ArpPacket& p,
                      const unsigned char sha[6], const unsigned char spa[4],
                      const unsigned char tha[6], const unsigned char tpa[4]) {
    memset(&p, 0, sizeof(p));
    p.htype = htons(1);
    p.ptype = htons(0x0800);
    p.hlen = 6;
    p.plen = 4;
    p.oper = htons(ARP_REPLY);
    setMac(p.sha, sha);
    setIp(p.spa, spa);
    setMac(p.tha, tha);
    setIp(p.tpa, tpa);
}

inline void makeGratuitous(ArpPacket& p,
                           const unsigned char sha[6], const unsigned char spa[4]) {
    // Announce own IP-MAC: a broadcast request whose target IP is the sender's own IP.
    makeRequest(p, sha, spa, spa);
}

#endif
