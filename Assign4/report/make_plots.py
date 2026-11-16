#!/usr/bin/env python3
"""Generates every figure for the Assignment 4 report.
Run from the report/ directory: python3 make_plots.py
"""
import os
import math
import numpy as np
import pandas as pd
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, FancyArrowPatch

HERE = os.path.dirname(os.path.abspath(__file__))
FIG = os.path.join(HERE, "figures")
RESULTS = os.path.join(HERE, "..", "results")
os.makedirs(FIG, exist_ok=True)

plt.rcParams.update({
    "font.size": 10,
    "axes.titlesize": 11,
    "axes.titleweight": "bold",
    "figure.dpi": 150,
    "savefig.dpi": 150,
    "savefig.bbox": "tight",
})

L_COLOR = {4: "#1f77b4", 8: "#ff7f0e", 16: "#2ca02c", 32: "#d62728", 64: "#9467bd"}
L_MARK = {4: "o", 8: "s", 16: "^", 32: "D", 64: "v"}


def Q(x):
    return 0.5 * math.erfc(x / math.sqrt(2))


def walsh(L):
    W = np.array([[1]])
    while W.shape[0] < L:
        W = np.block([[W, W], [W, -W]])
    return W

# ---------------------------------------------------------------------------
# 1. Architecture / procedural diagram
# ---------------------------------------------------------------------------
def box(ax, xy, w, h, text, fc, fs=9, ec="black"):
    b = FancyBboxPatch(xy, w, h, boxstyle="round,pad=0.02,rounding_size=0.06",
                        facecolor=fc, edgecolor=ec, linewidth=1.1)
    ax.add_patch(b)
    ax.text(xy[0] + w / 2, xy[1] + h / 2, text, ha="center", va="center", fontsize=fs, wrap=True)

def arrow(ax, p1, p2, color="black", style="-|>", lw=1.3):
    ax.add_patch(FancyArrowPatch(p1, p2, arrowstyle=style, color=color, lw=lw, mutation_scale=12))

fig, ax = plt.subplots(figsize=(10.5, 6.6))
ax.set_xlim(0, 10.5)
ax.set_ylim(0, 9.2)
ax.axis("off")
ax.set_title("CDMA Simulation — Lock-Step Bit-Slot Protocol", fontsize=13, weight="bold")

blue, green, yellow, gray, red = "#cfe3f7", "#d4f0d4", "#fbeec6", "#e6e6e6", "#f7cfc7"

for k, (y, lbl) in enumerate([(7.4, "sender 0\ncode $c_0$"), (5.6, "sender 1\ncode $c_1$"),
                              (3.8, "sender N-1\ncode $c_{N-1}$")]):
    box(ax, (0.3, y), 2.1, 1.0, lbl, blue)
    arrow(ax, (2.4, y + 0.5), (4.0, 5.35), color="#333")
ax.text(1.35, 5.1, "⋮", ha="center", fontsize=16)

box(ax, (4.0, 4.5), 2.6, 1.7,
    "channel_server\n$S[j] = \\sum_i d_i\\,c_i[j] + n_j$\n(chip-wise sum + noise)\ncounts bit errors", red)

for k, (y, lbl) in enumerate([(7.4, "receiver 0\n$v=\\frac{1}{L}S\\cdot c_0$"),
                              (5.6, "receiver 1\n$v=\\frac{1}{L}S\\cdot c_1$"),
                              (3.8, "receiver N-1\n$v=\\frac{1}{L}S\\cdot c_{N-1}$")]):
    box(ax, (8.1, y), 2.1, 1.0, lbl, green)
    arrow(ax, (6.6, 5.35), (8.1, y + 0.5), color="#333")
ax.text(9.15, 5.1, "⋮", ha="center", fontsize=16)

ax.text(5.3, 2.55,
        "each bit-slot k:\n  SERVER   --SLOT k-->                       every sender\n"
        "  SENDER   --CHIPS k x0..x(L-1) | IDLE k--> SERVER\n"
        "  SERVER   --SIGNAL k S0..S(L-1)-->        every receiver\n"
        "  RECEIVER --DECODED k 1|0|- -->            SERVER",
        ha="center", multialignment="left", fontsize=8.5, family="monospace",
        bbox=dict(boxstyle="round", fc="#fdfdfd", ec="#999"))

