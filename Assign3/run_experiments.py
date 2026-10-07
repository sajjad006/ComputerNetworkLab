#!/usr/bin/env python3
"""Automated CSMA experiment harness for Assignment 3.

For each configuration, launches channel_server + N station processes,
waits for completion, and parses the server's RESULT line. Two sweeps are
run, matching the assignment's required test cases:

  Experiment A: p-persistent CSMA, fixed N, p swept across a range.
  Experiment B: all four schemes (non-persistent, 1-persistent,
                p-persistent at the best p found in A, CSMA/CD),
                swept across N (number of contending stations).

Results are written to results/expA_p_sweep.csv and results/expB_n_sweep.csv.
"""
import subprocess
import time
import os
import sys
import csv

HERE = os.path.dirname(os.path.abspath(__file__))
SERVER = os.path.join(HERE, "channel_server")
STATION = os.path.join(HERE, "station")
RESULTS = os.path.join(HERE, "results")
os.makedirs(RESULTS, exist_ok=True)

FRAME_SLOTS = 5      # frame transmission time, in slots (a = 1/frameSlots = 0.2)
JAM_SLOTS = 1         # CSMA/CD collision-detection window, in slots
BACKOFF_CAP = 10      # binary exponential backoff cap (IEEE 802.3-style)
MAX_SLOTS = 20000     # simulation horizon, held constant for fair comparison

_next_port = [9500]

def run_once(scheme, N, p, max_slots=MAX_SLOTS, timeout=60):
    port = _next_port[0]
    _next_port[0] += 1

    srv = subprocess.Popen(
        [SERVER, str(port), str(N), scheme, str(p), str(FRAME_SLOTS), str(JAM_SLOTS), str(max_slots)],
        stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True)
    time.sleep(0.25)

    stations = []
    for _ in range(N):
        st = subprocess.Popen(
            [STATION, "127.0.0.1", str(port), scheme, str(p), str(BACKOFF_CAP)],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        stations.append(st)

    try:
        out, _ = srv.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        srv.kill()
        for st in stations:
            st.kill()
        raise RuntimeError(f"TIMEOUT scheme={scheme} N={N} p={p}")

    for st in stations:
        try:
            st.wait(timeout=5)
        except subprocess.TimeoutExpired:
            st.kill()

    line = next((l for l in out.splitlines() if l.startswith("RESULT")), None)
    if not line:
        raise RuntimeError(f"no RESULT line: scheme={scheme} N={N} p={p} out={out!r}")

    f = line.split(",")
    return {
        "scheme": f[1], "N": int(f[2]), "p": float(f[3]),
        "frame_slots": int(f[4]), "jam_slots": int(f[5]), "max_slots": int(f[6]),
        "successes": int(f[7]), "collisions": int(f[8]), "idle_slots": int(f[9]),
        "avg_delay_slots": float(f[10]), "throughput": float(f[11]),
    }


def main():
    # ---------------- Experiment A: p-persistent, sweep p, fixed N ----------------
    N_FIXED = 15
    p_values = [0.05, 0.1, 0.15, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0]

    print(f"=== Experiment A: p-persistent CSMA, N={N_FIXED}, sweeping p ===")
    rowsA = []
    for p in p_values:
        r = run_once("p_persistent", N_FIXED, p)
        rowsA.append(r)
        print(f"  p={p:<5} collisions={r['collisions']:<6} avg_delay={r['avg_delay_slots']:<9.2f} "
              f"throughput={r['throughput']:.4f}")

    best = max(rowsA, key=lambda r: r["throughput"])
    best_p = best["p"]
    print(f"\nBest p by throughput: p={best_p} (throughput={best['throughput']:.4f})\n")

    with open(os.path.join(RESULTS, "expA_p_sweep.csv"), "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rowsA[0].keys()))
        w.writeheader()
        w.writerows(rowsA)

    # ---------------- Experiment B: all schemes, sweep N ----------------
    N_values = [2, 5, 10, 15, 20, 30, 40, 60, 80, 100]
    schemes = [
        ("non_persistent", 0.0),
        ("one_persistent", 0.0),
        ("p_persistent", best_p),
        ("csma_cd", 0.0),
    ]

    print(f"=== Experiment B: all schemes, sweeping N (p-persistent uses p={best_p}) ===")
    rowsB = []
    for scheme, p in schemes:
        for N in N_values:
            r = run_once(scheme, N, p)
            rowsB.append(r)
            print(f"  {scheme:<15} N={N:<4} collisions={r['collisions']:<6} "
                  f"avg_delay={r['avg_delay_slots']:<9.2f} throughput={r['throughput']:.4f}")

    with open(os.path.join(RESULTS, "expB_n_sweep.csv"), "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rowsB[0].keys()))
        w.writeheader()
        w.writerows(rowsB)

    print("\nCSV written to results/expA_p_sweep.csv and results/expB_n_sweep.csv")


if __name__ == "__main__":
    main()
