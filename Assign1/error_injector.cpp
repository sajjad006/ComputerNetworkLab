#include "error_injector.h"
#include <cstdlib>
#include <vector>

using namespace std;

// flip one bit at absolute position p (bit 0 = MSB of byte 0)
static void flipBit(string& frame, int p) {
    frame[p / 8] ^= (1 << (7 - p % 8));
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
    }
    return frame;
}