box(ax, (0.3, 0.3), 9.9, 1.4,
    "Sender: bit 1 → d=+1, bit 0 → d=−1, no data → d=0;  sends chips  x = d·c_i  (L chips)\n"
    "Receiver: v = (1/L)·Σ S[j]·c_i[j]   →   v > +0.5: bit 1   |   v < −0.5: bit 0   |   otherwise: sender silent\n"
    "Walsh codes are mutually orthogonal, so every other sender's term cancels exactly: v = d_i + noise of std σ/√L.",
    yellow, fs=8.6)

fig.savefig(os.path.join(FIG, "architecture.png"))
plt.close(fig)

# ---------------------------------------------------------------------------
# 2. Walsh code matrix + cross-correlation (Walsh vs random)
# ---------------------------------------------------------------------------
L = 8
W = walsh(L)
rng = np.random.default_rng(3)
R = rng.choice([-1, 1], size=(L, L))

fig, axes = plt.subplots(1, 3, figsize=(11.5, 3.8))
ax = axes[0]
ax.imshow(W, cmap="RdBu", vmin=-1.6, vmax=1.6)
for r in range(L):
    for c in range(L):
        ax.text(c, r, "+" if W[r, c] > 0 else "−", ha="center", va="center", fontsize=9, weight="bold")
ax.set_title(f"Walsh codes $W_{{{L}}}$ (rows = stations)")
ax.set_xlabel("chip index j")
ax.set_ylabel("station i")

for ax, M, title in [(axes[1], W @ W.T, "Walsh: $c_a\\cdot c_b$"),
                     (axes[2], R @ R.T, "Random ±1 codes: $c_a\\cdot c_b$")]:
    ax.imshow(np.abs(M), cmap="Greys", vmin=0, vmax=L * 1.4)
    for r in range(L):
        for c in range(L):
            v = M[r, c]
            ax.text(c, r, str(v), ha="center", va="center", fontsize=8,
                    color="white" if abs(v) >= L * 0.8 else ("#c0392b" if (v != 0 and r != c) else "black"))
    ax.set_title(title)
    ax.set_xlabel("station b")
    ax.set_ylabel("station a")
fig.tight_layout()
fig.savefig(os.path.join(FIG, "walsh_codes.png"))
plt.close(fig)

# ---------------------------------------------------------------------------
# 3. Spreading / despreading worked example (N=4, L=4), slot 2 of the demo
# ---------------------------------------------------------------------------
W4 = walsh(4)
d = np.array([-1, -1, -1, +1])          # bits sent: 0 0 0 1
S = (d[:, None] * W4).sum(axis=0)       # composite channel signal

fig, axes = plt.subplots(6, 1, figsize=(8.2, 7.6))
t = np.arange(5)
for i in range(4):
    ax = axes[i]
    chips = d[i] * W4[i]
    ax.step(t, np.append(chips, chips[-1]), where="post", color=L_COLOR[[4, 8, 16, 32][i]], lw=2)
    ax.axhline(0, color="#999", lw=0.6)
    ax.set_ylim(-1.6, 1.6)
    ax.set_yticks([-1, 1])
    ax.set_ylabel(f"stn {i}", rotation=0, labelpad=22, va="center")
    code = "".join("+" if c > 0 else "−" for c in W4[i])
    ax.text(4.05, 0, f"bit {int(d[i] > 0)} → d={d[i]:+d}\ncode [{code}]", va="center", fontsize=8.5)
ax = axes[4]
ax.step(t, np.append(S, S[-1]), where="post", color="black", lw=2.2)
ax.axhline(0, color="#999", lw=0.6)
ax.set_ylim(-4.6, 4.6)
ax.set_yticks([-4, -2, 0, 2, 4])
ax.set_ylabel("channel\nΣ", rotation=0, labelpad=22, va="center")
ax.text(4.05, 0, f"S = {list(S)}", va="center", fontsize=8.5)
ax = axes[5]
ax.axis("off")
lines = [f"receiver {i}: (1/4)·S·c{i} = {S @ W4[i] / 4:+.0f} → bit {int(S @ W4[i] > 0)}" for i in range(4)]
ax.text(0.0, 0.5, "\n".join(lines), family="monospace", fontsize=9, va="center")
axes[0].set_title("Spreading and despreading one bit-slot (N=4, L=4): bits 0,0,0,1", loc="left")
axes[4].set_xlabel("chip index j")
for a in axes[:5]:
    a.set_xlim(0, 4)
    a.set_xticks(range(5))
