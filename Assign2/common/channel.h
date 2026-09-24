#ifndef CHANNEL_H
#define CHANNEL_H

#include <string>
#include <netinet/in.h>
using namespace std;

// Simulated "wire" impairments applied by Channel() before a frame (data
// or ACK) actually leaves this process via the socket.
struct ChannelConfig {
    double lossProb  = 0.0;   // P(frame never arrives -> sender times out)
    double errorProb = 0.0;   // P(frame arrives with a bit error -> FCS fails)
    int minDelayMs   = 1;     // propagation delay range, always applied
    int maxDelayMs   = 15;
};

struct ChannelStats {
    long framesOffered = 0;
    long framesDropped = 0;
    long framesCorrupted = 0;
    long bytesOnWire = 0;
};

// Applies random propagation delay, then with probability lossProb drops
// the frame silently (nothing sent), else with probability errorProb
// flips bits in it before sending. Returns true if something was put on
// the wire (dropped == false).
bool channelSend(int sockfd, string bytes, const sockaddr_in& to,
                  const ChannelConfig& cfg, ChannelStats& stats);

void seedChannelRng();

#endif
