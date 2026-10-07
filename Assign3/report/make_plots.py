#!/usr/bin/env python3
"""Generates every figure for the Assignment 3 report.
Run from the report/ directory: python3 make_plots.py
"""
import os
import pandas as pd
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, FancyArrowPatch, Rectangle

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

COLOR = {
    "non_persistent": "#1f77b4",
    "one_persistent": "#ff7f0e",
    "p_persistent":   "#2ca02c",
    "csma_cd":         "#d62728",
}
LABEL = {
    "non_persistent": "Non-Persistent",
    "one_persistent": "1-Persistent",
    "p_persistent":   "p-Persistent (best p)",
    "csma_cd":         "CSMA/CD",
}
MARK = {"non_persistent": "o", "one_persistent": "s", "p_persistent": "^", "csma_cd": "D"}

# ---------------------------------------------------------------------------
# 1. Architecture / procedural diagram
# ---------------------------------------------------------------------------
def box(ax, xy, w, h, text, fc, fs=9, ec="black"):
    b = FancyBboxPatch(xy, w, h, boxstyle="round,pad=0.02,rounding_size=0.06",
                        facecolor=fc, edgecolor=ec, linewidth=1.1)
    ax.add_patch(b)
    ax.text(xy[0] + w / 2, xy[1] + h / 2, text, ha="center", va="center", fontsize=fs, wrap=True)
    return (xy[0] + w / 2, xy[1] + h / 2)

def arrow(ax, p1, p2, color="black", style="-|>", lw=1.3, connectionstyle="arc3,rad=0.0"):
    a = FancyArrowPatch(p1, p2, arrowstyle=style, color=color, lw=lw,
                          mutation_scale=12, connectionstyle=connectionstyle)
    ax.add_patch(a)

fig, ax = plt.subplots(figsize=(10.5, 6.8))
ax.set_xlim(0, 10.5)
ax.set_ylim(0, 9.6)
ax.axis("off")
ax.set_title("CSMA Simulation — Lock-Step Slot Protocol", fontsize=13, weight="bold")

blue, green, yellow, gray, red = "#cfe3f7", "#d4f0d4", "#fbeec6", "#e6e6e6", "#f7cfc7"

ch = box(ax, (3.75, 7.5), 3.0, 1.1,
         "channel_server\nstate: IDLE / BUSY\ntracks collisions, delay, throughput", red)
ax.text(5.25, 8.85, "emulated shared channel", ha="center", fontsize=9.5, style="italic", color="#a33")

s1 = box(ax, (0.4, 5.2), 2.3, 1.0, "station 0\n(independent process)", blue)
s2 = box(ax, (4.1, 5.2), 2.3, 1.0, "station 1\n...", blue)
s3 = box(ax, (7.8, 5.2), 2.3, 1.0, "station N-1", blue)

for sx in [(1.55, 6.2), (5.25, 6.2), (8.95, 6.2)]:
    arrow(ax, (5.25, 7.5), sx, connectionstyle="arc3,rad=0.0", color="#333")
    arrow(ax, sx, (5.25, 7.5), connectionstyle="arc3,rad=0.25", color="#666")

ax.text(5.25, 4.55, "each slot:  SERVER --SLOT n, state--> STATION\n"
                     "            STATION --ACT n, TRANSMIT|WAIT--> SERVER",
        ha="center", fontsize=8.7, family="monospace",
        bbox=dict(boxstyle="round", fc="#fdfdfd", ec="#999"))

logic = box(ax, (0.4, 2.5), 9.7, 1.5,
    "Per-station decision (scheme-dependent):\n"
    "Non-Persistent: idle -> transmit; busy -> give up, random wait, re-sense later\n"
    "1-Persistent / CSMA-CD: idle -> transmit; busy -> keep sensing every slot (persist)\n"
    "p-Persistent: idle -> transmit w.p. p, else defer 1 slot and re-flip; busy -> persist",
    yellow, fs=8.6)

outcome = box(ax, (0.4, 0.9), 9.7, 1.2,
    "Collision resolution: >=2 simultaneous TRANSMIT -> collision.\n"
    "CSMA/CD aborts after jamSlots (fast abort); all others waste the full frameSlots.\n"
    "All schemes use the same post-collision Binary Exponential Backoff (IEEE 802.3).",
    gray, fs=8.6)

arrow(ax, (5.25, 5.2), (5.25, 4.0), color="#888", style="-", lw=1.0)
arrow(ax, (5.25, 2.5), (5.25, 2.1), color="#888", style="-", lw=1.0)

fig.savefig(os.path.join(FIG, "architecture.png"))
plt.close(fig)

# ---------------------------------------------------------------------------
# 2. Slot-outcome timeline (IDLE / SUCCESS / COLLISION), illustrative
# ---------------------------------------------------------------------------
fig, axes = plt.subplots(2, 1, figsize=(9, 4.2), sharex=True)
titles = ["Plain CSMA (Non-/1-/p-Persistent): collision wastes the full frame time",
          "CSMA/CD: collision aborted after a short jam window"]

