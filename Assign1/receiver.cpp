#include "common.h"
#include "socket_util.h"
#include <iostream>
using namespace std;

// receiver = UDP SERVER (Linux).  ./receiver [checksum16|crc8|crc10|crc16|crc32]

int main(int argc, char** argv) {
    FCS scheme = (argc >= 2) ? parseScheme(argv[1]) : CHECKSUM16;

    int srv = socket(AF_INET, SOCK_DGRAM, 0);
    if (srv < 0) { cout << "socket() failed\n"; return 1; }

    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(srv, (sockaddr*)&addr, sizeof(addr)) < 0) {
        cout << "bind failed (port busy?)\n"; return 1;
    }
    printf("========================================================\n");
    printf("  Receiver  —  UDP port %d  —  Scheme: %s\n", PORT, schemeName(scheme));
    printf("========================================================\n");

    char buf[2048];
    sockaddr_in from{};
    socklen_t flen = sizeof(from);
    int total = 0, accepted = 0, rejected = 0;

    while (true) {
        ssize_t n = recvfrom(srv, buf, sizeof(buf), 0, (sockaddr*)&from, &flen);
        if (n < 0)  break;
        if (n == 0) break;

        string raw(buf, n);
        Frame r = Frame::fromBytes(raw, scheme, PAYLOAD_SIZE);
        bool ok = verifyFCS(r, scheme);
        total++;

        if (ok) {
            accepted++;
            printf("  Frame #%-4d  seq=%-5d  [ ACCEPT ]  FCS OK\n",
                   total, r.seqno);
        } else {
            rejected++;
            printf("  Frame #%-4d  seq=%-5d  [ REJECT ]  FCS mismatch — error detected\n",
                   total, r.seqno);
        }
    }

    printf("--------------------------------------------------------\n");
    printf("  Total: %-5d  Accepted: %-5d  Rejected: %d\n",
           total, accepted, rejected);
    if (total > 0)
        printf("  Detection rate: %.2f%%\n", 100.0 * rejected / total);
    printf("========================================================\n");

    close(srv);
    return 0;
}
