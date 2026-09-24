// Stop-and-Wait ARQ receiver.
// usage: ./sw_receiver <output_file> <ack_loss_prob> <ack_error_prob> [fcs_scheme=crc32]
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
    int fd = makeUdpSocket(SW_RECEIVER_PORT);
    sockaddr_in senderAddr = loopbackAddr(SW_SENDER_PORT);
    ChannelConfig cfg{lossProb, errorProb, 1, 15};   // impairs the ACK channel
    ChannelStats  chStats{};

    fprintf(stderr, "[SW-receiver] waiting for setup...\n");
    unsigned total = recvSetup(fd);
    fprintf(stderr, "[SW-receiver] expecting %u frames, scheme=%s\n", total, schemeName(scheme));

    ofstream out(outputFile, ios::binary);
    unsigned char expected = 0;
    bool haveLastAck = false;
    unsigned char lastAckedSeq = 0;
    unsigned accepted = 0, corrupted = 0, duplicates = 0, unexpected = 0;
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
            AckFrame a = AckFrame::make(DST_MAC, SRC_MAC, f.seq, scheme);
            channelSend(fd, a.toBytes(), senderAddr, cfg, chStats);
            lastAckedSeq = f.seq; haveLastAck = true;
            expected = (unsigned char)((expected + 1) % 256);
        } else if (haveLastAck && f.seq == lastAckedSeq) {
            duplicates++;   // our ACK was lost; resend it
            AckFrame a = AckFrame::make(DST_MAC, SRC_MAC, f.seq, scheme);
            channelSend(fd, a.toBytes(), senderAddr, cfg, chStats);
        } else {
            unexpected++;
        }
    }
    out.close();

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
        if (!f.verify(scheme) || !haveLastAck || f.seq != lastAckedSeq) continue;
        AckFrame a = AckFrame::make(DST_MAC, SRC_MAC, f.seq, scheme);
        channelSend(fd, a.toBytes(), senderAddr, cfg, chStats);
    }

    fprintf(stderr,
        "[SW-receiver] done. accepted=%u corrupted_discarded=%u duplicates=%u unexpected=%u "
        "acks_dropped=%ld acks_corrupted=%ld\n",
        accepted, corrupted, duplicates, unexpected, chStats.framesDropped, chStats.framesCorrupted);

    close(fd);
    return 0;
}
