#!/usr/bin/env python3
"""Automated CDMA experiment harness for Assignment 4.

For each configuration, launches channel_server + N sender + N receiver
processes, waits for completion, parses the server's RESULT line, and
byte-compares every receiver's output file with its sender's input file.
Three sweeps are run:

  Experiment A: Walsh codes, noiseless channel, N swept -- correctness
                (every file reconstructed exactly) and how code length,
                slot count and channel efficiency scale with N.
  Experiment B: Walsh codes under additive Gaussian chip noise -- BER vs.
                noise sigma for several code lengths L (N = L, full load),
                plus BER vs. N at fixed L and sigma.
  Experiment C: Walsh vs. random (non-orthogonal) +/-1 codes, L fixed,
                noiseless, N swept -- isolates multiple-access interference.

Results are written to results/expA_correctness.csv, results/expB_noise.csv,
results/expB2_noise_vs_n.csv and results/expC_walsh_vs_random.csv.
"""
import subprocess
import time
import os
import csv
import random
import shutil
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
SERVER = os.path.join(HERE, "channel_server")
SENDER = os.path.join(HERE, "sender")
RECEIVER = os.path.join(HERE, "receiver")
RESULTS = os.path.join(HERE, "results")
os.makedirs(RESULTS, exist_ok=True)

_next_port = [9600]

TEXT = ("Code Division Multiple Access lets every station transmit at the same time "
        "on the same channel; each bit is spread over an orthogonal Walsh chip "
        "sequence and recovered by correlation at the receiver. ")


