#include "common.h"
#include <iostream>
#include <iomanip>
using namespace std;

// ================= helpers =================
FCS parseScheme(const string& name) {
    if (name == "crc8")       return CRC8;
    if (name == "crc10")      return CRC10;
    if (name == "crc16")      return CRC16;
    if (name == "crc32")      return CRC32;
    return CHECKSUM16;   // default
}

const char* schemeName(FCS fcs_type) {
    switch (fcs_type) {
        case CRC8:  return "CRC8";
        case CRC10: return "CRC10";
        case CRC16: return "CRC16";
        case CRC32: return "CRC32";
        default:    return "CHECKSUM16";
    }
}

string hexToBytes(const string& hex) {
    string bytes;
    for (size_t i = 0; i < hex.length(); i += 2) {
        string byteString = hex.substr(i, 2);
        char byte = static_cast<char>(stoul(byteString, nullptr, 16));
        bytes.push_back(byte);
    }
    return bytes;
}

string uint16ToBytes(unsigned short value) {
    string bytes;
    bytes.push_back(static_cast<char>((value >> 8) & 0xFF));
    bytes.push_back(static_cast<char>(value & 0xFF));
    return bytes;
}

void printHex(const string& data) {
    for (unsigned char byte : data)
        cout << hex << setw(2) << setfill('0') << static_cast<int>(byte) << " ";
    cout << dec << endl;
}

// ================= Checksum-16 =================
string calculateCheckSum16(string dataword) {
    unsigned int sum = 0;
    if (dataword.length() % 2 != 0) dataword.push_back('\0');

    for (size_t i = 0; i < dataword.length(); i += 2) {
        unsigned short word =
            (static_cast<unsigned char>(dataword[i]) << 8) |
             static_cast<unsigned char>(dataword[i + 1]);
        sum += word;
        if (sum > 0xFFFF) sum = (sum & 0xFFFF) + (sum >> 16);
    }
    unsigned short checksum = static_cast<unsigned short>(~sum);
    string result;
    result.push_back(static_cast<char>((checksum >> 8) & 0xFF));
    result.push_back(static_cast<char>(checksum & 0xFF));
    return result;
}

// ================= CRC engine =================
static vector<int> bytesToBits(const string& data) {
    vector<int> bits;
    for (unsigned char byte : data)
        for (int b = 7; b >= 0; b--)
            bits.push_back((byte >> b) & 1);
    return bits;
}

string calculateCRC(string dataword, vector<int> gen) {
    int n = gen.size() - 1;
    vector<int> bits = bytesToBits(dataword);
    int msgLen = bits.size();
    for (int i = 0; i < n; i++) bits.push_back(0);

    for (int i = 0; i < msgLen; i++)
        if (bits[i] == 1)
            for (int j = 0; j <= n; j++)
                bits[i + j] ^= gen[j];

    int numBytes = (n + 7) / 8;
    int pad = numBytes * 8 - n;
    vector<int> out(numBytes * 8, 0);
    for (int i = 0; i < n; i++) out[pad + i] = bits[msgLen + i];

    string result;
    for (int b = 0; b < numBytes; b++) {
        unsigned char byte = 0;
        for (int k = 0; k < 8; k++) byte = (byte << 1) | out[b * 8 + k];
        result.push_back(static_cast<char>(byte));
    }
    return result;
}

string calculateCRC8(string dataword) {
    vector<int> gen = {1,1,1,0,1,0,1,0,1};
    return calculateCRC(dataword, gen);
}

string calculateCRC10(string dataword) {
    vector<int> gen = {1,0,0,0,1,1,0,0,1,1,1};
    return calculateCRC(dataword, gen);
}

string calculateCRC16(string dataword) {
    vector<int> gen = {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,1};
    return calculateCRC(dataword, gen);
}

string calculateCRC32(string dataword) {
    vector<int> gen = {
        1,0,0,0,0,1,1,0,
        0,1,1,0,0,0,0,0,
        0,0,0,1,1,0,1,1,
        1,0,0,1,1,0,1,1,
        1,0,1,1,1,0,1,1,
        1
    };
    return calculateCRC(dataword, gen);
}

string calculateFCS(const string& dataword, FCS fcs_type) {
    switch (fcs_type) {
        case CHECKSUM16: return calculateCheckSum16(dataword);
        case CRC8:       return calculateCRC8(dataword);
        case CRC10:      return calculateCRC10(dataword);
        case CRC16:      return calculateCRC16(dataword);
        case CRC32:      return calculateCRC32(dataword);
    }
    return "";
}

bool verifyFCSBytes(const string& dataword, const string& fcs, FCS fcs_type) {
    return calculateFCS(dataword, fcs_type) == fcs;
}
