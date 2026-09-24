#!/usr/bin/env python3
"""Generates every figure used in Assignment2Report.tex.
Run from the report/ directory: python3 make_plots.py
"""
import os
import pandas as pd
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
from matplotlib.patches import FancyBboxPatch, FancyArrowPatch, Rectangle
import numpy as np

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
    "stop_and_wait": "#1f77b4",
    "go_back_n": "#2ca02c",
    "selective_repeat": "#d62728",
}
LABEL = {
    "stop_and_wait": "Stop-and-Wait",
    "go_back_n": "Go-Back-N",
    "selective_repeat": "Selective Repeat",
}
MARK = {"stop_and_wait": "o", "go_back_n": "s", "selective_repeat": "^"}

# ---------------------------------------------------------------------------
# 1. Frame byte-layout diagrams (Data frame, ACK frame)
# ---------------------------------------------------------------------------
def draw_frame_layout(fields, filename, title, bracket_upto=None, bracket_label=""):
    total = sum(w for _, w, _ in fields)
    fig, ax = plt.subplots(figsize=(9, 2.1))
    x = 0
    for name, w, color in fields:
        rect = Rectangle((x, 0), w, 1, facecolor=color, edgecolor="black", linewidth=1.1)
        ax.add_patch(rect)
        if w < 3.2:
            ax.text(x + w / 2, 0.5, name, ha="center", va="center", fontsize=7.5,
                     weight="bold", rotation=90)
        else:
            ax.text(x + w / 2, 0.5, name, ha="center", va="center", fontsize=9, weight="bold")
        ax.text(x + w / 2, -0.18, f"{w}B", ha="center", va="top", fontsize=8, color="dimgray")
        x += w
    ax.set_xlim(-0.5, total + 0.5)
    ax.set_ylim(-0.55, 1.55 if bracket_upto else 1.3)
    ax.axis("off")
    ax.set_title(title, fontsize=11, weight="bold", pad=(22 if bracket_upto else 8))
    if bracket_upto:
        y = 1.12
        ax.plot([0, bracket_upto], [y, y], color="#555", lw=1.2)
        ax.plot([0, 0], [y - 0.05, y + 0.05], color="#555", lw=1.2)
        ax.plot([bracket_upto, bracket_upto], [y - 0.05, y + 0.05], color="#555", lw=1.2)
        ax.text(bracket_upto / 2, y + 0.08, bracket_label, ha="center", va="bottom",
                 fontsize=8.5, style="italic", color="#333")
    fig.savefig(os.path.join(FIG, filename))
    plt.close(fig)

data_fields = [
    ("Src MAC", 6, "#aed0ee"), ("Dst MAC", 6, "#aed0ee"),
    ("Len", 2, "#aed0ee"), ("Seq", 1, "#aed0ee"),
    ("Payload (46 bytes, zero-padded)", 46, "#c3e6c3"),
    ("FCS", 4, "#f5d6a0"),
]
draw_frame_layout(data_fields, "data_frame_layout.png",
                   "Data Frame byte layout  (total = 65 bytes with CRC-32 FCS)",
                   bracket_upto=61, bracket_label="dataword — FCS is computed over this")

ack_fields = [
    ("Src MAC", 6, "#aed0ee"), ("Dst MAC", 6, "#aed0ee"),
    ("AckNo", 1, "#f7c9c0"), ("Type", 1, "#f7c9c0"),
    ("FCS", 4, "#f5d6a0"),
]
draw_frame_layout(ack_fields, "ack_frame_layout.png",
                   "ACK Frame byte layout  (total = 18 bytes with CRC-32 FCS)",
                   bracket_upto=14, bracket_label="dataword — FCS is computed over this")

# ---------------------------------------------------------------------------
# 2. Procedural / architecture diagram
# ---------------------------------------------------------------------------
def box(ax, xy, w, h, text, fc, fs=9, ec="black"):
    b = FancyBboxPatch(xy, w, h, boxstyle="round,pad=0.02,rounding_size=0.06",
                        facecolor=fc, edgecolor=ec, linewidth=1.1)
    ax.add_patch(b)
    ax.text(xy[0] + w / 2, xy[1] + h / 2, text, ha="center", va="center",
             fontsize=fs, wrap=True)
    return (xy[0] + w / 2, xy[1] + h / 2)

