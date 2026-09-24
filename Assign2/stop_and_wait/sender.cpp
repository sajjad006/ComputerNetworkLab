// Stop-and-Wait ARQ sender.
// usage: ./sw_sender <input_file> <loss_prob> <error_prob> [fcs_scheme=crc32]
#include "../common/common.h"
#include "../common/frame.h"
#include "../common/channel.h"
#include "../common/rtt_timer.h"
#include "../common/socket_util.h"
#include <iostream>
#include <chrono>
#include <cstdio>
using namespace std;
using namespace std::chrono;

static const string SRC_MAC = "F8CA12E9B0AA";
static const string DST_MAC = "CA29C8F10AB6";

int main(int argc, char** argv) {
    if (argc < 4) {
        fprintf(stderr, "usage: %s <input_file> <loss_prob> <error_prob> [fcs_scheme=crc32]\n", argv[0]);
        return 1;
    }
    string inputFile = argv[1];
    double lossProb  = atof(argv[2]);
    double errorProb = atof(argv[3]);
    FCS scheme = (argc >= 5) ? parseScheme(argv[4]) : CRC32;

    seedChannelRng();
    vector<DataFrame> frames = framing(inputFile, scheme, SRC_MAC, DST_MAC);
    if (frames.empty()) { fprintf(stderr, "no frames to send (empty/missing file)\n"); return 1; }

    int fd = makeUdpSocket(SW_SENDER_PORT);
    sockaddr_in dst = loopbackAddr(SW_RECEIVER_PORT);
    ChannelConfig cfg{lossProb, errorProb, 1, 15};
    ChannelStats  chStats{};
    RttTimer timer;

    sendSetup(fd, dst, (unsigned)frames.size());

    long framesTransmitted = 0;
    double sumRttMs = 0; int rttSamples = 0;
    size_t ackFrameSize = AckFrame::make(DST_MAC, SRC_MAC, 0, scheme).toBytes().size();

    auto t0 = high_resolution_clock::now();

    for (auto& f : frames) {
        bool acked = false;
        bool isRetransmission = false;
        while (!acked) {
            string bytes = f.toBytes();
            auto sendTime = high_resolution_clock::now();
            channelSend(fd, bytes, dst, cfg, chStats);
            framesTransmitted++;

            setRecvTimeoutMs(fd, timer.timeoutMs());
            char buf[256];
            sockaddr_in from{}; socklen_t flen = sizeof(from);
            ssize_t n = recvfrom(fd, buf, sizeof(buf), 0, (sockaddr*)&from, &flen);

            if (n == (ssize_t)ackFrameSize) {
                AckFrame a = AckFrame::fromBytes(string(buf, n), scheme);
                if (a.verify(scheme) && a.ackno == f.seq) {
                    acked = true;
                    if (!isRetransmission) {
                        double rttMs = duration_cast<duration<double, milli>>(
                                           high_resolution_clock::now() - sendTime).count();
                        timer.onSample(rttMs);
                        sumRttMs += rttMs; rttSamples++;
                    }
                    continue;
                }
            }
            // timeout, or corrupted/mismatched ACK -> retransmit
            isRetransmission = true;
        }
    }

    auto t1 = high_resolution_clock::now();
    double elapsedMs = duration_cast<duration<double, milli>>(t1 - t0).count();

    long fileBytes = 0;
    for (auto& f : frames) fileBytes += f.length;
    double throughputBps = (elapsedMs > 0) ? fileBytes / (elapsedMs / 1000.0) : 0;
    double efficiencyPct = 100.0 * frames.size() / framesTransmitted;
    double avgRttMs = rttSamples ? sumRttMs / rttSamples : 0;

    fprintf(stderr,
        "[SW-sender] frames=%zu tx_attempts=%ld retransmissions=%ld dropped=%ld corrupted=%ld "
        "elapsed_ms=%.2f avg_rtt_ms=%.2f throughput_Bps=%.1f efficiency=%.2f%%\n",
        frames.size(), framesTransmitted, framesTransmitted - (long)frames.size(),
        chStats.framesDropped, chStats.framesCorrupted, elapsedMs, avgRttMs, throughputBps, efficiencyPct);

    printf("RESULT,stop_and_wait,1,%.2f,%.2f,%s,%zu,%ld,%ld,%ld,%ld,%.3f,%.3f,%.3f,%.3f\n",
           lossProb, errorProb, schemeName(scheme), frames.size(), framesTransmitted,
           framesTransmitted - (long)frames.size(), chStats.framesDropped, chStats.framesCorrupted,
           elapsedMs, avgRttMs, throughputBps, efficiencyPct);

    close(fd);
    return 0;
}