fig.tight_layout()
fig.savefig(os.path.join(FIG, "spreading_example.png"))
plt.close(fig)

# ---------------------------------------------------------------------------
# 4. Experiment A: correctness + scaling (Walsh, noiseless)
# ---------------------------------------------------------------------------
A = pd.read_csv(os.path.join(RESULTS, "expA_correctness.csv")).sort_values("N")

fig, axes = plt.subplots(1, 2, figsize=(11, 4.0))
ax = axes[0]
ax.step(A["N"], A["L"], where="post", color="#555", lw=1.5)
ax.plot(A["N"], A["L"], "o", color="#555", label="code length L = 2^⌈log₂N⌉")
ax.plot(A["N"], A["N"], ":", color="#999", label="L = N (lower bound)")
ax.set_xlabel("Number of stations N")
ax.set_ylabel("Code length L (chips per bit)")
ax.set_title("Experiment A — Walsh code length vs. N")
ax.grid(alpha=0.3)
ax.legend(fontsize=8.5)
ax = axes[1]
ax.plot(A["N"], A["N"] / A["L"], "s--", color="#1f77b4", lw=1.5, ms=5, label="N/L  (every station busy)")
ax.plot(A["N"], A["bits_per_chip"], "^-", color="#d62728", lw=1.8, ms=6,
        label="measured (unequal message lengths)")
ax.set_ylim(0, 1.1)
ax.set_xlabel("Number of stations N")
ax.set_ylabel("Channel efficiency (data bits / chip)")
ax.set_title("Experiment A — Channel efficiency vs. N")
ax.grid(alpha=0.3)
ax.legend(fontsize=8.5, loc="lower left")
fig.tight_layout()
fig.savefig(os.path.join(FIG, "expA_efficiency.png"))
plt.close(fig)

fig, ax = plt.subplots(figsize=(6.2, 4.0))
ax.plot(A["N"], A["elapsed_s"], "o-", color="#2ca02c", lw=1.8, ms=6)
ax.set_xlabel("Number of stations N")
ax.set_ylabel("Wall-clock run time (s)")
ax.set_title("Experiment A — Simulation run time vs. N")
ax.grid(alpha=0.3)
fig.savefig(os.path.join(FIG, "expA_runtime.png"))
plt.close(fig)

# ---------------------------------------------------------------------------
# 5. Experiment B: BER vs sigma for each L, with theory Q(0.5*sqrt(L)/sigma)
# ---------------------------------------------------------------------------
B = pd.read_csv(os.path.join(RESULTS, "expB_noise.csv"))
sig_fine = np.linspace(0.05, 4.0, 300)

fig, ax = plt.subplots(figsize=(6.8, 4.6))
for Lv in sorted(B["L"].unique()):
    sub = B[(B.L == Lv) & (B.ber > 0)].sort_values("sigma")
    ax.plot(sub["sigma"], sub["ber"], L_MARK[Lv], color=L_COLOR[Lv], ms=6, label=f"L = N = {Lv} (measured)")
    ax.plot(sig_fine, [Q(0.5 * math.sqrt(Lv) / s) for s in sig_fine], "-", color=L_COLOR[Lv], lw=1.2, alpha=0.8)
ax.plot([], [], "-", color="#555", label="theory  Q(0.5·√L / σ)")
ax.set_yscale("log")
ax.set_ylim(1e-5, 1)
ax.set_xlabel("Per-chip noise standard deviation σ")
ax.set_ylabel("Bit error rate (BER)")
ax.set_title("Experiment B — BER vs. channel noise for each code length")
ax.grid(alpha=0.3, which="both")
ax.legend(fontsize=8)
fig.savefig(os.path.join(FIG, "expB_ber_sigma.png"))
plt.close(fig)

# Processing gain: plotting against the despread noise sigma/sqrt(L) collapses all curves.
fig, ax = plt.subplots(figsize=(6.4, 4.3))
for Lv in sorted(B["L"].unique()):
    sub = B[(B.L == Lv) & (B.ber > 0)].sort_values("sigma")
    ax.plot(sub["sigma"] / np.sqrt(Lv), sub["ber"], L_MARK[Lv], color=L_COLOR[Lv], ms=6, label=f"L = {Lv}")
