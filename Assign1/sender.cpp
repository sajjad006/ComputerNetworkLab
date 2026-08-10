// sender = UDP CLIENT (Linux).  ./sender <inputfile> [none|single|two|odd|burst] [checksum16|crc8|crc10|crc16|crc32]

#include "common.h"
#include "framing.h"
#include "error_injector.h"
#include "socket_util.h"
#include <iostream>
#include <cstdlib>
#include <ctime>
using namespace std;

int main(int argc, char** argv) {

    if (argc < 2) { cout << "usage: ./sender <inputfile> [none|single|two|odd|burst] [checksum16|crc8|crc10|crc16|crc32]\n"; return 1; }
    srand(time(nullptr));

    string inputFile = argv[1];
    string errArg = (argc >= 3) ? argv[2] : "none";
    FCS scheme    = (argc >= 4) ? parseScheme(argv[3]) : CHECKSUM16;

    int s = socket(AF_INET, SOCK_DGRAM, 0);   // SOCK_DGRAM = UDP
    if (s < 0) { cout << "socket() failed\n"; return 1; }

    sockaddr_in dst{};
    dst.sin_family = AF_INET;
    dst.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &dst.sin_addr);

    vector<Frame> frames = framing(inputFile, scheme, PAYLOAD_SIZE,
                                   "F8CA12E9B0AA", "CA29C8F10AB6");
    printf("--------------------------------------------------------\n");
    printf("Scheme    : %s\n", schemeName(scheme));
    printf("Error mode: %s\n", errArg.c_str());
    printf("Frames    : %zu\n", frames.size());
    printf("Dest port : %d\n", PORT);
    printf("--------------------------------------------------------\n");

    int frameNo = 0;
    for (auto& f : frames) {
        string bytes = f.toBinary();
        int inject = rand() % 2;

        printf("\n[Frame #%d]  seq=%-5d  size=%zu bytes\n",
               frameNo, f.seqno, bytes.size());
        printf("  Original : ");
        printHex(bytes);

        if (inject) {
            if      (errArg == "single") bytes = injectError(bytes, SINGLE_BIT);
            else if (errArg == "two")    bytes = injectError(bytes, TWO_ISOLATED);
            else if (errArg == "odd")    bytes = injectError(bytes, ODD_ERRORS);
            else if (errArg == "burst")  bytes = injectError(bytes, BURST);
            printf("  Injected : YES (%s)\n", errArg.c_str());
            printf("  Corrupted: ");
            printHex(bytes);
        } else {
            printf("  Injected : NO  (sent clean)\n");
        }

        sendto(s, bytes.data(), bytes.size(), 0, (sockaddr*)&dst, sizeof(dst));
        frameNo++;
    }

    sendto(s, "", 0, 0, (sockaddr*)&dst, sizeof(dst));   // empty datagram = end-marker

    printf("--------------------------------------------------------\n");
    printf("Sent %zu frame(s) + end-marker.\n", frames.size());
    close(s);
    return 0;
}
