#include "framing.h"
#include <fstream>
#include <iostream>
using namespace std;

vector<Frame> framing(string file_name, FCS fcs_type, int payload_size,
                      string src_mac, string dst_mac) {
    ifstream myFile(file_name, ios::binary);
    unsigned short int seqno = 0;
    if (!myFile) { cout << "Failed to open file\n"; return {}; }

    vector<char> buffer(payload_size);
    vector<Frame> frames;
    while (myFile.read(buffer.data(), payload_size) || myFile.gcount() > 0) {
        streamsize bytesRead = myFile.gcount();
        string chunk(buffer.data(), bytesRead);
        Frame f(src_mac, dst_mac, chunk, fcs_type, seqno++,
                static_cast<unsigned short>(payload_size));
        frames.push_back(f);
    }
    myFile.close();
    return frames;
}
