// Go-Back-N ARQ sender. Single timer for the whole outstanding window;
// on timeout the entire window [base, nextSeq) is retransmitted.
// usage: ./gbn_sender <input_file> <window_N> <loss_prob> <error_prob> [fcs_scheme=crc32]
#include "../common/common.h"
#include "../common/frame.h"
#include "../common/channel.h"
#include "../common/rtt_timer.h"
#include "../common/socket_util.h"
#include <iostream>
#include <chrono>
#include <map>
#include <set>
#include <cstdio>
using namespace std;
using namespace std::chrono;

static const string SRC_MAC = "F8CA12E9B0AA";
static const string DST_MAC = "CA29C8F10AB6";

int main(int argc, char** argv) {
    if (argc < 5) {
        fprintf(stderr, "usage: %s <input_file> <window_N> <loss_prob> <error_prob> [fcs_scheme=crc32]\n", argv[0]);
        return 1;
    }
    string inputFile = argv[1];
    int windowN = atoi(argv[2]);
    double lossProb  = atof(argv[3]);
    double errorProb = atof(argv[4]);
    FCS scheme = (argc >= 6) ? parseScheme(argv[5]) : CRC32;
    if (windowN < 1) windowN = 1;
    if (windowN > 255) windowN = 255;   // 1-byte seq field bound

    seedChannelRng();
    vector<DataFrame> frames = framing(inputFile, scheme, SRC_MAC, DST_MAC);
    if (frames.empty()) { fprintf(stderr, "no frames to send (empty/missing file)\n"); return 1; }
    int total = (int)frames.size();

    int fd = makeUdpSocket(GBN_SENDER_PORT);
    sockaddr_in dst = loopbackAddr(GBN_RECEIVER_PORT);
    ChannelConfig cfg{lossProb, errorProb, 1, 15};
    ChannelStats  chStats{};
    RttTimer timer;

    sendSetup(fd, dst, (unsigned)total);

    int base = 0, nextSeq = 0;
    long framesTransmitted = 0, timeoutRounds = 0;
    double sumRttMs = 0; int rttSamples = 0;
    map<int, high_resolution_clock::time_point> sendTimeOf;
    set<int> retransmitted;
    size_t ackFrameSize = AckFrame::make(DST_MAC, SRC_MAC, 0, scheme).toBytes().size();

    auto t0 = high_resolution_clock::now();

    while (base < total) {
        while (nextSeq < total && nextSeq < base + windowN) {
            string bytes = frames[nextSeq].toBytes();
            channelSend(fd, bytes, dst, cfg, chStats);
            framesTransmitted++;
            sendTimeOf[nextSeq] = high_resolution_clock::now();
            nextSeq++;
        }

        setRecvTimeoutMs(fd, timer.timeoutMs());
        char buf[256];
        sockaddr_in from{}; socklen_t flen = sizeof(from);
        ssize_t n = recvfrom(fd, buf, sizeof(buf), 0, (sockaddr*)&from, &flen);

        if (n == (ssize_t)ackFrameSize) {
            AckFrame a = AckFrame::fromBytes(string(buf, n), scheme);
            if (a.verify(scheme)) {
                int newBase = -1;
                for (int i = base; i < nextSeq; i++)
                    if (frames[i].seq == a.ackno) { newBase = i + 1; break; }
                if (newBase > base) {
                    for (int i = base; i < newBase; i++) {
                        if (retransmitted.find(i) == retransmitted.end()) {
                            double rttMs = duration_cast<duration<double, milli>>(
                                               high_resolution_clock::now() - sendTimeOf[i]).count();
                            timer.onSample(rttMs);
                            sumRttMs += rttMs; rttSamples++;
                        }
                        retransmitted.erase(i);
                    }
                    base = newBase;
                }
            }
        } else {
            // timeout (n < 0) or a garbled/short datagram: go back N -> resend whole window
            for (int i = base; i < nextSeq; i++) {
                string bytes = frames[i].toBytes();
                channelSend(fd, bytes, dst, cfg, chStats);
                framesTransmitted++;
                sendTimeOf[i] = high_resolution_clock::now();
                retransmitted.insert(i);
            }
            timeoutRounds++;
        }
        if (framesTransmitted % 200 == 0 || base == total)
            fprintf(stderr, "[GBN-sender] progress base=%d/%d tx=%ld timeouts=%ld\n",
                    base, total, framesTransmitted, timeoutRounds);
    }

    auto t1 = high_resolution_clock::now();
    double elapsedMs = duration_cast<duration<double, milli>>(t1 - t0).count();

    long fileBytes = 0;
    for (auto& f : frames) fileBytes += f.length;
    double throughputBps = (elapsedMs > 0) ? fileBytes / (elapsedMs / 1000.0) : 0;
    double efficiencyPct = 100.0 * total / framesTransmitted;
    double avgRttMs = rttSamples ? sumRttMs / rttSamples : 0;

    fprintf(stderr,
        "[GBN-sender] N=%d frames=%d tx_attempts=%ld retransmissions=%ld dropped=%ld corrupted=%ld "
        "elapsed_ms=%.2f avg_rtt_ms=%.2f throughput_Bps=%.1f efficiency=%.2f%%\n",
        windowN, total, framesTransmitted, framesTransmitted - total,
        chStats.framesDropped, chStats.framesCorrupted, elapsedMs, avgRttMs, throughputBps, efficiencyPct);

    printf("RESULT,go_back_n,%d,%.2f,%.2f,%s,%d,%ld,%ld,%ld,%ld,%.3f,%.3f,%.3f,%.3f\n",
           windowN, lossProb, errorProb, schemeName(scheme), total, framesTransmitted,
           framesTransmitted - total, chStats.framesDropped, chStats.framesCorrupted,
           elapsedMs, avgRttMs, throughputBps, efficiencyPct);

    close(fd);
    return 0;
}
