
// usage: ./sender <host> <port> <id> <inputFile>
#include "common/protocol.h"
#include "common/net_util.h"
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <unistd.h>
using namespace std;

int main(int argc, char** argv) {
    if (argc < 5) {
        fprintf(stderr, "usage: %s <host> <port> <id> <inputFile>\n", argv[0]);
        return 1;
    }
    const char* host = argv[1];
    int port = atoi(argv[2]);
    int myId = atoi(argv[3]);

    ifstream in(argv[4], ios::binary);
    if (!in) {
        fprintf(stderr, "[sender %d] cannot open %s\n", myId, argv[4]);
        return 1;
    }
    vector<unsigned char> data((istreambuf_iterator<char>(in)), istreambuf_iterator<char>());
    long totalBits = (long)data.size() * 8;

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, host, &addr.sin_addr);
    if (connect(fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "[sender %d] connect failed\n", myId);
        return 1;
    }
    setNoDelay(fd);
    LineSocket sock(fd);
    sock.writeLine("REGISTER SENDER " + to_string(myId));

    string line;
    if (!sock.readLine(line) || line.rfind("CODE", 0) != 0) {
        fprintf(stderr, "[sender %d] registration failed\n", myId);
        return 1;
    }
    const char* p = line.c_str() + 4;
    strtol(p, (char**)&p, 10);   // id
    int L = (int)strtol(p, (char**)&p, 10);
    vector<int> code(L);
    for (int& c : code) c = (int)strtol(p, (char**)&p, 10);

    long sentBits = 0;
    while (sock.readLine(line)) {
        if (line == "DONE") break;
        if (line.rfind("SLOT", 0) != 0) continue;
        long k = atol(line.c_str() + 5);

        if (sentBits >= totalBits) {
            sock.writeLine("IDLE " + to_string(k));
            continue;
        }
        int bit = (data[sentBits / 8] >> (7 - sentBits % 8)) & 1;
        int d = bit ? 1 : -1;
        string msg = "CHIPS " + to_string(k);
        for (int c : code) msg += (d * c > 0) ? " 1" : " -1";
        sock.writeLine(msg);
        sentBits++;
    }

    fprintf(stderr, "[sender %d] sent %ld bits (%zu bytes) using a %d-chip code\n",
            myId, sentBits, data.size(), L);
    close(fd);
    return 0;
}
