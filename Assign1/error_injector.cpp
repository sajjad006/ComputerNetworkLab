#include "error_injector.h"
#include <cstdlib>
#include <vector>

using namespace std;

// flip one bit at absolute position p (bit 0 = MSB of byte 0)
static void flipBit(string& frame, int p) {
    frame[p / 8] ^= (1 << (7 - p % 8));
}

// error_injector.cpp — add this function:
string injectCrcMultiple(string frame, vector<int> gen) {
    int n = gen.size() - 1;                 // degree
    int genBytesSpan = (n / 8) + 2;         // generator spans at most this many bytes
    // choose a byte offset where the whole generator pattern fits inside the frame
    int maxStart = (int)frame.size() - genBytesSpan;
    if (maxStart < 0) return frame;         // frame too small (won't happen at 46B)
    int startByte = rand() % (maxStart + 1);

    // XOR the generator bits into the frame starting at bit (startByte*8)
    int startBit = startByte * 8;
    for (int j = 0; j <= n; j++) {
        if (gen[j] == 1) {
            int p = startBit + j;
            frame[p / 8] ^= (1 << (7 - p % 8));
        }
    }
    return frame;
}

string injectError(string frame, ErrorType type) {
    int totalBits = frame.size() * 8;

    switch (type) {

        case SINGLE_BIT: {
            int p = rand() % totalBits;
            flipBit(frame, p);
            break;
        }
        case TWO_ISOLATED: {
            int p1 = rand() % totalBits;
            int p2=p1;

            while (p1==p2) {
                p2=rand()%totalBits;
            }

            flipBit(frame, p1);
            flipBit(frame, p2);
            break;
        }
        case ODD_ERRORS: {
            // Maximum odd number <= totalBits
            int maxOdd = (totalBits % 2 == 1) ? totalBits : totalBits - 1;

            // If only one bit exists, flip it
            if (maxOdd <= 1) {
                flipBit(frame, 0);
                break;
            }

            int numErrors = 1 + 2 * (rand() % ((maxOdd + 1) / 2));
            vector<int> positions;

            while (static_cast<int>(positions.size()) < numErrors) {

                int p = rand() % totalBits;
                bool alreadySelected = false;

                for (int existing : positions) {
                    if (existing == p) {
                        alreadySelected = true;
                        break;
                    }
                }

                if (!alreadySelected) {
                    positions.push_back(p);
                    flipBit(frame, p);
                }
            }

            break;
        }

        case BURST: {

            int maxBurstLength = min(8, totalBits);
            int minBurstLength = (totalBits >= 2) ? 2 : 1;

            int L;

            if (minBurstLength == maxBurstLength) {
                L = minBurstLength;
            } else {
                L = minBurstLength +
                    rand() % (maxBurstLength - minBurstLength + 1);
            }

            int start = rand() % (totalBits - L + 1);

            for (int i = 0; i < L; i++) {
                flipBit(frame, start + i);
            }

            break;
        }
        case WORD_SWAP: {
            // frame layout: 6 src | 6 dst | 2 len | 2 seq | payload | fcs
            int payloadStart = 16;                 // first payload byte
            int payloadBytes = 46;       // 46
            // need at least two whole 16-bit words to swap
            if (payloadBytes >= 4) {
                // pick two DIFFERENT 16-bit word slots inside the payload
                int words = payloadBytes / 2;      // number of full words
                int w1 = rand() % words;
                int w2 = rand() % words;
                while (w2 == w1) w2 = rand() % words;
                int a = payloadStart + w1 * 2;     // byte offset of word 1
                int b = payloadStart + w2 * 2;     // byte offset of word 2
                swap(frame[a],   frame[b]);        // swap high bytes
                swap(frame[a+1], frame[b+1]);      // swap low bytes
            }
            break;
        }
        case CRC_MULTIPLE: {
            // gen is provided via the overload below; this plain case is a no-op
            // guard so the enum is exhaustive. Use injectCrcMultiple(...) instead.
            break;
        }
    }
    return frame;
}