x = np.linspace(0.02, 2.0, 300)
ax.plot(x, [Q(0.5 / v) for v in x], "k-", lw=1.4, label="theory  Q(0.5 / (σ/√L))")
ax.set_yscale("log")
ax.set_ylim(1e-5, 1)
ax.set_xlabel("Effective noise after despreading  σ / √L")
ax.set_ylabel("Bit error rate (BER)")
ax.set_title("Experiment B — Processing gain: all code lengths collapse")
ax.grid(alpha=0.3, which="both")
ax.legend(fontsize=8)
fig.savefig(os.path.join(FIG, "expB_processing_gain.png"))
plt.close(fig)

B2 = pd.read_csv(os.path.join(RESULTS, "expB2_noise_vs_n.csv")).sort_values("N")
L2, s2 = int(B2["L"].iloc[0]), float(B2["sigma"].iloc[0])
fig, ax = plt.subplots(figsize=(6.2, 4.0))
ax.plot(B2["N"], B2["ber"], "^-", color="#2ca02c", lw=1.8, ms=6, label="measured")
ax.axhline(Q(0.5 * math.sqrt(L2) / s2), color="k", ls="--", lw=1.2,
           label=f"theory Q(0.5·√{L2}/{s2}) = {Q(0.5 * math.sqrt(L2) / s2):.4f}")
ax.set_ylim(0, 0.15)
ax.set_xlabel("Number of active stations N")
ax.set_ylabel("Bit error rate (BER)")
ax.set_title(f"Experiment B2 — BER vs. N  (Walsh, L={L2}, σ={s2})")
ax.grid(alpha=0.3)
ax.legend(fontsize=8.5)
fig.savefig(os.path.join(FIG, "expB2_ber_vs_n.png"))
plt.close(fig)

# ---------------------------------------------------------------------------
# 6. Experiment C: Walsh vs random codes (noiseless, L fixed)
# ---------------------------------------------------------------------------
C = pd.read_csv(os.path.join(RESULTS, "expC_walsh_vs_random.csv"))
Cw = C[C.code == "walsh"].sort_values("N")
Cr = C[C.code == "random"].groupby("N").agg(
    ber_mean=("ber", "mean"), ber_min=("ber", "min"), ber_max=("ber", "max"),
    ok=("files_ok", "mean")).reset_index()
Lc = int(C["L"].iloc[0])

fig, axes = plt.subplots(1, 2, figsize=(11, 4.0))
ax = axes[0]
ax.plot(Cw["N"], Cw["ber"], "o-", color="#1f77b4", lw=1.8, ms=6, label="Walsh (orthogonal)")
ax.plot(Cr["N"], Cr["ber_mean"], "s-", color="#d62728", lw=1.8, ms=6, label="Random ±1 (mean of 5 seeds)")
ax.fill_between(Cr["N"], Cr["ber_min"], Cr["ber_max"], color="#d62728", alpha=0.15, label="random: min–max")
ax.set_xlabel("Number of stations N")
ax.set_ylabel("Bit error rate (BER)")
ax.set_title(f"Experiment C — BER vs. N, no noise (L={Lc})")
ax.grid(alpha=0.3)
ax.legend(fontsize=8.5)
ax = axes[1]
ax.plot(Cw["N"], 100 * Cw["files_ok"] / Cw["N"], "o-", color="#1f77b4", lw=1.8, ms=6, label="Walsh")
ax.plot(Cr["N"], 100 * Cr["ok"] / Cr["N"], "s-", color="#d62728", lw=1.8, ms=6, label="Random ±1 (mean)")
ax.set_xlabel("Number of stations N")
ax.set_ylabel("Files reconstructed exactly (%)")
ax.set_ylim(-5, 105)
ax.set_title("Experiment C — Perfect reconstruction rate")
ax.grid(alpha=0.3)
ax.legend(fontsize=8.5)
fig.tight_layout()
fig.savefig(os.path.join(FIG, "expC_walsh_vs_random.png"))
plt.close(fig)

print("All figures written to", FIG)
for f in sorted(os.listdir(FIG)):
    print(" -", f)
