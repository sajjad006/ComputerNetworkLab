#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <cstring>

// Wire protocol between channel_server and station, over TCP, newline-delimited
// text messages (simulation coordination traffic, not the modeled channel itself).
//
// Server -> Station, once per slot (lock-step):
//   "SLOT <slot_no> <IDLE|BUSY|COLLISION>\n"
//     The channel state sensed by every station at the start of this slot.
// Server -> Station, asynchronously when that station's own in-flight frame resolves:
//   "OUTCOME <SUCCESS|COLLISION>\n"
// Server -> Station, when the run ends:
//   "DONE\n"
//
// Station -> Server, once per slot, in reply to SLOT:
//   "ACT <slot_no> <TRANSMIT|WAIT>\n"

enum ChannelState { CH_IDLE, CH_BUSY, CH_COLLISION };
enum StationAction { ACT_TRANSMIT, ACT_WAIT };
enum Outcome { OUT_SUCCESS, OUT_COLLISION };

enum CsmaScheme { NON_PERSISTENT, ONE_PERSISTENT, P_PERSISTENT, CSMA_CD };

inline const char* schemeName(CsmaScheme s) {
    switch (s) {
        case NON_PERSISTENT: return "non_persistent";
        case ONE_PERSISTENT: return "one_persistent";
        case P_PERSISTENT:   return "p_persistent";
        case CSMA_CD:         return "csma_cd";
    }
    return "?";
}

inline CsmaScheme parseSchemeArg(const char* s) {
    if (!strcmp(s, "non_persistent")) return NON_PERSISTENT;
    if (!strcmp(s, "one_persistent")) return ONE_PERSISTENT;
    if (!strcmp(s, "p_persistent"))   return P_PERSISTENT;
    if (!strcmp(s, "csma_cd"))         return CSMA_CD;
    return ONE_PERSISTENT;
}

#endif
