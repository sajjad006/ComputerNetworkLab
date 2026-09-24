#ifndef ERROR_INJECTOR_H
#define ERROR_INJECTOR_H

#include <string>
using namespace std;

enum ErrorType { SINGLE_BIT, TWO_ISOLATED, ODD_ERRORS, BURST };

// Flips bit(s) in `frame` according to `type`. Returns the corrupted copy.
string injectError(string frame, ErrorType type);

#endif
