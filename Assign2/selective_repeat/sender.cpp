// Selective-Repeat ARQ sender. Each outstanding frame has its own
// deadline; only frames that individually time out (or are individually
// unacknowledged) are retransmitted, and the window slides past any
// contiguous run of already-acked frames at its base.
// usage: ./sr_sender <input_file> <window_N> <loss_prob> <error_prob> [fcs_scheme=crc32]
#include "../common/common.h"
#include "../common/frame.h"
#include "../common/channel.h"
#include "../common/rtt_timer.h"
#include "../common/socket_util.h"
#include <iostream>
#include <chrono>
#include <vector>
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
    if (windowN > 128) windowN = 128;   // SR bound: N <= seq-space/2 (seq is 1 byte -> 256)

    seedChannelRng();
    vector<DataFrame> frames = framing(inputFile, scheme, SRC_MAC, DST_MAC);
    if (frames.empty()) { fprintf(stderr, "no frames to send (empty/missing file)\n"); return 1; }
    int total = (int)frames.size();

    int fd = makeUdpSocket(SR_SENDER_PORT);
    sockaddr_in dst = loopbackAddr(SR_RECEIVER_PORT);
    ChannelConfig cfg{lossProb, errorProb, 1, 15};
    ChannelStats  chStats{};
    RttTimer timer;

    sendSetup(fd, dst, (unsigned)total);

    vector<bool> acked(total, false), retransmittedFlag(total, false);
    vector<high_resolution_clock::time_point> deadline(total), sentAt(total);
    int base = 0, nextSeq = 0;
    long framesTransmitted = 0;
    double sumRttMs = 0; int rttSamples = 0;
    size_t ackFrameSize = AckFrame::make(DST_MAC, SRC_MAC, 0, scheme).toBytes().size();

    auto sendFrame = [&](int i, high_resolution_clock::time_point now) {
        string bytes = frames[i].toBytes();
        channelSend(fd, bytes, dst, cfg, chStats);
        framesTransmitted++;
        sentAt[i] = now;
        deadline[i] = now + milliseconds((long)timer.timeoutMs());
    };

    auto t0 = high_resolution_clock::now();

    while (base < total) {
        auto now = high_resolution_clock::now();
        while (nextSeq < total && nextSeq < base + windowN) {
            sendFrame(nextSeq, now);
            nextSeq++;
        }

        // nearest deadline among outstanding unacked frames
        auto nearest = deadline[base];
        for (int i = base; i < nextSeq; i++)
            if (!acked[i] && deadline[i] < nearest) nearest = deadline[i];

        double waitMs = duration_cast<duration<double, milli>>(nearest - now).count();
        setRecvTimeoutMs(fd, waitMs > 1 ? waitMs : 1);

        char buf[256];
        sockaddr_in from{}; socklen_t flen = sizeof(from);
        ssize_t n = recvfrom(fd, buf, sizeof(buf), 0, (sockaddr*)&from, &flen);
        auto now2 = high_resolution_clock::now();

        if (n == (ssize_t)ackFrameSize) {
            AckFrame a = AckFrame::fromBytes(string(buf, n), scheme);
            if (a.verify(scheme)) {
                for (int i = base; i < nextSeq; i++) {
                    if (!acked[i] && frames[i].seq == a.ackno) {
                        acked[i] = true;
                        if (!retransmittedFlag[i]) {
                            double rttMs = duration_cast<duration<double, milli>>(now2 - sentAt[i]).count();
                            timer.onSample(rttMs);
                            sumRttMs += rttMs; rttSamples++;
                        }
                        break;
                    }
                }
            }
        }

        while (base < total && acked[base]) base++;

        for (int i = base; i < nextSeq; i++) {
            if (!acked[i] && now2 >= deadline[i]) {
                sendFrame(i, now2);
                retransmittedFlag[i] = true;
            }
        }

        if (framesTransmitted % 200 == 0 || base == total)
            fprintf(stderr, "[SR-sender] progress base=%d/%d tx=%ld\n", base, total, framesTransmitted);
    }

    auto t1 = high_resolution_clock::now();
    double elapsedMs = duration_cast<duration<double, milli>>(t1 - t0).count();

    long fileBytes = 0;
    for (auto& f : frames) fileBytes += f.length;
    double throughputBps = (elapsedMs > 0) ? fileBytes / (elapsedMs / 1000.0) : 0;
    double efficiencyPct = 100.0 * total / framesTransmitted;
    double avgRttMs = rttSamples ? sumRttMs / rttSamples : 0;

    fprintf(stderr,
        "[SR-sender] N=%d frames=%d tx_attempts=%ld retransmissions=%ld dropped=%ld corrupted=%ld "
        "elapsed_ms=%.2f avg_rtt_ms=%.2f throughput_Bps=%.1f efficiency=%.2f%%\n",
        windowN, total, framesTransmitted, framesTransmitted - total,
        chStats.framesDropped, chStats.framesCorrupted, elapsedMs, avgRttMs, throughputBps, efficiencyPct);

    printf("RESULT,selective_repeat,%d,%.2f,%.2f,%s,%d,%ld,%ld,%ld,%ld,%.3f,%.3f,%.3f,%.3f\n",
           windowN, lossProb, errorProb, schemeName(scheme), total, framesTransmitted,
           framesTransmitted - total, chStats.framesDropped, chStats.framesCorrupted,
           elapsedMs, avgRttMs, throughputBps, efficiencyPct);

    close(fd);
    return 0;
}
