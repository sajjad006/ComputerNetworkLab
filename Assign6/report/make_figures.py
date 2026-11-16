#!/usr/bin/env python3
"""Draws the logical topology (with addressing) of every Assignment 6 network.
Run from anywhere: python3 make_figures.py  ->  figures/topo_q*.png
All addresses are the ones configured in Packet Tracer (see the screenshots).
"""
import os
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, Ellipse

HERE = os.path.dirname(os.path.abspath(__file__))
FIG = os.path.join(HERE, "figures")
os.makedirs(FIG, exist_ok=True)

plt.rcParams.update({"font.size": 9, "savefig.dpi": 170, "savefig.bbox": "tight"})

STYLE = {  # face colour, edge colour
    "pc":     ("#e8f1fb", "#1f5fa8"),
    "server": ("#eaf6ea", "#2e7d32"),
    "hub":    ("#fff3e0", "#e65100"),
    "switch": ("#ede7f6", "#4527a0"),
    "router": ("#fdecea", "#b71c1c"),
}
KIND_LABEL = {"pc": "PC", "server": "Server", "hub": "Hub",
              "switch": "Switch", "router": "Router"}


def node(ax, x, y, kind, name, lines=(), w=1.9, h=None):
    fc, ec = STYLE[kind]
    body = list(lines)
    h = h or 0.42 + 0.27 * len(body)
    ax.add_patch(FancyBboxPatch((x - w / 2, y - h / 2), w, h,
                                boxstyle="round,pad=0.04,rounding_size=0.12",
                                fc=fc, ec=ec, lw=1.4, zorder=3))
    ax.text(x, y + h / 2 - 0.2, name, ha="center", va="center",
            fontweight="bold", color=ec, zorder=4, fontsize=9)
    for i, ln in enumerate(body):
        ax.text(x, y + h / 2 - 0.47 - 0.27 * i, ln, ha="center", va="center",
                family="monospace", fontsize=7.4, zorder=4)
    return (x, y)


def link(ax, a, b, cross=False, label=None, la=None, lb=None, frac=0.5, color="#333"):
    ax.plot([a[0], b[0]], [a[1], b[1]], color=color, lw=1.6,
            ls=(0, (5, 3)) if cross else "-", zorder=1)
    def put(t, f):
        x = a[0] + (b[0] - a[0]) * f
        y = a[1] + (b[1] - a[1]) * f
        ax.text(x, y, t, ha="center", va="center", fontsize=7, zorder=5,
                bbox=dict(fc="white", ec="none", pad=0.6))
    if label:
        put(label, frac)
    if la:
        put(la, 0.22)
    if lb:
        put(lb, 0.78)


def legend(ax, x, y):
    ax.plot([x, x + 0.6], [y, y], color="#333", lw=1.6)
    ax.text(x + 0.7, y, "straight-through", va="center", fontsize=7.5)
    ax.plot([x + 2.6, x + 3.2], [y, y], color="#333", lw=1.6, ls=(0, (5, 3)))
    ax.text(x + 3.3, y, "cross-over", va="center", fontsize=7.5)


def canvas(w, h):
    fig, ax = plt.subplots(figsize=(w, h))
    ax.set_xlim(0, w * 1.25)
    ax.set_ylim(0, h * 1.25)
    ax.set_aspect("equal")
    ax.axis("off")
    return fig, ax


def save(fig, name):
    fig.savefig(os.path.join(FIG, name))
    plt.close(fig)


# --- Q1: back-to-back -------------------------------------------------------
fig, ax = canvas(6.4, 1.8)
a = node(ax, 1.4, 1.2, "pc", "PC0", ["192.168.1.1/24"])
b = node(ax, 6.6, 1.2, "pc", "PC1", ["192.168.1.2/24"])
link(ax, (2.35, 1.2), (5.65, 1.2), cross=True, label="Copper cross-over\nFa0 <-> Fa0")
legend(ax, 2.0, 0.2)
save(fig, "topo_q1.png")

# --- LAN-A / LAN-B helpers ---------------------------------------------------
LAN_A = [("PC2", "192.168.1.3", "00D0.FFA5.37EE"),
         ("PC3", "192.168.1.4", "0090.2BE8.B750"),
         ("PC4", "192.168.1.5", "0001.C70A.0697")]
LAN_B = [("PC5", "192.168.1.6", "0030.F223.90E0", "Fa0/1"),
         ("PC6", "192.168.1.7", "0005.5ED3.1D1E", "Fa1/1"),
         ("PC7", "192.168.1.8", "0002.16AA.B4DC", "Fa2/1")]


def draw_lan_a(ax, cx, top, hub_y):
    hub = node(ax, cx, hub_y, "hub", "LAN A (Hub-PT)", [], w=2.4)
    for i, (n, ip, mac) in enumerate(LAN_A):
        x = cx + (i - 1) * 2.6
        p = node(ax, x, top, "pc", n, [ip + "/24", mac], w=2.3)
        link(ax, (x, top - 0.45), (cx + (i - 1) * 0.5, hub_y + 0.2))
    return hub


def draw_lan_b(ax, cx, top, sw_y):
    sw = node(ax, cx, sw_y, "switch", "LAN B (Switch-PT)", [], w=2.6)
    for i, (n, ip, mac, port) in enumerate(LAN_B):
        x = cx + (i - 1) * 2.6
        node(ax, x, top, "pc", n, [ip + "/24", mac], w=2.3)
        link(ax, (x, top - 0.45), (cx + (i - 1) * 0.6, sw_y + 0.2), lb=port)
    return sw


