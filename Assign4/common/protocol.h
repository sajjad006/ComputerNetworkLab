#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <cstring>

// Wire protocol between channel_server, sender and receiver, over TCP,
// newline-delimited text messages (simulation coordination traffic, not the
// modeled radio channel itself).
//
// Registration (station -> server), first line on every connection:
//   "REGISTER SENDER <id>\n"   or   "REGISTER RECEIVER <id>\n"
// Server -> station, reply to REGISTER:
//   "CODE <id> <L> c0 c1 ... c(L-1)\n"       (chip sequence, each +1 / -1)
//
// Once per bit-slot (lock-step):
//   Server   -> Sender   : "SLOT <k>\n"
//   Sender   -> Server   : "CHIPS <k> x0 ... x(L-1)\n"   x = d * c, d in {+1,-1}
//                          "IDLE <k>\n"                    no data left (all chips 0)
//   Server   -> Receiver : "SIGNAL <k> s0 ... s(L-1)\n"  chip-wise sum of all senders (+ noise)
//   Receiver -> Server   : "DECODED <k> <1|0|->\n"        '-' = sender silent
// End of run (every sender IDLE in the same slot):
//   Server -> all        : "DONE\n"

enum CodeType { CODE_WALSH, CODE_RANDOM };

inline const char* codeTypeName(CodeType t) {
    return t == CODE_WALSH ? "walsh" : "random";
}

inline CodeType parseCodeTypeArg(const char* s) {
    if (!strcmp(s, "random")) return CODE_RANDOM;
    return CODE_WALSH;
}

// Ternary symbol carried by one bit-slot: bit 1 -> +1, bit 0 -> -1, no data -> 0.
const int SYM_SILENT = 0;

#endif
