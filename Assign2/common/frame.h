#ifndef FRAME_H
#define FRAME_H

#include "common.h"
#include <string>
#include <vector>
using namespace std;

// ---------------------------------------------------------------------
// Data Frame  (Fig 1 of the assignment):
//   Header:  Source MAC(6) | Dest MAC(6) | Length(2) | Seq no.(1)   = 15 bytes
//   Data:    Payload                                                = 46-1500 bytes
//   Trailer: FCS (CRC/Checksum)                                     = scheme-dependent
//              (assignment shows 4 bytes -> default scheme is CRC32)
// Sequence number is 1 byte -> wraps modulo 256, so any window size N
// used by Go-Back-N / Selective-Repeat must satisfy N <= 128 (SR) or
// N <= 255 (GBN) to avoid ambiguity, per the sliding-window bound.
// ---------------------------------------------------------------------
struct DataFrame {
    string srcMAC, dstMAC;          // 6 raw bytes each
    unsigned short length;          // payload length actually used
    unsigned char seq;              // 0-255, wraps
    string payload;                 // padded to PAYLOAD_SIZE
    string fcs;

    static DataFrame make(const string& srcMacHex, const string& dstMacHex,
                           const string& data, unsigned char seq, FCS scheme);
    string toBytes() const;
    static DataFrame fromBytes(const string& raw, FCS scheme);
    bool verify(FCS scheme) const;   // recompute FCS over header+payload and compare
};

// ---------------------------------------------------------------------
// ACK Frame:
//   Header:  Source MAC(6) | Dest MAC(6) | Ack no.(1) | Type(1)
//   Trailer: FCS
// Type: 0 = ACK. (NAK not used - all three schemes here rely on timeout
// for loss recovery, per the assignment's Timer()/Timeout() design.)
// ---------------------------------------------------------------------
struct AckFrame {
    string srcMAC, dstMAC;
    unsigned char ackno;
    unsigned char type;             // 0 = ACK
    string fcs;

    static AckFrame make(const string& srcMacHex, const string& dstMacHex,
                          unsigned char ackno, FCS scheme);
    string toBytes() const;
    static AckFrame fromBytes(const string& raw, FCS scheme);
    bool verify(FCS scheme) const;
};

// Split a whole file into a sequence of DataFrames of PAYLOAD_SIZE bytes each.
vector<DataFrame> framing(const string& fileName, FCS scheme,
                           const string& srcMacHex, const string& dstMacHex);

#endif