def arrow(ax, p1, p2, color="black", style="-|>", lw=1.3, connectionstyle="arc3,rad=0.0"):
    a = FancyArrowPatch(p1, p2, arrowstyle=style, color=color, lw=lw,
                          mutation_scale=12, connectionstyle=connectionstyle)
    ax.add_patch(a)

fig, ax = plt.subplots(figsize=(11, 7.2))
ax.set_xlim(0, 11)
ax.set_ylim(0, 10.6)
ax.axis("off")
ax.set_title("Flow-Control Simulation — Procedural Structure", fontsize=13, weight="bold")

blue, green, teal, yellow, gray = "#cfe3f7", "#d4f0d4", "#cdeee7", "#fbeec6", "#e6e6e6"

ax.text(2.6, 10.15, "SENDER  (UDP client)", ha="center", fontsize=10.5, weight="bold", color="#1a4a7a")
ax.text(8.3, 10.15, "RECEIVER  (UDP server)", ha="center", fontsize=10.5, weight="bold", color="#1a6a3a")

s_input   = box(ax, (1.1, 8.9), 3.0, 0.7, "input file\n(46-byte chunks)", yellow)
s_framing = box(ax, (1.1, 7.8), 3.0, 0.7, "Framing()\nbuild DataFrame + FCS", blue)
s_send    = box(ax, (1.1, 6.7), 3.0, 0.7, "Send()\nnew frame or retransmit?", blue)
s_timer   = box(ax, (1.1, 5.6), 3.0, 0.7, "Timer() / Timeout()\nadaptive RTT estimate", blue)
s_recv    = box(ax, (1.1, 4.5), 3.0, 0.7, "Recv()\nvalidate ACK, slide window", blue)

r_recv    = box(ax, (6.9, 6.7), 3.0, 0.7, "Recv()\nparse DataFrame", green)
r_check   = box(ax, (6.9, 5.6), 3.0, 0.7, "Check()\nrecompute & compare FCS", green)
r_decide  = box(ax, (6.9, 4.5), 3.0, 0.7, "Accept / Discard\n(buffer if SR, else in-order only)", "#e3c9f0")
r_send    = box(ax, (6.9, 3.4), 3.0, 0.7, "Send()\nbuild & send ACK frame", green)
r_out     = box(ax, (6.9, 2.3), 3.0, 0.7, "output file\n(delivered in order)", yellow)

ch = box(ax, (4.55, 5.6), 1.9, 0.7, "Channel()\ndelay + loss\n+ bit-error", "#f7cfc7")

arrow(ax, (2.6, 8.9), (2.6, 8.5))
arrow(ax, (2.6, 7.8), (2.6, 7.4))
arrow(ax, (2.6, 6.7), (2.6, 6.3))
arrow(ax, (2.6, 5.6), (2.6, 5.2))
arrow(ax, (4.1, 6.7), (5.5, 6.05), connectionstyle="arc3,rad=-0.15")
arrow(ax, (6.4, 6.05), (6.9, 7.05), connectionstyle="arc3,rad=-0.15")
arrow(ax, (8.4, 6.7), (8.4, 6.3))
arrow(ax, (8.4, 5.6), (8.4, 5.2))
arrow(ax, (8.4, 4.5), (8.4, 4.1))
arrow(ax, (6.9, 3.4), (5.5, 6.0), connectionstyle="arc3,rad=0.35", color="#a33")
arrow(ax, (4.55, 5.75), (4.1, 5.4), connectionstyle="arc3,rad=0.25", color="#a33")
ax.text(5.5, 4.55, "ACK\n(through Channel too)", fontsize=7.5, color="#a33", ha="center", style="italic")
arrow(ax, (4.1, 5.2), (4.55, 5.9), connectionstyle="arc3,rad=0.15")
arrow(ax, (8.4, 3.4), (8.4, 3.0))

common = box(ax, (1.1, 1.0), 8.8, 0.85,
             "common/  —  shared modules: common.cpp (Checksum/CRC), error_injector.cpp,\n"
             "frame.cpp (DataFrame/AckFrame), channel.cpp, rtt_timer.cpp (Jacobson/Karels)",
             teal, fs=9)
arrow(ax, (2.6, 4.5), (2.6, 1.85), color="#888", style="-", lw=1.0, connectionstyle="arc3,rad=0")
arrow(ax, (8.4, 2.3), (8.4, 1.85), color="#888", style="-", lw=1.0, connectionstyle="arc3,rad=0")