# --- Q2 ----------------------------------------------------------------------
fig, ax = canvas(6.6, 2.6)
draw_lan_a(ax, 4.1, 2.45, 0.75)
save(fig, "topo_q2.png")

# --- Q3 ----------------------------------------------------------------------
fig, ax = canvas(6.6, 2.6)
draw_lan_b(ax, 4.1, 2.45, 0.75)
save(fig, "topo_q3.png")

# --- Q4 ----------------------------------------------------------------------
fig, ax = canvas(13.0, 2.9)
draw_lan_a(ax, 4.0, 2.75, 0.85)
draw_lan_b(ax, 12.4, 2.75, 0.85)
link(ax, (5.2, 0.85), (11.1, 0.85), cross=True,
     label="Copper cross-over (hub port <-> switch Fa3/1)")
legend(ax, 6.6, 0.2)
save(fig, "topo_q4.png")


# --- Q5 / Q6: JU-Main and JU-SL ---------------------------------------------
def two_lans(with_servers):
    fig, ax = canvas(13.0, 4.6)
    ax.add_patch(Ellipse((4.1, 2.9), 8.0, 5.3, fc="#fafafa", ec="#999", lw=1, zorder=0))
    ax.add_patch(Ellipse((12.2, 2.9), 8.0, 5.3, fc="#fafafa", ec="#999", lw=1, zorder=0))
    ax.text(1.0, 5.25, "JU-Main  192.168.148.0/24", fontweight="bold", color="#555")
    ax.text(9.3, 5.25, "JU-SL  192.168.149.0/24", fontweight="bold", color="#555")

    r1 = node(ax, 5.9, 4.3, "router", "R1 (1841)",
              ["Fa0/0 192.168.148.1", "Fa0/1 192.168.150.1"], w=2.6)
    r2 = node(ax, 10.4, 4.3, "router", "R2 (1841)",
              ["Fa0/0 192.168.149.1", "Fa0/1 192.168.150.2"], w=2.6)
    link(ax, (7.2, 4.3), (9.1, 4.3), cross=True, color="#b71c1c")
    ax.text(8.15, 4.75, "WAN (cross-over)\n192.168.150.0/24", ha="center", va="center",
            fontsize=7, color="#b71c1c")

    s1 = node(ax, 3.4, 2.9, "switch", "PC-LAB1-Switch", ["Cisco 2950-24"], w=2.5)
    s2 = node(ax, 12.9, 2.9, "switch", "PC-LAB2-Switch", ["Cisco 2950-24"], w=2.5)
    link(ax, (4.6, 3.1), (5.4, 3.85))
    link(ax, (11.6, 3.1), (10.9, 3.85))

    for i, n in enumerate(["PC8", "PC9", "PC10"]):
        x = 1.3 + i * 2.1
        lines = ["DHCP, gw .148.1"] if with_servers else ["192.168.148.%d" % (i + 2), "gw 192.168.148.1"]
        node(ax, x, 0.9, "pc", n, lines, w=1.95)
        link(ax, (x, 1.3), (3.4 + (i - 1) * 0.4, 2.55))
    for i, n in enumerate(["PC12", "PC13", "PC14"]):
        x = 10.8 + i * 2.1
        lines = ["DHCP, gw .149.1"] if with_servers else ["192.168.149.%d" % (i + 2), "gw 192.168.149.1"]
        node(ax, x, 0.9, "pc", n, lines, w=1.95)
        link(ax, (x, 1.3), (12.9 + (i - 1) * 0.4, 2.55))
    if with_servers:
        node(ax, 6.6, 2.4, "server", "Server0 (DHCP)", ["192.168.148.10/24"], w=2.4)
        link(ax, (4.65, 2.8), (5.4, 2.5))
        node(ax, 9.7, 2.4, "server", "Server1 (DHCP)", ["192.168.149.10/24"], w=2.4)
        link(ax, (11.65, 2.8), (10.9, 2.5))
    return fig


save(two_lans(False), "topo_q5.png")
save(two_lans(True), "topo_q6.png")

# --- Q7: CSE ------------------------------------------------------------------
fig, ax = canvas(10.4, 4.4)
ax.add_patch(Ellipse((6.5, 2.75), 12.6, 5.4, fc="#fafafa", ec="#999", lw=1, zorder=0))
ax.text(1.0, 5.15, "CSE LAN  192.168.151.0/24   domain: cse.myuniv.edu",
        fontweight="bold", color="#555")
sw = node(ax, 6.5, 2.75, "switch", "CSE-Switch", ["Cisco 2950-24"], w=2.3)
for i, n in enumerate(["CSE-PC1", "CSE-PC2", "CSE-PC3"]):
    x = 3.7 + i * 2.8
    node(ax, x, 4.45, "pc", n, ["DHCP" + (" -> .151.100" if i == 0 else ""), "DNS 192.168.151.13"], w=2.5)
    link(ax, (x, 4.0), (6.5 + (i - 1) * 0.5, 3.1))
servers = [("DHCP Server", "192.168.151.10", "pool from .151.100", 2.3, 1.0),
           ("WebServer", "192.168.151.11", "www.cse.myuniv.edu", 5.1, 0.75),
           ("FTP Server", "192.168.151.12", "ftp.cse.myuniv.edu", 7.9, 0.75),
           ("DNS Server", "192.168.151.13", "A records for www/ftp", 10.7, 1.0)]
for n, ip, extra, x, y in servers:
    node(ax, x, y, "server", n, [ip + "/24", extra], w=2.6)
    link(ax, (x, y + 0.45), (6.5 + (x - 6.5) * 0.12, 2.4))
save(fig, "topo_q7.png")

print("figures written to", FIG)
