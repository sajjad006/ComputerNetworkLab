// Emulated shared channel. Coordinates N station processes in lock-step
// time slots (the standard slotted approximation used to analyze CSMA/CD:
// one slot = 2*propagation delay, the minimum time for a station to learn
// the medium is occupied). Every slot the server broadcasts the sensed
// channel state (IDLE/BUSY), collects each station's TRANSMIT/WAIT action,
// resolves collisions/successes, and reports the outcome only to the
// station(s) actually involved -- a bystander can sense BUSY but, exactly
// as on a real LAN, cannot itself detect that a collision occurred.
//
// usage: ./channel_server <port> <N> <scheme> <p> <frameSlots> <jamSlots> <maxSlots> [demoDelayMs]
//   demoDelayMs > 0 prints a live per-slot trace (state, transmitters, outcomes) and
//   paces the loop with a sleep of that many ms per slot -- for a watchable lab demo.
#include "common/protocol.h"
#include "common/net_util.h"
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
using namespace std;

int main(int argc, char** argv) {
    if (argc < 8) {
        fprintf(stderr, "usage: %s <port> <N> <scheme> <p> <frameSlots> <jamSlots> <maxSlots> [demoDelayMs]\n", argv[0]);
        return 1;
    }
    int port = atoi(argv[1]);
    int N = atoi(argv[2]);
    CsmaScheme scheme = parseSchemeArg(argv[3]);
    double p = atof(argv[4]);
    int frameSlots = atoi(argv[5]);
    int jamSlots = atoi(argv[6]);
    long maxSlots = atol(argv[7]);
    int demoDelayMs = (argc >= 9) ? atoi(argv[8]) : 0;
    bool demo = demoDelayMs > 0;

    int listenFd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    if (bind(listenFd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "[channel_server] bind failed on port %d\n", port);
        return 1;
    }
    listen(listenFd, N + 4);
    fprintf(stderr, "[channel_server] listening on %d, waiting for %d stations...\n", port, N);

    vector<LineSocket*> stations;
    for (int i = 0; i < N; i++) {
        int fd = accept(listenFd, nullptr, nullptr);
        setNoDelay(fd);
        auto* ls = new LineSocket(fd);
        string line;
        ls->readLine(line);   // "REGISTER"
        char msg[64];
        snprintf(msg, sizeof(msg), "ID %d", i);
        ls->writeLine(msg);
        stations.push_back(ls);
    }
    close(listenFd);
    fprintf(stderr, "[channel_server] all %d stations connected; starting %ld slots\n", N, maxSlots);

    vector<long> readySlot(N, 0);
    bool busy = false;
    bool busyIsCollision = false;
    long busyUntil = 0;
    vector<int> busyStations;

    long successes = 0, collisions = 0, idleSlots = 0;
    double sumDelay = 0;

    for (long slot = 0; slot < maxSlots; slot++) {
        ChannelState state = busy ? CH_BUSY : CH_IDLE;
        char slotMsg[64];
        snprintf(slotMsg, sizeof(slotMsg), "SLOT %ld %s", slot, state == CH_BUSY ? "BUSY" : "IDLE");
        for (auto* ls : stations) ls->writeLine(slotMsg);

        vector<bool> transmitted(N, false);
        for (int i = 0; i < N; i++) {
            string line;
            if (!stations[i]->readLine(line)) continue;
            if (line.find("TRANSMIT") != string::npos) transmitted[i] = true;
        }

        if (demo) printf("slot %5ld  state=%-4s", slot, state == CH_BUSY ? "BUSY" : "IDLE");

        if (!busy) {
            vector<int> txers;
            for (int i = 0; i < N; i++) if (transmitted[i]) txers.push_back(i);

            if (txers.size() == 1) {
                busy = true; busyIsCollision = false;
                busyUntil = slot + frameSlots;   // this slot (slot0) is itself part of the duration
                busyStations = txers;
                if (demo) printf("   -> station %d starts TRANSMIT (%d slots)\n", txers[0], frameSlots);
            } else if (txers.size() >= 2) {
                collisions++;
                busy = true; busyIsCollision = true;
                busyUntil = slot + (scheme == CSMA_CD ? jamSlots : frameSlots);
                busyStations = txers;
                if (demo) {
                    printf("   -> COLLISION! stations {");
                    for (int id : txers) printf("%d ", id);
                    printf("} transmitted simultaneously\n");
                }
            } else {
                idleSlots++;
                if (demo) printf("\n");
            }
        } else if (demo) {
            printf("\n");
        }

        if (busy && slot + 1 == busyUntil) {
            if (!busyIsCollision) {
                int id = busyStations[0];
                stations[id]->writeLine("OUTCOME SUCCESS");
                successes++;
                double delay = (double)(slot + 1 - readySlot[id]);
                sumDelay += delay;
                readySlot[id] = slot + 1;
                if (demo) printf("           -> OUTCOME: station %d SUCCESS (delay=%.0f slots)\n", id, delay);
            } else {
                for (int id : busyStations) stations[id]->writeLine("OUTCOME COLLISION");
                if (demo) {
                    printf("           -> OUTCOME: COLLISION resolved for stations {");
                    for (int id : busyStations) printf("%d ", id);
                    printf("}, backoff triggered\n");
                }
            }
            busy = false;
            busyStations.clear();
        }

        if (demo) { fflush(stdout); usleep(demoDelayMs * 1000); }
    }

    for (auto* ls : stations) { ls->writeLine("DONE"); close(ls->fd()); }

    double avgDelay = successes ? sumDelay / successes : 0.0;
    double throughput = (double)(successes * frameSlots) / (double)maxSlots;

    fprintf(stderr,
        "[channel_server] done. successes=%ld collisions=%ld idle=%ld avg_delay_slots=%.3f throughput=%.4f\n",
        successes, collisions, idleSlots, avgDelay, throughput);

    printf("RESULT,%s,%d,%.3f,%d,%d,%ld,%ld,%ld,%ld,%.4f,%.4f\n",
           schemeName(scheme), N, p, frameSlots, jamSlots, maxSlots,
           successes, collisions, idleSlots, avgDelay, throughput);

    for (auto* ls : stations) delete ls;
    return 0;
}