driver = box(ax, (1.1, 0.05), 8.8, 0.65,
             "run_experiments.py (offline)  —  spawns sender+receiver pairs across\n"
             "protocols × window sizes × loss/error probabilities → results/*.csv", gray, fs=8.5)

fig.savefig(os.path.join(FIG, "architecture.png"))
plt.close(fig)

# ---------------------------------------------------------------------------
# 3. Window-mechanism timeline comparison (SW vs GBN vs SR)
# ---------------------------------------------------------------------------
fig, axes = plt.subplots(3, 1, figsize=(9, 7.5), sharex=True)
titles = ["Stop-and-Wait  (window = 1)", "Go-Back-N  (window = 4, cumulative ACK)",
          "Selective Repeat  (window = 4, independent ACK)"]

# Stop-and-wait
ax = axes[0]
t = 0
for i in range(3):
    ax.add_patch(Rectangle((t, 0.6), 1, 0.5, facecolor=COLOR["stop_and_wait"], edgecolor="k"))
    ax.text(t + 0.5, 0.85, f"D{i}", ha="center", va="center", color="white", fontsize=8, weight="bold")
    ax.add_patch(Rectangle((t + 1.3, 0.6), 0.5, 0.5, facecolor="#cfe3f7", edgecolor="k"))
    ax.text(t + 1.55, 0.85, f"A{i}", ha="center", va="center", fontsize=7.5)
    t += 2.2
ax.set_ylim(0, 1.5); ax.set_yticks([])

# Go-Back-N
ax = axes[1]
for i in range(4):
    ax.add_patch(Rectangle((i * 0.9, 0.6), 0.8, 0.5, facecolor=COLOR["go_back_n"], edgecolor="k"))
    ax.text(i * 0.9 + 0.4, 0.85, f"D{i}", ha="center", va="center", color="white", fontsize=8, weight="bold")
ax.add_patch(Rectangle((4.3, 0.6), 0.6, 0.5, facecolor="#d4f0d4", edgecolor="k"))
ax.text(4.6, 0.85, "A3\n(cum.)", ha="center", va="center", fontsize=7)
for i in range(4, 8):
    ax.add_patch(Rectangle((5.1 + (i - 4) * 0.9, 0.6), 0.8, 0.5, facecolor=COLOR["go_back_n"], edgecolor="k"))
    ax.text(5.1 + (i - 4) * 0.9 + 0.4, 0.85, f"D{i}", ha="center", va="center", color="white", fontsize=8, weight="bold")
ax.set_ylim(0, 1.5); ax.set_yticks([])

# Selective Repeat
ax = axes[2]
labels = ["D0", "D1 (lost)", "D2", "D3"]
for i in range(4):
    fc = "#f0a0a0" if i == 1 else COLOR["selective_repeat"]
    ax.add_patch(Rectangle((i * 0.9, 0.6), 0.8, 0.5, facecolor=fc, edgecolor="k"))
    ax.text(i * 0.9 + 0.4, 0.85, f"D{i}", ha="center", va="center", color="white", fontsize=8, weight="bold")
for i, keep in zip([0, 2, 3], [True, True, True]):
    ax.add_patch(Rectangle((i * 0.9 + 0.1, 0.0), 0.6, 0.4, facecolor="#f6bcbc", edgecolor="k"))
    ax.text(i * 0.9 + 0.4, 0.2, f"A{i}", ha="center", va="center", fontsize=7)
ax.add_patch(Rectangle((4.0, 0.6), 0.8, 0.5, facecolor=COLOR["selective_repeat"], edgecolor="k"))
ax.text(4.4, 0.85, "D1'\n(retx)", ha="center", va="center", color="white", fontsize=7, weight="bold")
ax.add_patch(Rectangle((5.0, 0.0), 0.6, 0.4, facecolor="#f6bcbc", edgecolor="k"))
ax.text(5.3, 0.2, "A1", ha="center", va="center", fontsize=7)
ax.set_ylim(-0.2, 1.5); ax.set_yticks([])
ax.text(-0.3, 0.85, "sent →", ha="right", va="center", fontsize=8, color="#555")
ax.text(-0.3, 0.2, "ACKed →", ha="right", va="center", fontsize=8, color="#555")

for a, ti in zip(axes, titles):
    a.set_title(ti, fontsize=10.5, loc="left")
    a.set_xlim(-1.0, 7.6)
    for s in ["top", "right", "left"]:
        a.spines[s].set_visible(False)
