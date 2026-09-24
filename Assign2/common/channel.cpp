#include "channel.h"
#include "error_injector.h"
#include <sys/socket.h>
#include <unistd.h>
#include <cstdlib>
#include <ctime>

static double frand() { return rand() / (double)RAND_MAX; }

void seedChannelRng() {
    srand((unsigned)time(nullptr) ^ (unsigned)getpid());
}

bool channelSend(int sockfd, string bytes, const sockaddr_in& to,
                  const ChannelConfig& cfg, ChannelStats& stats) {
    stats.framesOffered++;

    int delay = cfg.minDelayMs;
    if (cfg.maxDelayMs > cfg.minDelayMs)
        delay += rand() % (cfg.maxDelayMs - cfg.minDelayMs + 1);
    if (delay > 0) usleep(delay * 1000);

    if (frand() < cfg.lossProb) {
        stats.framesDropped++;
        return false;   // simulated loss / excessive delay -> never arrives
    }

    if (frand() < cfg.errorProb) {
        bytes = injectError(bytes, SINGLE_BIT);
        stats.framesCorrupted++;
    }

    stats.bytesOnWire += (long)bytes.size();
    sendto(sockfd, bytes.data(), bytes.size(), 0, (const sockaddr*)&to, sizeof(to));
    return true;
}
