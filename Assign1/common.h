#ifndef COMMON_H
#define COMMON_H

#include <string>
#include <vector>
using namespace std;

const int PAYLOAD_SIZE = 46;

enum FCS { CHECKSUM16, CRC8, CRC10, CRC16, CRC32 };
// FCS scheme = CHECKSUM16;

// ---- helpers ----
string hexToBytes(const string& hex);
string uint16ToBytes(unsigned short value);
void   printHex(const string& data);
FCS    parseScheme(const string& name);   // "checksum16"|"crc8"|"crc10"|"crc16"|"crc32"
const char* schemeName(FCS fcs_type);

// ---- FCS calculators ----
string calculateCheckSum16(string dataword);
string calculateCRC(string dataword, vector<int> gen);
string calculateCRC8(string dataword);
string calculateCRC10(string dataword);
string calculateCRC16(string dataword);
string calculateCRC32(string dataword);

class Frame {
public:
    string srcMAC, dstMAC;
    unsigned short int payload_length, seqno;
    string payload;
    string fcs;

    Frame(string srcMAC, string dstMAC, string payload, FCS fcs_type,
          unsigned short int seqno, unsigned short int payload_size);

    // Rebuild a Frame's fields from raw on-the-wire bytes (receiver side).
    static Frame fromBytes(const string& raw, FCS fcs_type, int payload_size);

    static string calculateFCS(string dataword, FCS fcs_type);
    string toBinary() const;

private:
    Frame() {}
};

bool verifyFCS(const Frame& r, FCS fcs_type);

#endif