axes[-1].set_xlabel("time →  (not to scale)")
fig.suptitle("Window mechanism comparison: what is outstanding at once, and how loss is recovered",
              fontsize=11, weight="bold", y=0.995)
fig.tight_layout(rect=[0, 0, 1, 0.97])
fig.savefig(os.path.join(FIG, "window_timeline.png"))
plt.close(fig)

# ---------------------------------------------------------------------------
# 4. Experiment A: window-size sweep, clean channel
# ---------------------------------------------------------------------------
A = pd.read_csv(os.path.join(RESULTS, "experiment_A_window_effect.csv"))

def lineplot_by_protocol(df, xcol, ycol, ylabel, title, filename, sw_as_point=True, logx=False):
    fig, ax = plt.subplots(figsize=(6.2, 4.2))
    for proto in ["stop_and_wait", "go_back_n", "selective_repeat"]:
        sub = df[df.protocol == proto].sort_values(xcol)
        if proto == "stop_and_wait" and sw_as_point:
            ax.axhline(sub[ycol].iloc[0], color=COLOR[proto], ls="--", lw=1.3,
                        label=f"{LABEL[proto]} (N=1, reference)")
        else:
            ax.plot(sub[xcol], sub[ycol], marker=MARK[proto], color=COLOR[proto],
                     lw=1.8, ms=6, label=LABEL[proto])
    if logx:
        ax.set_xscale("log", base=2)
        ax.set_xticks(sorted(df[df.protocol != "stop_and_wait"][xcol].unique()))
        ax.get_xaxis().set_major_formatter(matplotlib.ticker.ScalarFormatter())
    ax.set_xlabel("Window size N")
    ax.set_ylabel(ylabel)
    ax.set_title(title)
    ax.grid(alpha=0.3)
    ax.legend(fontsize=8.5)
    fig.savefig(os.path.join(FIG, filename))
    plt.close(fig)

lineplot_by_protocol(A, "window", "throughput_Bps", "Throughput (bytes/s)",
                      "Experiment A — Throughput vs. window size (no loss/error)",
                      "exp_a_throughput.png", logx=True)
lineplot_by_protocol(A, "window", "efficiency_pct", "Efficiency (%)",
                      "Experiment A — Efficiency vs. window size (no loss/error)",
                      "exp_a_efficiency.png", logx=True)
lineplot_by_protocol(A, "window", "avg_rtt_ms", "Avg. RTT per frame (ms)",
                      "Experiment A — Propagation-to-ACK time vs. window size",
                      "exp_a_rtt.png", logx=True)

# ---------------------------------------------------------------------------
# 5. Experiment B: impairment-probability sweep
# ---------------------------------------------------------------------------
B = pd.read_csv(os.path.join(RESULTS, "experiment_B_loss_sweep.csv"))
B["p"] = (B["loss_prob"] + B["error_prob"]).round(2)   # reconstruct nominal p in [0,0.5]

def lineplot_vs_p(df, ycol, ylabel, title, filename):
    fig, ax = plt.subplots(figsize=(6.2, 4.2))
    for proto in ["stop_and_wait", "go_back_n", "selective_repeat"]:
        sub = df[df.protocol == proto].sort_values("p")
        ax.plot(sub["p"], sub[ycol], marker=MARK[proto], color=COLOR[proto],
                 lw=1.8, ms=6, label=LABEL[proto])
    ax.set_xlabel("Combined channel impairment probability  p\n(loss_prob = error_prob = p/2, applied to data and ACK channels)")
    ax.set_ylabel(ylabel)
    ax.set_title(title)
    ax.grid(alpha=0.3)
    ax.legend(fontsize=8.5)
    fig.savefig(os.path.join(FIG, filename))
    plt.close(fig)

lineplot_vs_p(B, "efficiency_pct", "Efficiency (%)",
              "Experiment B — Efficiency vs. impairment probability",
              "exp_b_efficiency.png")
lineplot_vs_p(B, "throughput_Bps", "Throughput (bytes/s)",
              "Experiment B — Throughput vs. impairment probability",
              "exp_b_throughput.png")
lineplot_vs_p(B, "retransmissions", "Retransmitted frames (count)",
              "Experiment B — Retransmission overhead vs. impairment probability",
              "exp_b_retransmissions.png")

print("All figures written to", FIG)
for f in sorted(os.listdir(FIG)):
    print(" -", f)
