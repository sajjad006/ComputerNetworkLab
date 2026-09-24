#include "rtt_timer.h"
#include <algorithm>

RttTimer::RttTimer(double initialTimeoutMs)
    : estRtt_(initialTimeoutMs / 2.0), devRtt_(initialTimeoutMs / 4.0), timeout_(initialTimeoutMs) {}

void RttTimer::onSample(double sampleRttMs) {
    if (!haveSample_) {
        estRtt_ = sampleRttMs;
        devRtt_ = sampleRttMs / 2.0;
        haveSample_ = true;
    } else {
        devRtt_ = (1 - BETA) * devRtt_ + BETA * std::abs(sampleRttMs - estRtt_);
        estRtt_ = (1 - ALPHA) * estRtt_ + ALPHA * sampleRttMs;
    }
    timeout_ = estRtt_ + 4 * devRtt_;
    timeout_ = std::max(MIN_TIMEOUT_MS, std::min(MAX_TIMEOUT_MS, timeout_));
}
