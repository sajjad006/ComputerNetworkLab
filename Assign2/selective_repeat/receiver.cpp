// Selective-Repeat ARQ receiver. Receive window = N: buffers correctly
// received frames even if out of order, individually ACKs every frame it
// accepts (in-order or not), discards only corrupted frames, and delivers
// to the output file as soon as a contiguous run from the window base
// becomes available.
// usage: ./sr_receiver <output_file> <window_N> <ack_loss_prob> <ack_error_prob> [fcs_scheme=crc32]
#include "../common/common.h"
#include "../common/frame.h"
#include "../common/channel.h"
#include "../common/socket_util.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <cstdio>
#include <chrono>
using namespace std;
using namespace std::chrono;

static const string SRC_MAC = "F8CA12E9B0AA";
static const string DST_MAC = "CA29C8F10AB6";


static int resolveIndex(int deliverIdx, int windowN, int total, unsigned char seq) {
    int lo = max(0, deliverIdx - windowN);
    int hi = min(total, deliverIdx + windowN);
    for (int i = lo; i < hi; i++)
        if ((unsigned char)(i & 0xFF) == seq) return i;
    return -1;
}

int main(int argc, char** argv) {
    if (argc < 5) {
        fprintf(stderr, "usage: %s <output_file> <window_N> <ack_loss_prob> <ack_error_prob> [fcs_scheme=crc32]\n", argv[0]);
        return 1;
    }
    string outputFile = argv[1];
    int windowN = atoi(argv[2]);
    double lossProb  = atof(argv[3]);
    double errorProb = atof(argv[4]);
    FCS scheme = (argc >= 6) ? parseScheme(argv[5]) : CRC32;
    if (windowN < 1) windowN = 1;
    if (windowN > 128) windowN = 128;

    seedChannelRng();
    int fd = makeUdpSocket(SR_RECEIVER_PORT);
    sockaddr_in senderAddr = loopbackAddr(SR_SENDER_PORT);
    ChannelConfig cfg{lossProb, errorProb, 1, 15};
    ChannelStats  chStats{};

    fprintf(stderr, "[SR-receiver] waiting for setup...\n");
    unsigned total = recvSetup(fd);
    fprintf(stderr, "[SR-receiver] expecting %u frames, N=%d, scheme=%s\n", total, windowN, schemeName(scheme));

    ofstream out(outputFile, ios::binary);
    vector<bool> got(total, false);
    vector<DataFrame> buf(total);
    int deliverIdx = 0;
    unsigned corrupted = 0, newFrames = 0, duplicates = 0, ignored = 0;
    size_t dataFrameSize = DataFrame::make(SRC_MAC, DST_MAC, "", 0, scheme).toBytes().size();

    while (deliverIdx < (int)total) {
        char raw[512];
        sockaddr_in from{}; socklen_t flen = sizeof(from);
        ssize_t n = recvfrom(fd, raw, sizeof(raw), 0, (sockaddr*)&from, &flen);
        if (n != (ssize_t)dataFrameSize) continue;   // ignore stray/malformed datagrams

        DataFrame f = DataFrame::fromBytes(string(raw, n), scheme);
        if (!f.verify(scheme)) { corrupted++; continue; }   // corrupted -> discard, no ACK

        int idx = resolveIndex(deliverIdx, windowN, (int)total, f.seq);
        if (idx < 0) { ignored++; continue; }                // outside both windows: stray/garbled seq

        AckFrame a = AckFrame::make(DST_MAC, SRC_MAC, f.seq, scheme);
        channelSend(fd, a.toBytes(), senderAddr, cfg, chStats);   // ACK every accepted frame, in-order or not

        if (idx < deliverIdx) { duplicates++; continue; }     // already delivered; ACK re-sent above, nothing else to do

        if (!got[idx]) {
            got[idx] = true;
            buf[idx] = f;
            newFrames++;
        } else {
            duplicates++;
        }

        while (deliverIdx < (int)total && got[deliverIdx]) {
            out.write(buf[deliverIdx].payload.data(), buf[deliverIdx].length);
            deliverIdx++;
        }
    }
    out.close();

    setRecvTimeoutMs(fd, 300);
    auto drainStart = steady_clock::now();
    int idleStreak = 0;
    while (idleStreak < 3 && duration_cast<seconds>(steady_clock::now() - drainStart).count() < 5) {
        char raw[512];
        sockaddr_in from{}; socklen_t flen = sizeof(from);
        ssize_t n = recvfrom(fd, raw, sizeof(raw), 0, (sockaddr*)&from, &flen);
        if (n != (ssize_t)dataFrameSize) { idleStreak++; continue; }
        DataFrame f = DataFrame::fromBytes(string(raw, n), scheme);
        idleStreak = 0;
        if (!f.verify(scheme)) continue;
        int idx = resolveIndex(deliverIdx, windowN, (int)total, f.seq);
        if (idx < 0) continue;
        AckFrame a = AckFrame::make(DST_MAC, SRC_MAC, f.seq, scheme);
        channelSend(fd, a.toBytes(), senderAddr, cfg, chStats);
    }

    fprintf(stderr,
        "[SR-receiver] done. delivered=%d new=%u duplicates=%u corrupted_discarded=%u ignored=%u "
        "acks_dropped=%ld acks_corrupted=%ld\n",
        deliverIdx, newFrames, duplicates, corrupted, ignored, chStats.framesDropped, chStats.framesCorrupted);

    close(fd);
    return 0;
}
