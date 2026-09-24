#include "frame.h"
#include <fstream>
#include <iostream>
using namespace std;

// ================= DataFrame =================
DataFrame DataFrame::make(const string& srcMacHex, const string& dstMacHex,
                           const string& data, unsigned char seq, FCS scheme) {
    DataFrame f;
    f.srcMAC = hexToBytes(srcMacHex);
    f.dstMAC = hexToBytes(dstMacHex);
    f.seq = seq;
    f.length = static_cast<unsigned short>(data.length());

    string payload = data;
    if (payload.length() < (size_t)PAYLOAD_SIZE) payload.append(PAYLOAD_SIZE - payload.length(), '\0');
    f.payload = payload;

    string dataword = f.srcMAC + f.dstMAC + uint16ToBytes(f.length) +
                       string(1, (char)f.seq) + f.payload;
    f.fcs = calculateFCS(dataword, scheme);
    return f;
}

string DataFrame::toBytes() const {
    return srcMAC + dstMAC + uint16ToBytes(length) + string(1, (char)seq) + payload + fcs;
}

DataFrame DataFrame::fromBytes(const string& raw, FCS scheme) {
    DataFrame f;
    size_t p = 0;
    f.srcMAC = raw.substr(p, 6); p += 6;
    f.dstMAC = raw.substr(p, 6); p += 6;
    f.length = (static_cast<unsigned char>(raw[p]) << 8) | static_cast<unsigned char>(raw[p + 1]); p += 2;
    f.seq = static_cast<unsigned char>(raw[p]); p += 1;
    f.payload = raw.substr(p, PAYLOAD_SIZE); p += PAYLOAD_SIZE;
    f.fcs = raw.substr(p);
    (void)scheme;
    return f;
}

bool DataFrame::verify(FCS scheme) const {
    string dataword = srcMAC + dstMAC + uint16ToBytes(length) + string(1, (char)seq) + payload;
    return verifyFCSBytes(dataword, fcs, scheme);
}

// ================= AckFrame =================
AckFrame AckFrame::make(const string& srcMacHex, const string& dstMacHex,
                         unsigned char ackno, FCS scheme) {
    AckFrame a;
    a.srcMAC = hexToBytes(srcMacHex);
    a.dstMAC = hexToBytes(dstMacHex);
    a.ackno = ackno;
    a.type = 0;
    string dataword = a.srcMAC + a.dstMAC + string(1, (char)a.ackno) + string(1, (char)a.type);
    a.fcs = calculateFCS(dataword, scheme);
    return a;
}

string AckFrame::toBytes() const {
    return srcMAC + dstMAC + string(1, (char)ackno) + string(1, (char)type) + fcs;
}

AckFrame AckFrame::fromBytes(const string& raw, FCS scheme) {
    AckFrame a;
    size_t p = 0;
    a.srcMAC = raw.substr(p, 6); p += 6;
    a.dstMAC = raw.substr(p, 6); p += 6;
    a.ackno = static_cast<unsigned char>(raw[p]); p += 1;
    a.type = static_cast<unsigned char>(raw[p]); p += 1;
    a.fcs = raw.substr(p);
    (void)scheme;
    return a;
}

bool AckFrame::verify(FCS scheme) const {
    string dataword = srcMAC + dstMAC + string(1, (char)ackno) + string(1, (char)type);
    return verifyFCSBytes(dataword, fcs, scheme);
}

// ================= framing =================
vector<DataFrame> framing(const string& fileName, FCS scheme,
                           const string& srcMacHex, const string& dstMacHex) {
    ifstream in(fileName, ios::binary);
    vector<DataFrame> frames;
    if (!in) { cerr << "framing(): failed to open " << fileName << "\n"; return frames; }

    vector<char> buf(PAYLOAD_SIZE);
    unsigned char seq = 0;
    while (in.read(buf.data(), PAYLOAD_SIZE) || in.gcount() > 0) {
        streamsize n = in.gcount();
        string chunk(buf.data(), n);
        frames.push_back(DataFrame::make(srcMacHex, dstMacHex, chunk, seq, scheme));
        seq = (unsigned char)((seq + 1) % 256);
    }
    return frames;
}
