#ifndef WALSH_H
#define WALSH_H

#include <vector>
#include <random>
using namespace std;

// Smallest power of two >= n (Walsh codes exist only for lengths 2^k).
inline int nextPow2(int n) {
    int L = 1;
    while (L < n) L <<= 1;
    return L;
}

// Sylvester construction of the L x L Walsh (Hadamard) matrix:
//   W1 = [+1],   W2n = [ Wn  Wn ]
//                      [ Wn -Wn ]
// Every pair of distinct rows is orthogonal: sum_j W[a][j]*W[b][j] = 0 (a != b), = L (a == b).
inline vector<vector<int>> walshMatrix(int L) {
    vector<vector<int>> W(1, vector<int>(1, 1));
    for (int n = 1; n < L; n <<= 1) {
        vector<vector<int>> next(2 * n, vector<int>(2 * n));
        for (int r = 0; r < n; r++)
            for (int c = 0; c < n; c++) {
                next[r][c]         =  W[r][c];
                next[r][c + n]     =  W[r][c];
                next[r + n][c]     =  W[r][c];
                next[r + n][c + n] = -W[r][c];
            }
        W = next;
    }
    return W;
}

// N independent +/-1 chip sequences of length L: a non-orthogonal baseline,
// used only to show what Walsh orthogonality buys.
inline vector<vector<int>> randomCodes(int N, int L, unsigned seed) {
    mt19937 rng(seed);
    bernoulli_distribution coin(0.5);
    vector<vector<int>> C(N, vector<int>(L));
    for (auto& row : C)
        for (int& c : row) c = coin(rng) ? 1 : -1;
    return C;
}

#endif
