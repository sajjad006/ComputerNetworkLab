#ifndef RTT_TIMER_H
#define RTT_TIMER_H

// Adaptive retransmission timer (Jacobson/Karels, as used by TCP):
//   EstimatedRTT = (1-a)*EstimatedRTT + a*SampleRTT
//   DevRTT       = (1-b)*DevRTT       + b*|SampleRTT - EstimatedRTT|
//   Timeout      = EstimatedRTT + 4*DevRTT
// Implements the Sender's Timer()/Timeout() methods: Timeout() folds a
// fresh RTT sample into the estimate and recomputes the timeout value.
class RttTimer {
public:
    explicit RttTimer(double initialTimeoutMs = 200.0);

    // Call with the measured RTT (ms) of a frame that was NOT retransmitted
    // (Karn's algorithm: ambiguous samples from retransmitted frames are
    // never fed into the estimator).
    void onSample(double sampleRttMs);

    double timeoutMs() const { return timeout_; }
    double estimatedRttMs() const { return estRtt_; }

private:
    double estRtt_;
    double devRtt_;
    double timeout_;
    bool   haveSample_ = false;
    static constexpr double ALPHA = 0.125;
    static constexpr double BETA  = 0.25;
    static constexpr double MIN_TIMEOUT_MS = 20.0;
    static constexpr double MAX_TIMEOUT_MS = 2000.0;
};

#endif
