// usage: ./channel_server <port> <N> <walsh|random> <codeLen> <noiseSigma> <seed> [demoDelayMs]
//   codeLen 0 = smallest power of two >= N.
//   demoDelayMs > 0 prints a live per-slot trace and paces the loop for a lab demo.
#include "common/protocol.h"
#include "common/net_util.h"
#include "common/walsh.h"
#include <vector>
#include <random>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
using namespace std;

static int dot(const vector<int>& a, const vector<int>& b) {
    int s = 0;
    for (size_t j = 0; j < a.size(); j++) s += a[j] * b[j];
    return s;
}

static string codeLine(int id, const vector<int>& code) {
    string s = "CODE " + to_string(id) + " " + to_string(code.size());
    for (int c : code) s += c > 0 ? " 1" : " -1";
    return s;
}

static char symChar(int sym) { return sym > 0 ? '1' : (sym < 0 ? '0' : '-'); }

int main(int argc, char** argv) {
    if (argc < 7) {
        fprintf(stderr, "usage: %s <port> <N> <walsh|random> <codeLen> <noiseSigma> <seed> [demoDelayMs]\n", argv[0]);
        return 1;
    }
    int port = atoi(argv[1]);
    int N = atoi(argv[2]);
    CodeType codeType = parseCodeTypeArg(argv[3]);
    int L = atoi(argv[4]);
    double sigma = atof(argv[5]);
    unsigned seed = (unsigned)atol(argv[6]);
    int demoDelayMs = (argc >= 8) ? atoi(argv[7]) : 0;
    bool demo = demoDelayMs > 0;

    if (L <= 0) L = nextPow2(N);
    vector<vector<int>> codes;
    if (codeType == CODE_WALSH) {
        if (L < N || nextPow2(L) != L) {
            fprintf(stderr, "[channel_server] Walsh codes need a power-of-two length >= N (got L=%d, N=%d)\n", L, N);
            return 1;
        }
        auto W = walshMatrix(L);
        codes.assign(W.begin(), W.begin() + N);
    } else {
        codes = randomCodes(N, L, seed);
    }

    int listenFd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    if (bind(listenFd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "[channel_server] bind failed on port %d\n", port);
        return 1;
    }
    listen(listenFd, 2 * N + 4);
    fprintf(stderr, "[channel_server] listening on %d, waiting for %d senders + %d receivers (L=%d, %s codes)...\n",
            port, N, N, L, codeTypeName(codeType));

    vector<LineSocket*> senders(N, nullptr), receivers(N, nullptr);
    int registered = 0;
    while (registered < 2 * N) {
        int fd = accept(listenFd, nullptr, nullptr);
        setNoDelay(fd);
        auto* ls = new LineSocket(fd);
        string line;
        char role[16] = "";
        int id = -1;
        if (!ls->readLine(line) || sscanf(line.c_str(), "REGISTER %15s %d", role, &id) != 2 ||
            id < 0 || id >= N) {
            fprintf(stderr, "[channel_server] bad registration '%s', dropping\n", line.c_str());
            close(fd); delete ls; continue;
        }
        auto& slot = (!strcmp(role, "SENDER")) ? senders : receivers;
        if (slot[id]) {
            fprintf(stderr, "[channel_server] duplicate %s %d, dropping\n", role, id);
            close(fd); delete ls; continue;
        }
        slot[id] = ls;
        ls->writeLine(codeLine(id, codes[id]));
        registered++;
    }
    close(listenFd);
    fprintf(stderr, "[channel_server] all stations connected; starting transmission\n");

    if (demo) {
        printf("Chip sequences (L=%d):\n", L);
        for (int i = 0; i < N; i++) {
            printf("  station %2d: [", i);
            for (int c : codes[i]) printf("%+d", c);
            printf("]\n");
        }
        printf("\n");
    }

    mt19937 rng(seed * 7919u + 17u);
    normal_distribution<double> noise(0.0, sigma > 0 ? sigma : 1.0);

    long slots = 0, dataBits = 0, bitErrors = 0, silenceErrors = 0;
    vector<long> stationBits(N, 0), stationErrors(N, 0);
    vector<int> truth(N);
    vector<int> chips(L);
    vector<double> signal(L);
    char buf[64];

    for (long k = 0; ; k++) {
        snprintf(buf, sizeof(buf), "SLOT %ld", k);
        for (auto* s : senders) s->writeLine(buf);

        fill(signal.begin(), signal.end(), 0.0);
        int active = 0;
        for (int i = 0; i < N; i++) {
            string line;
            truth[i] = SYM_SILENT;
            if (!senders[i]->readLine(line) || line.rfind("CHIPS", 0) != 0) continue;
            const char* p = line.c_str() + 5;
            strtol(p, (char**)&p, 10);   // slot number
            for (int j = 0; j < L; j++) {
                chips[j] = (int)strtol(p, (char**)&p, 10);
                signal[j] += chips[j];
            }
            int corr = dot(chips, codes[i]);
            truth[i] = corr > 0 ? 1 : (corr < 0 ? -1 : SYM_SILENT);
            if (truth[i] != SYM_SILENT) active++;
        }
        if (active == 0) break;   // every sender has finished
        slots++;

        if (sigma > 0)
            for (double& s : signal) s += noise(rng);

        string sigLine = "SIGNAL " + to_string(k);
        for (double s : signal) {
            snprintf(buf, sizeof(buf), " %.6g", s);
            sigLine += buf;
        }
        for (auto* r : receivers) r->writeLine(sigLine);

        string decodedStr;
        for (int i = 0; i < N; i++) {
            string line;
            int sym = SYM_SILENT;
            char c = '-';
            if (receivers[i]->readLine(line) && sscanf(line.c_str(), "DECODED %*s %c", &c) == 1)
                sym = (c == '1') ? 1 : (c == '0' ? -1 : SYM_SILENT);
            decodedStr += symChar(sym);

            if (truth[i] != SYM_SILENT) {
                dataBits++; stationBits[i]++;
                if (sym != truth[i]) { bitErrors++; stationErrors[i]++; }
            } else if (sym != SYM_SILENT) {
                silenceErrors++; stationErrors[i]++;
            }
        }

        if (demo) {
            printf("slot %4ld  sent=", k);
            for (int i = 0; i < N; i++) putchar(symChar(truth[i]));
            printf("  channel=[");
            for (int j = 0; j < L; j++) printf(sigma > 0 ? "%+.2f " : "%+.0f ", signal[j]);
            bool anyError = false;
            for (int i = 0; i < N; i++) anyError |= symChar(truth[i]) != decodedStr[i];
            printf("]  decoded=%s%s\n", decodedStr.c_str(), anyError ? "  <-- ERROR" : "");
            fflush(stdout);
            usleep(demoDelayMs * 1000);
        }
    }

    for (auto* s : senders)   { s->writeLine("DONE"); close(s->fd()); delete s; }
    for (auto* r : receivers) { r->writeLine("DONE"); close(r->fd()); delete r; }

    double ber = dataBits ? (double)bitErrors / dataBits : 0.0;
    double bitsPerChip = slots ? (double)dataBits / ((double)slots * L) : 0.0;

    for (int i = 0; i < N; i++)
        fprintf(stderr, "[channel_server] station %d: bits=%ld errors=%ld\n", i, stationBits[i], stationErrors[i]);
    fprintf(stderr,
        "[channel_server] done. slots=%ld data_bits=%ld bit_errors=%ld silence_errors=%ld BER=%.6f bits/chip=%.4f\n",
        slots, dataBits, bitErrors, silenceErrors, ber, bitsPerChip);

    printf("RESULT,%s,%d,%d,%.3f,%ld,%ld,%ld,%ld,%.6f,%.4f\n",
           codeTypeName(codeType), N, L, sigma, slots, dataBits, bitErrors, silenceErrors, ber, bitsPerChip);
    return 0;
}
