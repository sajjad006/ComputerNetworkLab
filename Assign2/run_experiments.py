#!/usr/bin/env python3
"""
Experiment A - window-size effect with a clean channel (loss=error=0):
    shows the throughput/efficiency gain from pipelining (GBN/SR) over
    Stop-and-Wait as the window grows, and reports the average
    frame-send -> ACK-received time (RTT) for each.

Experiment B - efficiency vs. channel impairment probability p in
    [0.1, 0.5]: p is split into p/2 chance of loss and p/2 chance of a
    bit error per transmission (applied independently to the data frame
    and to its ACK), so the *combined* chance something goes wrong on a
    given frame's round trip is close to p without the two probabilities
    compounding into an unrealistically harsh channel at p=0.5. Window
    is fixed at N=4 for GBN/SR so Go-Back-N's whole-window retransmit
    cost stays tractable at high loss.

Usage: python3 run_experiments.py
Requires: `make` already run in this directory (binaries sw_sender, ...).
"""
import csv
import os
import shlex
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
RESULTS_DIR = os.path.join(HERE, "results")
os.makedirs(RESULTS_DIR, exist_ok=True)

BIG_FILE = os.path.join(HERE, "testfile.txt")     # ~33.5 KB -> 730 frames
SMALL_FILE = os.path.join(HERE, "medium.txt")      # 3 KB -> 66 frames

RUN_TIMEOUT_S = 90
SETTLE_S = 0.3

FIELDS = ["protocol", "window", "loss_prob", "error_prob", "fcs", "total_frames",
          "tx_attempts", "retransmissions", "dropped", "corrupted",
          "elapsed_ms", "avg_rtt_ms", "throughput_Bps", "efficiency_pct", "correct"]


def run_once(protocol, input_file, window, loss_p, error_p, scheme="crc32"):
    tag = f"{protocol}_w{window}_l{loss_p:.2f}_e{error_p:.2f}"
    out_file = os.path.join(RESULTS_DIR, f"rx_{tag}.txt")
    rx_log = os.path.join(RESULTS_DIR, f"log_{tag}_rx.txt")
    tx_log = os.path.join(RESULTS_DIR, f"log_{tag}_tx.txt")

    if protocol == "stop_and_wait":
        rx_cmd = [f"./sw_receiver", out_file, str(loss_p), str(error_p), scheme]
        tx_cmd = [f"./sw_sender", input_file, str(loss_p), str(error_p), scheme]
    elif protocol == "go_back_n":
        rx_cmd = [f"./gbn_receiver", out_file, str(loss_p), str(error_p), scheme]
        tx_cmd = [f"./gbn_sender", input_file, str(window), str(loss_p), str(error_p), scheme]
    elif protocol == "selective_repeat":
        rx_cmd = [f"./sr_receiver", out_file, str(window), str(loss_p), str(error_p), scheme]
        tx_cmd = [f"./sr_sender", input_file, str(window), str(loss_p), str(error_p), scheme]
    else:
        raise ValueError(protocol)

    print(f"  -> {tag} ... ", end="", flush=True)
    with open(rx_log, "w") as rxlog:
        rx_proc = subprocess.Popen(rx_cmd, cwd=HERE, stdout=rxlog, stderr=subprocess.STDOUT)
    time.sleep(SETTLE_S)

    result_row = None
    try:
        with open(tx_log, "w") as txlog:
            tx_proc = subprocess.run(tx_cmd, cwd=HERE, stdout=subprocess.PIPE,
                                      stderr=txlog, timeout=RUN_TIMEOUT_S, text=True)
        for line in tx_proc.stdout.splitlines():
            if line.startswith("RESULT,"):
                parts = line.strip().split(",")[1:]
                result_row = dict(zip(FIELDS[:-1], parts))
    except subprocess.TimeoutExpired:
        print("TIMEOUT")
        rx_proc.kill()
        return None

    try:
        rx_proc.wait(timeout=10)
    except subprocess.TimeoutExpired:
        rx_proc.kill()

    if result_row is None:
        print("NO RESULT LINE (check logs)")
        return None

    with open(input_file, "rb") as a:
        original = a.read()
    correct = os.path.exists(out_file)
    if correct:
        with open(out_file, "rb") as b:
            correct = (b.read() == original)
    result_row["correct"] = "OK" if correct else "MISMATCH"
    print(f"ok  eff={result_row['efficiency_pct']}%  rtt={result_row['avg_rtt_ms']}ms  "
          f"time={result_row['elapsed_ms']}ms  [{result_row['correct']}]")
    return result_row


