#ifndef COMMON_H
#define COMMON_H

#include <string>
#include <vector>
using namespace std;

// Fixed payload size (bytes) taken from the input file per data frame.
// Assignment allows 46-1500; we use the Ethernet minimum, 46, so a
// modest input file produces many frames -> windowing effects are visible.
const int PAYLOAD_SIZE = 46;

enum FCS { CHECKSUM16, CRC8, CRC10, CRC16, CRC32 };

// ---- helpers (ported from Assignment 1) ----
string hexToBytes(const string& hex);
string uint16ToBytes(unsigned short value);
void   printHex(const string& data);
FCS    parseScheme(const string& name);   // "checksum16"|"crc8"|"crc10"|"crc16"|"crc32"
const char* schemeName(FCS fcs_type);

// ---- FCS calculators (ported from Assignment 1) ----
string calculateCheckSum16(string dataword);
string calculateCRC(string dataword, vector<int> gen);
string calculateCRC8(string dataword);
string calculateCRC10(string dataword);
string calculateCRC16(string dataword);
string calculateCRC32(string dataword);
string calculateFCS(const string& dataword, FCS fcs_type);
bool   verifyFCSBytes(const string& dataword, const string& fcs, FCS fcs_type);

#endif
