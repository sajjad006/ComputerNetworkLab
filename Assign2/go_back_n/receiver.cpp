// Go-Back-N ARQ receiver. Receive window = 1: only accepts the next
// in-order frame; corrupted or out-of-order frames are discarded.
// usage: ./gbn_receiver <output_file> <ack_loss_prob> <ack_error_prob> [fcs_scheme=crc32]
#include "../common/common.h"
#include "../common/frame.h"
#include "../common/channel.h"
#include "../common/socket_util.h"
#include <iostream>
#include <fstream>
#include <cstdio>
#include <chrono>
using namespace std;
using namespace std::chrono;

static const string SRC_MAC = "F8CA12E9B0AA";
static const string DST_MAC = "CA29C8F10AB6";

int main(int argc, char** argv) {
    if (argc < 4) {
        fprintf(stderr, "usage: %s <output_file> <ack_loss_prob> <ack_error_prob> [fcs_scheme=crc32]\n", argv[0]);
        return 1;
    }
    string outputFile = argv[1];
    double lossProb  = atof(argv[2]);
    double errorProb = atof(argv[3]);
    FCS scheme = (argc >= 5) ? parseScheme(argv[4]) : CRC32;

    seedChannelRng();
    int fd = makeUdpSocket(GBN_RECEIVER_PORT);
    sockaddr_in senderAddr = loopbackAddr(GBN_SENDER_PORT);
    ChannelConfig cfg{lossProb, errorProb, 1, 15};
    ChannelStats  chStats{};

    fprintf(stderr, "[GBN-receiver] waiting for setup...\n");
    unsigned total = recvSetup(fd);
    fprintf(stderr, "[GBN-receiver] expecting %u frames, scheme=%s\n", total, schemeName(scheme));

    ofstream out(outputFile, ios::binary);
    unsigned char expected = 0;
    unsigned accepted = 0, corrupted = 0, outOfOrder = 0;
    bool haveLastAck = false; unsigned char lastAckedSeq = 0;
    size_t dataFrameSize = DataFrame::make(SRC_MAC, DST_MAC, "", 0, scheme).toBytes().size();

    while (accepted < total) {
        char buf[512];
        sockaddr_in from{}; socklen_t flen = sizeof(from);
        ssize_t n = recvfrom(fd, buf, sizeof(buf), 0, (sockaddr*)&from, &flen);
        if (n != (ssize_t)dataFrameSize) continue;   // ignore stray/malformed datagrams

        DataFrame f = DataFrame::fromBytes(string(buf, n), scheme);
        if (!f.verify(scheme)) { corrupted++; continue; }   // corrupted -> discard, no ACK

        if (f.seq == expected) {
            out.write(f.payload.data(), f.length);
            accepted++;
            lastAckedSeq = f.seq; haveLastAck = true;
            AckFrame a = AckFrame::make(DST_MAC, SRC_MAC, f.seq, scheme);   // cumulative ACK
            channelSend(fd, a.toBytes(), senderAddr, cfg, chStats);
            expected = (unsigned char)((expected + 1) % 256);
        } else {
            outOfOrder++;   // discard; re-issue last cumulative ACK so a lost ACK doesn't
                             // needlessly stall the sender until the full window times out
            if (haveLastAck) {
                AckFrame a = AckFrame::make(DST_MAC, SRC_MAC, lastAckedSeq, scheme);
                channelSend(fd, a.toBytes(), senderAddr, cfg, chStats);
            }
        }
    }
    out.close();

    // Drain: keep re-issuing the final cumulative ACK for a short grace
    // period in case the sender's copy of it was lost, so it doesn't
    // retransmit the last window into silence forever.
    setRecvTimeoutMs(fd, 300);
    auto drainStart = steady_clock::now();
    int idleStreak = 0;
    while (idleStreak < 3 && duration_cast<seconds>(steady_clock::now() - drainStart).count() < 5) {
        char buf[512];
        sockaddr_in from{}; socklen_t flen = sizeof(from);
        ssize_t n = recvfrom(fd, buf, sizeof(buf), 0, (sockaddr*)&from, &flen);
        if (n != (ssize_t)dataFrameSize) { idleStreak++; continue; }
        DataFrame f = DataFrame::fromBytes(string(buf, n), scheme);
        idleStreak = 0;
        if (!f.verify(scheme) || !haveLastAck) continue;
        AckFrame a = AckFrame::make(DST_MAC, SRC_MAC, lastAckedSeq, scheme);
        channelSend(fd, a.toBytes(), senderAddr, cfg, chStats);
    }

    fprintf(stderr,
        "[GBN-receiver] done. accepted=%u corrupted_discarded=%u out_of_order_discarded=%u "
        "acks_dropped=%ld acks_corrupted=%ld\n",
        accepted, corrupted, outOfOrder, chStats.framesDropped, chStats.framesCorrupted);

    close(fd);
    return 0;
}