def text_payload(i, nbytes):
    s = f"[station {i}] " + TEXT * (nbytes // len(TEXT) + 1)
    return s[:nbytes].encode()


def random_payload(i, nbytes, seed):
    rng = random.Random(seed * 1000 + i)
    return bytes(rng.getrandbits(8) for _ in range(nbytes))


def run_once(N, payloads, code_type="walsh", code_len=0, sigma=0.0, seed=1, timeout=120):
    port = _next_port[0]
    _next_port[0] += 1
    work = tempfile.mkdtemp(prefix="cdma_")
    try:
        ins, outs = [], []
        for i, data in enumerate(payloads):
            pin, pout = os.path.join(work, f"in{i}.bin"), os.path.join(work, f"out{i}.bin")
            with open(pin, "wb") as f:
                f.write(data)
            ins.append(pin)
            outs.append(pout)

        t0 = time.time()
        srv = subprocess.Popen(
            [SERVER, str(port), str(N), code_type, str(code_len), str(sigma), str(seed)],
            stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True)
        time.sleep(0.25)

        procs = []
        for i in range(N):
            procs.append(subprocess.Popen([RECEIVER, "127.0.0.1", str(port), str(i), outs[i]],
                                          stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL))
            procs.append(subprocess.Popen([SENDER, "127.0.0.1", str(port), str(i), ins[i]],
                                          stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL))
        try:
            out, _ = srv.communicate(timeout=timeout)
        except subprocess.TimeoutExpired:
            srv.kill()
            for p in procs:
                p.kill()
            raise RuntimeError(f"TIMEOUT N={N} code={code_type} sigma={sigma}")
        for p in procs:
            try:
                p.wait(timeout=5)
            except subprocess.TimeoutExpired:
                p.kill()
        elapsed = time.time() - t0

        line = next((l for l in out.splitlines() if l.startswith("RESULT")), None)
        if not line:
            raise RuntimeError(f"no RESULT line: N={N} code={code_type} sigma={sigma} out={out!r}")

        files_ok = 0
        for pin, pout in zip(ins, outs):
            with open(pin, "rb") as a, open(pout, "rb") as b:
                files_ok += a.read() == b.read()
    finally:
        shutil.rmtree(work, ignore_errors=True)

    f = line.split(",")
    return {
        "code": f[1], "N": int(f[2]), "L": int(f[3]), "sigma": float(f[4]), "seed": seed,
        "slots": int(f[5]), "data_bits": int(f[6]), "bit_errors": int(f[7]),
        "silence_errors": int(f[8]), "ber": float(f[9]), "bits_per_chip": float(f[10]),
        "files_ok": files_ok, "elapsed_s": round(elapsed, 3),
    }


def write_csv(name, rows):
    with open(os.path.join(RESULTS, name), "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        w.writeheader()
        w.writerows(rows)


def main():
    # ---------------- Experiment A: correctness + scaling, Walsh, noiseless ----------------
    # Stations carry text messages of different lengths, so shorter senders go
    # silent while longer ones are still transmitting (exercises the 0 level).
    N_values = [1, 2, 3, 4, 5, 6, 8, 10, 12, 16, 20, 24, 32, 48, 64]
    print("=== Experiment A: Walsh codes, noiseless, sweeping N ===")
    rowsA = []
    for N in N_values:
        payloads = [text_payload(i, 200 + 25 * i) for i in range(N)]
        r = run_once(N, payloads)
        rowsA.append(r)
        print(f"  N={N:<3} L={r['L']:<3} slots={r['slots']:<5} bits={r['data_bits']:<6} "
              f"errors={r['bit_errors']+r['silence_errors']:<3} files_ok={r['files_ok']}/{N} "
              f"bits/chip={r['bits_per_chip']:.3f} t={r['elapsed_s']}s")
    write_csv("expA_correctness.csv", rowsA)

    # ---------------- Experiment B: BER vs noise sigma, several L (N = L) ----------------
    L_values = [4, 8, 16, 32, 64]
    sigmas = [0.0, 0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 2.0, 2.5, 3.0, 4.0]
    print("\n=== Experiment B: Walsh codes, Gaussian chip noise, sweeping sigma for each L (N = L) ===")
    rowsB = []
    for L in L_values:
        for s in sigmas:
            payloads = [random_payload(i, 500, seed=L) for i in range(L)]
            r = run_once(L, payloads, sigma=s, seed=L * 100 + int(s * 100))
            rowsB.append(r)
            print(f"  L={L:<3} sigma={s:<5} bits={r['data_bits']:<7} errors={r['bit_errors']:<6} "
                  f"BER={r['ber']:.6f}")
    write_csv("expB_noise.csv", rowsB)

    # ---------------- Experiment B2: BER vs N at fixed L and sigma ----------------
    L_FIXED, SIGMA_FIXED = 16, 1.5
    print(f"\n=== Experiment B2: Walsh codes, L={L_FIXED}, sigma={SIGMA_FIXED}, sweeping N ===")
    rowsB2 = []
    for N in [1, 2, 4, 6, 8, 10, 12, 14, 16]:
        payloads = [random_payload(i, 1000, seed=7) for i in range(N)]
        r = run_once(N, payloads, code_len=L_FIXED, sigma=SIGMA_FIXED, seed=500 + N)
        rowsB2.append(r)
        print(f"  N={N:<3} bits={r['data_bits']:<7} errors={r['bit_errors']:<5} BER={r['ber']:.6f}")
    write_csv("expB2_noise_vs_n.csv", rowsB2)

    # ---------------- Experiment C: Walsh vs random codes, noiseless ----------------
    L_C = 16
    print(f"\n=== Experiment C: Walsh vs random codes, L={L_C}, noiseless, sweeping N ===")
    rowsC = []
    for N in [1, 2, 4, 6, 8, 10, 12, 14, 16]:
        payloads = [random_payload(i, 250, seed=11) for i in range(N)]
        r = run_once(N, payloads, code_type="walsh", code_len=L_C)
        rowsC.append(r)
        print(f"  walsh  N={N:<3} BER={r['ber']:.6f} files_ok={r['files_ok']}/{N}")
        for seed in range(1, 6):
            r = run_once(N, payloads, code_type="random", code_len=L_C, seed=seed)
            rowsC.append(r)
            print(f"  random N={N:<3} seed={seed} BER={r['ber']:.6f} files_ok={r['files_ok']}/{N}")
    write_csv("expC_walsh_vs_random.csv", rowsC)

    print("\nCSV written to results/expA_correctness.csv, results/expB_noise.csv,")
    print("               results/expB2_noise_vs_n.csv, results/expC_walsh_vs_random.csv")


if __name__ == "__main__":
    main()
