// DHCP message format (RFC 2131 / RFC 2132), shared by the client and the server.
#ifndef DHCP_PACKET_H
#define DHCP_PACKET_H

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <arpa/inet.h>   // htonl / ntohl

// ---------------------------------------------------------------- Constants
constexpr uint16_t SERVER_PORT = 67;
constexpr uint16_t CLIENT_PORT = 68;

constexpr uint8_t BOOTREQUEST = 1;   // op field: client -> server
constexpr uint8_t BOOTREPLY   = 2;   // op field: server -> client

// Option 53 values (DHCP message type)
constexpr uint8_t DHCPDISCOVER = 1;
constexpr uint8_t DHCPOFFER    = 2;
constexpr uint8_t DHCPREQUEST  = 3;
constexpr uint8_t DHCPDECLINE  = 4;
constexpr uint8_t DHCPACK      = 5;
constexpr uint8_t DHCPNAK      = 6;
constexpr uint8_t DHCPRELEASE  = 7;

// Option codes. Every option is [code][length][data...] except PAD and END (1 byte).
constexpr uint8_t OPT_PAD          = 0;
constexpr uint8_t OPT_SUBNET_MASK  = 1;
constexpr uint8_t OPT_ROUTER       = 3;
constexpr uint8_t OPT_DNS_SERVER   = 6;
constexpr uint8_t OPT_REQUESTED_IP = 50;
constexpr uint8_t OPT_LEASE_TIME   = 51;
constexpr uint8_t OPT_MSG_TYPE     = 53;
constexpr uint8_t OPT_SERVER_ID    = 54;
constexpr uint8_t OPT_PARAM_LIST   = 55;
constexpr uint8_t OPT_CLIENT_ID    = 61;
constexpr uint8_t OPT_END          = 255;

constexpr uint32_t DHCP_MAGIC_COOKIE = 0x63825363;  // marks the start of the options
constexpr std::size_t DHCP_MIN_PACKET_LEN = 300;    // BOOTP minimum, padded with PAD bytes

// ---------------------------------------------------------------- Packet layout
// Fields are naturally aligned, so no #pragma pack is needed (checked by static_assert).
struct Dhcp_packet_t {
    uint8_t  op;            // 1 = BOOTREQUEST, 2 = BOOTREPLY
    uint8_t  htype;         // hardware type: 1 = Ethernet
    uint8_t  hlen;          // hardware address length: 6 for Ethernet
    uint8_t  hops;          // used by relay agents, client sets 0
    uint32_t xid;           // transaction ID, random, same for the whole exchange
    uint16_t secs;          // seconds since the client started
    uint16_t flags;         // 0x8000 = broadcast flag ("reply by broadcast, I have no IP yet")
    uint32_t ciaddr;        // client's current IP (only when already BOUND/RENEWING)
    uint32_t yiaddr;        // "your" IP: the address the server offers/assigns
    uint32_t siaddr;        // next (boot) server IP
    uint32_t giaddr;        // relay agent IP
    uint8_t  chaddr[16];    // client hardware (MAC) address, 6 bytes used
    char     sname[64];     // optional server host name
    char     file[128];     // optional boot file name
    uint32_t magic_cookie;  // 99.130.83.99
    uint8_t  options[312];  // variable-length options
};
static_assert(offsetof(Dhcp_packet_t, magic_cookie) == 236, "fixed header must be 236 bytes");
static_assert(offsetof(Dhcp_packet_t, options) == 240, "options start at byte 240");
constexpr std::size_t HEADER_LEN = offsetof(Dhcp_packet_t, options);

// ---------------------------------------------------------------- Writing options
// Appends one [code][length][data] option at position i.
inline void addOption(Dhcp_packet_t& p, std::size_t& i, uint8_t code, const void* data,
                      uint8_t len) {
    p.options[i++] = code;
    p.options[i++] = len;
    std::memcpy(p.options + i, data, len);
    i += len;
}

// Writes END, pads to the BOOTP minimum, and returns the number of bytes to send.
inline std::size_t finishOptions(Dhcp_packet_t& p, std::size_t i) {
    p.options[i++] = OPT_END;
    while (HEADER_LEN + i < DHCP_MIN_PACKET_LEN) p.options[i++] = OPT_PAD;
    return HEADER_LEN + i;
}

// ---------------------------------------------------------------- Reading options
// After parsing, data[code] points at the option's bytes and len[code] is its length.
struct DhcpOptions {
    const uint8_t* data[256]{};
    uint8_t len[256]{};

    bool has(uint8_t code) const { return data[code] != nullptr; }
    uint8_t byte(uint8_t code) const { return len[code] >= 1 ? data[code][0] : 0; }
    // First IPv4 address in the option, in network byte order (0 if missing).
    uint32_t ip(uint8_t code) const {
        uint32_t value = 0;
        if (len[code] >= 4) std::memcpy(&value, data[code], 4);
        return value;
    }
};

// Checks the magic cookie and walks the options up to END.
// Returns false if anything is malformed or runs past `length` bytes.
inline bool parseOptions(const Dhcp_packet_t& p, std::size_t length, DhcpOptions& out) {
    if (length < HEADER_LEN || length > sizeof(p) ||
        ntohl(p.magic_cookie) != DHCP_MAGIC_COOKIE) return false;
    out = DhcpOptions{};
    const std::size_t end = length - HEADER_LEN;
    for (std::size_t i = 0; i < end;) {
        uint8_t code = p.options[i++];
        if (code == OPT_PAD) continue;
        if (code == OPT_END) return true;
        if (i >= end) return false;              // no length byte
        uint8_t len = p.options[i++];
        if (len > end - i) return false;         // data runs past the packet
        out.data[code] = p.options + i;
        out.len[code] = len;
        i += len;
    }
    return false;                                // no END option
}

inline const char* messageName(uint8_t type) {
    static const char* names[] = {"?", "DISCOVER", "OFFER", "REQUEST", "DECLINE",
                                  "ACK", "NAK", "RELEASE", "INFORM"};
    return type <= 8 ? names[type] : "?";
}

#endif
