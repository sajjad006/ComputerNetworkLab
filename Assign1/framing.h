#ifndef FRAMING_H
#define FRAMING_H
#include "common.h"
#include <vector>
#include <string>
using namespace std;
vector<Frame> framing(string file_name, FCS fcs_type, int payload_size,
                      string src_mac, string dst_mac);
#endif
