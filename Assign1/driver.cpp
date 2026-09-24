// driver — runs 5000 trials per (scheme, error-type), measures detection
// rate + timing, writes results.csv, and prints formatted tables to stdout.
#include "common.h"
#include "error_injector.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <cstdio>
using namespace std;
using namespace std::chrono;

static const int TRIALS = 5000;

static void line(int w = 76) { for (int i = 0; i < w; i++) putchar('-'); putchar('\n'); }
static void dline(int w = 76) { for (int i = 0; i < w; i++) putchar('='); putchar('\n'); }
static void banner(const char* title) {
    putchar('\n'); dline();
    printf("  %s\n", title);
    dline();
}

// driver.cpp — add near the top (mirror YOUR filled-in generators):
static vector<int> genFor(FCS s) {
    switch (s) {
        case CRC8:  return {1,1,1,0,1,0,1,0,1};              // your CRC-8 (model)
        case CRC10: return { /* your 11 bits */ };
        case CRC16: return { /* your 17 bits */ };
        case CRC32: return { /* your 33 bits */ };
        default:    return {};                               // checksum: no generator
    }
}

int main() {
    srand(time(nullptr));

    FCS    schemes[]    = { CHECKSUM16, CRC8, CRC10, CRC16, CRC32 };
    const char* sName[] = { "CHECKSUM16","CRC8","CRC10","CRC16","CRC32" };
    // ErrorType errs[]    = { SINGLE_BIT, TWO_ISOLATED, ODD_ERRORS, BURST };
    // const char* eName[] = { "single-bit","two-isolated","odd-errors","burst" };

    ErrorType errs[] = { SINGLE_BIT, TWO_ISOLATED, ODD_ERRORS, BURST, WORD_SWAP, CRC_MULTIPLE };
    const char* errName[] = { "single","two_isolated","odd","burst","word_swap","crc_multiple" };

    string sample = "The quick brown fox jumps over the lazy dog 12345";

    // store timing for summary
    double timing[5][4] = {};

    ofstream csv("results.csv");
    csv << "scheme,error_type,trials,detected,missed,detect_rate,total_ms\n";

    // ================================================================
    //  SECTION 1 — Detection Rate per Scheme & Error Type
    // ================================================================
    banner("CSE/PC/B/S/314 — Error Detection Evaluation   (trials = 5000)");
    printf("\n  %-12s  %-13s  %8s  %6s  %9s  %10s\n",
           "Scheme", "Error Type", "Detected", "Missed", "Rate (%)", "Time (ms)");
    line();

    for (int s = 0; s < 5; s++) {
        for (int e = 0; e < 4; e++) {
            int detected = 0, missed = 0;
            auto t0 = high_resolution_clock::now();

            for (int t = 0; t < TRIALS; t++) {
                Frame f("F8CA12E9B0AA", "CA29C8F10AB6", sample,
                        schemes[s], t & 0xFFFF, PAYLOAD_SIZE);
                string clean     = f.toBinary();
                // string corrupted = injectError(clean, errs[e]);
                string corrupted;
                if (errs[e] == CRC_MULTIPLE) {
                    vector<int> g = genFor(schemes[s]);
                    if (g.empty())               // checksum has no generator:
                        corrupted = injectError(clean, BURST);  // fall back so the row still runs
                    else
                        corrupted = injectCrcMultiple(clean, g);
                } else {
                    corrupted = injectError(clean, errs[e]);
                }

                Frame r = Frame::fromBytes(corrupted, schemes[s], PAYLOAD_SIZE);
                bool changed = (corrupted != clean);
                bool passes  = verifyFCS(r, schemes[s]);

                if (changed && !passes) detected++;
                else                    missed++;
            }

            auto t1 = high_resolution_clock::now();
            double ms = duration_cast<microseconds>(t1 - t0).count() / 1000.0;
            double rate = (detected + missed) ? (100.0 * detected / (detected + missed)) : 0.0;
            timing[s][e] = ms;

            csv << sName[s] << "," << eName[e] << "," << TRIALS << ","
                << detected << "," << missed << "," << rate << "," << ms << "\n";

            printf("  %-12s  %-13s  %8d  %6d  %8.2f%%  %10.2f\n",
                   sName[s], eName[e], detected, missed, rate, ms);
        }
        if (s < 4) line();
    }
    dline();
    csv.close();

    // ================================================================
    //  SECTION 2 — Cross-Comparison: Checksum-16 vs each CRC scheme
    //  Same bit-error pattern applied to both scheme frames so the
    //  four categories (both / chk-only / crc-only / neither) are fair.
    // ================================================================
    FCS    crcList[]    = { CRC8,  CRC10,  CRC16,  CRC32  };
    const char* cName[] = { "CRC8","CRC10","CRC16","CRC32" };

    banner("Cross-Comparison: Checksum-16 vs CRC  (same errors, same frames)");
    printf("  Categories: [Both] = both schemes detect  [Chk] = checksum only\n");
    printf("              [CRC]  = CRC only              [Miss] = neither detects\n");

    for (int c = 0; c < 4; c++) {
        printf("\n  CHECKSUM-16  vs  %s\n", cName[c]);
        printf("  %-13s  %10s  %10s  %10s  %10s\n",
               "Error Type", "Both", "Chk Only", "CRC Only", "Missed");
        line(60);

        for (int e = 0; e < 4; e++) {
            int both = 0, chkOnly = 0, crcOnly = 0, neither = 0;

            for (int t = 0; t < TRIALS; t++) {
                Frame fChk("F8CA12E9B0AA","CA29C8F10AB6", sample, CHECKSUM16,   t & 0xFFFF, PAYLOAD_SIZE);
                Frame fCRC("F8CA12E9B0AA","CA29C8F10AB6", sample, crcList[c],   t & 0xFFFF, PAYLOAD_SIZE);

                string cleanChk = fChk.toBinary();
                string cleanCRC = fCRC.toBinary();
                string corrChk  = injectError(cleanChk, errs[e]);

                // apply the exact same bit-flip mask to the CRC frame
                string corrCRC = cleanCRC;
                for (size_t i = 0; i < corrChk.size() && i < cleanChk.size(); i++)
                    corrCRC[i] ^= (corrChk[i] ^ cleanChk[i]);

                if (corrChk == cleanChk) { neither++; continue; }

                Frame rChk = Frame::fromBytes(corrChk, CHECKSUM16, PAYLOAD_SIZE);
                Frame rCRC = Frame::fromBytes(corrCRC, crcList[c], PAYLOAD_SIZE);

                bool chkDet = !verifyFCS(rChk, CHECKSUM16);
                bool crcDet = !verifyFCS(rCRC, crcList[c]);

                if      ( chkDet &&  crcDet) both++;
                else if ( chkDet && !crcDet) chkOnly++;
                else if (!chkDet &&  crcDet) crcOnly++;
                else                          neither++;
            }

            printf("  %-13s  %10d  %10d  %10d  %10d\n",
                   eName[e], both, chkOnly, crcOnly, neither);
        }
    }
    dline();

    // ================================================================
    //  SECTION 3 — Timing Summary (avg ms per trial, across error types)
    // ================================================================
    banner("Timing Summary — Average Time per 5000-Trial Run (ms)");
    printf("\n  %-12s  %14s  %14s  %14s  %14s\n",
           "Scheme", "single-bit", "two-isolated", "odd-errors", "burst");
    line();
    for (int s = 0; s < 5; s++) {
        printf("  %-12s  %14.2f  %14.2f  %14.2f  %14.2f\n",
               sName[s], timing[s][0], timing[s][1], timing[s][2], timing[s][3]);
    }
    dline();

    printf("\nWrote results.csv\n\n");
    return 0;
}
