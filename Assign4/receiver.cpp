
// usage: ./receiver <host> <port> <id> <outputFile>
#include "common/protocol.h"
#include "common/net_util.h"
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
using namespace std;

int main(int argc, char** argv) {
    if (argc < 5) {
        fprintf(stderr, "usage: %s <host> <port> <id> <outputFile>\n", argv[0]);
        return 1;
    }
    const char* host = argv[1];
    int port = atoi(argv[2]);
    int myId = atoi(argv[3]);
    const char* outPath = argv[4];

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, host, &addr.sin_addr);
    if (connect(fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "[receiver %d] connect failed\n", myId);
        return 1;
    }
    setNoDelay(fd);
    LineSocket sock(fd);
    sock.writeLine("REGISTER RECEIVER " + to_string(myId));

    string line;
    if (!sock.readLine(line) || line.rfind("CODE", 0) != 0) {
        fprintf(stderr, "[receiver %d] registration failed\n", myId);
        return 1;
    }
    const char* p = line.c_str() + 4;
    strtol(p, (char**)&p, 10);   // id
    int L = (int)strtol(p, (char**)&p, 10);
    vector<int> code(L);
    for (int& c : code) c = (int)strtol(p, (char**)&p, 10);

    vector<unsigned char> bytes;
    unsigned char cur = 0;
    long bits = 0, silentSlots = 0;

    while (sock.readLine(line)) {
        if (line == "DONE") break;
        if (line.rfind("SIGNAL", 0) != 0) continue;
        p = line.c_str() + 6;
        long k = strtol(p, (char**)&p, 10);

        double v = 0;
        for (int j = 0; j < L; j++) v += strtod(p, (char**)&p) * code[j];
        v /= L;

        char sym;
        if (v > 0.5)       sym = '1';
        else if (v < -0.5) sym = '0';
        else               sym = '-';

        if (sym == '-') {
            silentSlots++;
        } else {
            cur = (unsigned char)((cur << 1) | (sym == '1'));
            if (++bits % 8 == 0) { bytes.push_back(cur); cur = 0; }
        }
        sock.writeLine("DECODED " + to_string(k) + " " + sym);
    }
    close(fd);

    FILE* out = fopen(outPath, "wb");
    if (!out) {
        fprintf(stderr, "[receiver %d] cannot write %s\n", myId, outPath);
        return 1;
    }
    fwrite(bytes.data(), 1, bytes.size(), out);
    fclose(out);

    fprintf(stderr, "[receiver %d] decoded %ld bits -> %zu bytes to %s (%ld silent slots, %ld leftover bits)\n",
            myId, bits, bytes.size(), outPath, silentSlots, bits % 8);
    return 0;
}
