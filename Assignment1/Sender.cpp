#include <string>
#include <iostream>
#include <fstream>
#include <vector>

using namespace std;

enum FCS
{
    CHECKSUM16,
    CRC8,
    CRC10,
    CRC16,
    CRC32
};

static string calculateCheckSum16(string dataword) {
    
}

class Frame {
public:    
    string srcMAC, dstMAC;
    unsigned short int payload_length, seqno;
    string payload;
    string fcs;

    Frame(string srcMAC, string dstMAC, string payload, FCS fcs_type, unsigned short int seqno, unsigned short int payload_length) {
        // this->srcMAC=hexToBytes(srcMAC);
        // this->dstMAC=hexToBytes(dstMAC);
        this->srcMAC=srcMAC;
        this->dstMAC=dstMAC;
        this->seqno=seqno;
        this->payload_length=payload.length();

        if (payload.length() < payload_length) {
            int diff=payload_length-payload.length();

            while (diff>0) {
                payload+='0';
                diff--;
            }
        }

        this->payload=payload;
        this->fcs=calculateFCS(srcMAC+dstMAC+to_string(payload_length)+to_string(seqno)+payload, fcs_type);
    }

    // static string hexToBytes(const string& hex) {
    //     string bytes;
    //     for (size_t i = 0; i < hex.length(); i += 2) {
    //         string byteString = hex.substr(i, 2);
    //         char byte = static_cast<char>(stoul(byteString, nullptr, 16));
    //         bytes.push_back(byte);
    //     }
    //     return bytes;
    // }

    static string calculateFCS(string dataword, FCS fcs_type) {
        if (fcs_type==FCS::CHECKSUM16) {
            calculateCheckSum16(dataword);
        } else if (fcs_type==FCS::CRC8) {
            calculateCRC8(dataword);
        } else if (fcs_type==FCS::CRC10) {
            calculateCRC10(dataword);
        } else if (fcs_type==FCS::CRC16) {
            calculateCRC16(dataword);
        } else if (fcs_type==FCS::CRC32) {
            calculateCRC32(dataword);
        } 
    }

    string toBinary() {
        string frame=srcMAC+dstMAC+to_string(payload_length)+to_string(seqno)+payload+fcs;
        return frame;
    } 

    // string toBinary() {
    //     string header = srcMAC + dstMAC + 
    //                     string(reinterpret_cast<char*>(&payload_length), sizeof(payload_length)) + 
    //                     string(reinterpret_cast<char*>(&seqno), sizeof(seqno));
    //     return header + payload + fcs;
    // }
};

vector<Frame> framing(string file_name, FCS fcs_type, int payload_size, string src_mac, string dst_mac) {
    ifstream myFile(file_name, ios::binary);
    unsigned short int seqno=0;

    if (!myFile) {
        cout << "Failed to open file";
        return {};
    }

    vector<char> buffer(payload_size);
    vector<Frame> frames;

    while (myFile.read(buffer.data(), payload_size) || myFile.gcount() > 0) {
        // int bytesRead=myFile.gcount();
        streamsize bytesRead = myFile.gcount();
        
        string chunk(buffer.data(), bytesRead);
        Frame newFrame(src_mac, dst_mac, chunk, fcs_type, seqno++, static_cast<unsigned short int>(payload_size));
        frames.push_back(newFrame);
    }
    myFile.close(); 
    return frames;
}

int main () {

    vector<Frame> result = framing("myfile.txt", FCS.CRC32, 44, "F8CA12E9B0AA", "CA29C8F10AB6");
    cout << "Total frames created: " << result.size() << endl;

    for (int i=0; i<result.size(); i++) {
        cout << result[i].toBinary() << "\n\n\n";
    }
    return 0;
}