# non-CD: idle, success (5 slots), collision (5 slots wasted), success
ax = axes[0]
segs = [("IDLE", 1, "#e6e6e6"), ("SUCCESS", 5, "#2ca02c"), ("COLLISION", 5, "#d62728"), ("SUCCESS", 4, "#2ca02c")]
x = 0
for label, w, c in segs:
    ax.add_patch(Rectangle((x, 0.3), w, 0.6, facecolor=c, edgecolor="k"))
    ax.text(x + w / 2, 0.6, label, ha="center", va="center", fontsize=8, weight="bold",
            color="white" if label != "IDLE" else "black")
    x += w
ax.set_xlim(-0.3, x + 0.3); ax.set_ylim(0, 1); ax.set_yticks([])

ax = axes[1]
segs = [("IDLE", 1, "#e6e6e6"), ("SUCCESS", 5, "#2ca02c"), ("COLL\n(jam=1)", 1, "#d62728"),
        ("IDLE", 1, "#e6e6e6"), ("SUCCESS", 5, "#2ca02c")]
x = 0
for label, w, c in segs:
    ax.add_patch(Rectangle((x, 0.3), w, 0.6, facecolor=c, edgecolor="k"))
    ax.text(x + w / 2, 0.6, label, ha="center", va="center", fontsize=7.5, weight="bold",
            color="white" if "IDLE" not in label else "black")
    x += w
ax.set_xlim(-0.3, 13.3); ax.set_ylim(0, 1); ax.set_yticks([])

for a, t in zip(axes, titles):
    a.set_title(t, fontsize=10, loc="left")
    for s in ["top", "right", "left"]:
        a.spines[s].set_visible(False)
axes[-1].set_xlabel("time (slots) →")
fig.tight_layout()
fig.savefig(os.path.join(FIG, "slot_timeline.png"))
plt.close(fig)

# ---------------------------------------------------------------------------
# 3. Experiment A: p-persistent, sweep p (fixed N)
# ---------------------------------------------------------------------------
A = pd.read_csv(os.path.join(RESULTS, "expA_p_sweep.csv")).sort_values("p")
N_fixed = int(A["N"].iloc[0])

def plot_single(df, xcol, ycol, ylabel, title, filename, color="#2ca02c", marker="^"):
    fig, ax = plt.subplots(figsize=(6.2, 4.2))
    ax.plot(df[xcol], df[ycol], marker=marker, color=color, lw=1.8, ms=6)
    ax.set_xlabel(xcol)
    ax.set_ylabel(ylabel)
    ax.set_title(title)
    ax.grid(alpha=0.3)
    fig.savefig(os.path.join(FIG, filename))
    plt.close(fig)

plot_single(A, "p", "collisions", "Collision count",
            f"Experiment A — Collisions vs. p  (p-Persistent, N={N_fixed})",
            "expA_collisions.png")
plot_single(A, "p", "avg_delay_slots", "Avg. transmission delay (slots)",
            f"Experiment A — Delay vs. p  (p-Persistent, N={N_fixed})",
            "expA_delay.png")
plot_single(A, "p", "throughput", "Normalized throughput S",
            f"Experiment A — Throughput vs. p  (p-Persistent, N={N_fixed})",
            "expA_throughput.png")

# ---------------------------------------------------------------------------
# 4. Experiment B: all schemes, sweep N
# ---------------------------------------------------------------------------
B = pd.read_csv(os.path.join(RESULTS, "expB_n_sweep.csv"))
best_p = B[B.scheme == "p_persistent"]["p"].iloc[0]

def plot_by_scheme(df, ycol, ylabel, title, filename, logy=False):
    fig, ax = plt.subplots(figsize=(6.6, 4.4))
    for scheme in ["non_persistent", "one_persistent", "p_persistent", "csma_cd"]:
        sub = df[df.scheme == scheme].sort_values("N")
        ax.plot(sub["N"], sub[ycol], marker=MARK[scheme], color=COLOR[scheme],
                 lw=1.8, ms=6, label=LABEL[scheme])
    ax.set_xlabel("Number of contending stations N")
    ax.set_ylabel(ylabel)
    ax.set_title(title)
    if logy:
        ax.set_yscale("log")
    ax.grid(alpha=0.3)
    ax.legend(fontsize=8.5)
    fig.savefig(os.path.join(FIG, filename))
    plt.close(fig)

plot_by_scheme(B, "collisions", "Collision count",
                "Experiment B — Collisions vs. N (all schemes)", "expB_collisions.png")
plot_by_scheme(B, "avg_delay_slots", "Avg. transmission delay (slots)",
                "Experiment B — Delay vs. N (all schemes)", "expB_delay.png", logy=True)
plot_by_scheme(B, "throughput", "Normalized throughput S",
                "Experiment B — Throughput vs. N (all schemes)", "expB_throughput.png")

print("All figures written to", FIG)
for f in sorted(os.listdir(FIG)):
    print(" -", f)