def write_csv(path, rows):
    with open(path, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=FIELDS)
        w.writeheader()
        for r in rows:
            w.writerow(r)


def experiment_a():
    print("\n=== Experiment A: window size effect, clean channel (loss=error=0) ===")
    print(f"Input file: {BIG_FILE}")
    rows = []
    rows.append(run_once("stop_and_wait", BIG_FILE, 1, 0.0, 0.0))
    for N in [1, 2, 4, 8, 16]:
        rows.append(run_once("go_back_n", BIG_FILE, N, 0.0, 0.0))
    for N in [1, 2, 4, 8, 16]:
        rows.append(run_once("selective_repeat", BIG_FILE, N, 0.0, 0.0))
    rows = [r for r in rows if r]
    write_csv(os.path.join(RESULTS_DIR, "experiment_A_window_effect.csv"), rows)
    return rows


def experiment_b():
    print("\n=== Experiment B: efficiency vs. channel impairment probability p in [0.1,0.5] ===")
    print(f"Input file: {SMALL_FILE}  (window N=4 for GBN/SR; p split p/2 loss + p/2 error)")
    rows = []
    for p in [0.0, 0.1, 0.2, 0.3, 0.4, 0.5]:
        half = p / 2.0
        rows.append(run_once("stop_and_wait", SMALL_FILE, 1, half, half))
    for p in [0.0, 0.1, 0.2, 0.3, 0.4, 0.5]:
        half = p / 2.0
        rows.append(run_once("go_back_n", SMALL_FILE, 4, half, half))
    for p in [0.0, 0.1, 0.2, 0.3, 0.4, 0.5]:
        half = p / 2.0
        rows.append(run_once("selective_repeat", SMALL_FILE, 4, half, half))
    rows = [r for r in rows if r]
    write_csv(os.path.join(RESULTS_DIR, "experiment_B_loss_sweep.csv"), rows)
    return rows


def print_table(rows, cols):
    widths = {c: max(len(c), max((len(str(r.get(c, ""))) for r in rows), default=0)) for c in cols}
    header = "  ".join(c.ljust(widths[c]) for c in cols)
    print(header)
    print("-" * len(header))
    for r in rows:
        print("  ".join(str(r.get(c, "")).ljust(widths[c]) for c in cols))


if __name__ == "__main__":
    for exe in ["sw_sender", "sw_receiver", "gbn_sender", "gbn_receiver", "sr_sender", "sr_receiver"]:
        if not os.path.exists(os.path.join(HERE, exe)):
            print(f"Missing {exe} -- run `make` first.")
            sys.exit(1)

    a_rows = experiment_a()
    b_rows = experiment_b()

    print("\n\n################ SUMMARY: Experiment A (window effect, no loss) ################")
    print_table(a_rows, ["protocol", "window", "total_frames", "elapsed_ms", "avg_rtt_ms",
                          "throughput_Bps", "efficiency_pct", "correct"])

    print("\n\n################ SUMMARY: Experiment B (loss/error sweep) ################")
    print_table(b_rows, ["protocol", "window", "loss_prob", "error_prob", "retransmissions",
                          "elapsed_ms", "avg_rtt_ms", "throughput_Bps", "efficiency_pct", "correct"])

    print(f"\nCSV written to {RESULTS_DIR}/experiment_A_window_effect.csv and experiment_B_loss_sweep.csv")
