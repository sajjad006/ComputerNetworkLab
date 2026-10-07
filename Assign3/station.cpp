// A single contending station. Runs a saturated traffic model (always has
// a frame ready to send) and implements carrier sensing + persistence
// strategy per the requested CSMA scheme. All four schemes share the same
// post-collision binary exponential backoff (IEEE 802.3-style) so that the
// only independent variable under test is the idle/busy sensing behavior
// itself, isolating "persistence strategy" as the thing being compared.
//
// usage: ./station <host> <port> <scheme> <p> <backoffCap>
#include "common/protocol.h"
#include "common/net_util.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <unistd.h>
using namespace std;

static int uniformInt(int lo, int hi) {   // inclusive
    if (hi <= lo) return lo;
    return lo + rand() % (hi - lo + 1);
}

static double uniform01() { return rand() / (double)RAND_MAX; }

int main(int argc, char** argv) {
    if (argc < 6) {
        fprintf(stderr, "usage: %s <host> <port> <scheme> <p> <backoffCap>\n", argv[0]);
        return 1;
    }
    const char* host = argv[1];
    int port = atoi(argv[2]);
    CsmaScheme scheme = parseSchemeArg(argv[3]);
    double p = atof(argv[4]);
    int backoffCap = atoi(argv[5]);

    srand((unsigned)time(nullptr) ^ (unsigned)getpid());

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, host, &addr.sin_addr);
    if (connect(fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "[station] connect failed\n");
        return 1;
    }
    setNoDelay(fd);
    LineSocket sock(fd);
    sock.writeLine("REGISTER");
    string idLine;
    sock.readLine(idLine);
    int myId = -1;
    sscanf(idLine.c_str(), "ID %d", &myId);

    bool awaitingOutcome = false;
    int backoffRemaining = 0;
    int collisionCount = 0;
    const int NONPERSIST_DEFER_MAX = 8;

    string line;
    while (sock.readLine(line)) {
        if (line == "DONE") break;

        if (line.rfind("SLOT", 0) == 0) {
            long slotNo; char stateStr[16];
            sscanf(line.c_str(), "SLOT %ld %15s", &slotNo, stateStr);
            ChannelState state = (strcmp(stateStr, "BUSY") == 0) ? CH_BUSY : CH_IDLE;

            StationAction action = ACT_WAIT;
            if (awaitingOutcome) {
                action = ACT_WAIT;   // our own frame is in flight; nothing to decide
            } else if (backoffRemaining > 0) {
                backoffRemaining--;
                action = ACT_WAIT;
            } else {
                switch (scheme) {
                    case NON_PERSISTENT:
                        if (state == CH_IDLE) action = ACT_TRANSMIT;
                        else { backoffRemaining = uniformInt(1, NONPERSIST_DEFER_MAX); action = ACT_WAIT; }
                        break;
                    case ONE_PERSISTENT:
                        action = (state == CH_IDLE) ? ACT_TRANSMIT : ACT_WAIT;
                        break;
                    case P_PERSISTENT:
                        if (state == CH_IDLE) action = (uniform01() < p) ? ACT_TRANSMIT : ACT_WAIT;
                        else action = ACT_WAIT;
                        break;
                    case CSMA_CD:
                        action = (state == CH_IDLE) ? ACT_TRANSMIT : ACT_WAIT;
                        break;
                }
            }

            if (action == ACT_TRANSMIT) awaitingOutcome = true;

            char reply[64];
            snprintf(reply, sizeof(reply), "ACT %ld %s", slotNo, action == ACT_TRANSMIT ? "TRANSMIT" : "WAIT");
            sock.writeLine(reply);

        } else if (line.rfind("OUTCOME", 0) == 0) {
            awaitingOutcome = false;
            if (line.find("SUCCESS") != string::npos) {
                collisionCount = 0;
                backoffRemaining = 0;
            } else {   // COLLISION
                collisionCount++;
                int window = 1 << (collisionCount < backoffCap ? collisionCount : backoffCap);
                backoffRemaining = uniformInt(0, window - 1);
            }
        }
    }

    close(fd);
    return 0;
}
