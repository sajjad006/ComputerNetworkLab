#ifndef ERROR_INJECTOR_H
#define ERROR_INJECTOR_H

#include <string>
using namespace std;

enum ErrorType { SINGLE_BIT, TWO_ISOLATED, ODD_ERRORS, BURST };

string injectError(string frame, ErrorType type);

#endif
