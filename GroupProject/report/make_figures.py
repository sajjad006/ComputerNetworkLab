#!/usr/bin/env python3
"""Generates every figure for the DHCP + ARP group project report.
Run from the report/ directory: python3 make_figures.py
(The result plots need ../results/dhcp_loss.csv and ../results/arp_traffic.csv,
 produced by ../experiments/run_experiments.py.)
"""
import os
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

DHCP_C = "#0d7a78"
ARP_C = "#b06000"
GREY = "#555555"
LIGHT = {"dhcp": "#e1f2f1", "arp": "#fbeedb", "grey": "#f2f2f2", "white": "#ffffff"}


# ------------------------------------------------------------------ drawing helpers
def box(ax, x, y, w, h, text, fc="#ffffff", ec=GREY, fs=9, bold=False, lw=1.2):
    ax.add_patch(FancyBboxPatch((x, y), w, h, boxstyle="round,pad=0.02,rounding_size=0.08",
                                fc=fc, ec=ec, lw=lw))
    ax.text(x + w / 2, y + h / 2, text, ha="center", va="center", fontsize=fs,
            fontweight="bold" if bold else "normal")


def arrow(ax, x1, y1, x2, y2, color=GREY, style="-|>", ls="-", lw=1.4, both=False):
    ax.add_patch(FancyArrowPatch((x1, y1), (x2, y2),
                                 arrowstyle="<|-|>" if both else style, mutation_scale=12,
                                 color=color, lw=lw, linestyle=ls, shrinkA=0, shrinkB=0))


def canvas(w, h, figsize):
    fig, ax = plt.subplots(figsize=figsize)
    ax.set_xlim(0, w)
    ax.set_ylim(0, h)
    ax.axis("off")
    return fig, ax


def save(fig, name):
    fig.savefig(os.path.join(FIG, name))
    plt.close(fig)
    print("wrote", name)


def ladder(name, actors, messages, color, height_per=1.0, figsize=(9, 5), notes=()):
    """Sequence diagram. messages: (from_idx, to_idx_or_list, label, sublabel, dashed)."""
    n = len(actors)
    top = len(messages) * height_per + 1.6
    fig, ax = canvas(n * 3, top, figsize)
    xs = [1.5 + 3 * i for i in range(n)]
    for x, a in zip(xs, actors):
        box(ax, x - 1.15, top - 0.95, 2.3, 0.75, a, fc=LIGHT["grey"], bold=True, fs=9)
        ax.plot([x, x], [0.2, top - 0.95], color="#bbbbbb", lw=1.5, zorder=0)
    y = top - 1.6
    for frm, to, label, sub, dashed in messages:
        targets = to if isinstance(to, list) else [to]
        for t in targets:
            x1, x2 = xs[frm], xs[t]
            off = 0.06 if x2 > x1 else -0.06
            arrow(ax, x1 + off, y, x2 - off, y, color=color, ls="--" if dashed else "-")
        mid = (xs[frm] + xs[targets[0]]) / 2 if len(targets) == 1 else \
            (xs[frm] + xs[targets[-1]]) / 2
        ax.text(mid, y + 0.12, label, ha="center", va="bottom", fontsize=9,
                fontweight="bold", color=color, family="monospace")
        if sub:
            ax.text(mid, y - 0.12, sub, ha="center", va="top", fontsize=7.5, color=GREY)
        y -= height_per
    for (actor, yy, text) in notes:
        ax.text(xs[actor] + 0.12, top - 1.6 - yy * height_per, text, fontsize=7.5,
                color=GREY, va="center", style="italic")
    save(fig, name)


# ------------------------------------------------------------------ DHCP diagrams
def fig_dhcp_dora():
    ladder("dhcp_dora.png", ["Client (UDP :68)", "Server (UDP :67)"], [
        (0, 1, "1. DHCPDISCOVER", "0.0.0.0 -> 255.255.255.255   options 53=1, 55, 61   xid = X", False),
        (1, 0, "2. DHCPOFFER", "yiaddr = 192.168.50.100   options 53=2, 54, 51, 1, 3, 6", False),
        (0, 1, "3. DHCPREQUEST", "broadcast   options 53=3, 50 = offered IP, 54 = chosen server", False),
        (1, 0, "4. DHCPACK / DHCPNAK", "same xid, lease starts (server: OFFERED -> BOUND)", False),
    ], DHCP_C, height_per=1.1, figsize=(8.5, 4.4))


def fig_virtual_lan():
    fig, ax = canvas(12, 6.2, (9, 4.6))
    box(ax, 3.2, 3.9, 5.6, 1.9,
        "router namespace\nbr0 = Linux bridge (switch)   192.168.50.1/24\n"
        "runs dhcp_server · tcpdump / capture here", fc=LIGHT["dhcp"], ec=DHCP_C, fs=9)
    pcs = [("pc1", "02:00:00:00:00:01", ".100"), ("pc2", "02:00:00:00:00:02", ".101"),
           ("pc3", "02:00:00:00:00:03", ".102")]
    for i, (pc, mac, ip) in enumerate(pcs):
        x = 0.5 + i * 4
        box(ax, x, 0.4, 3, 1.6, f"{pc} namespace\neth0  {mac}\nno IP -> 192.168.50{ip}",
            fc=LIGHT["white"], ec=GREY, fs=8.5)
        arrow(ax, x + 1.5, 2.0, 4.6 + i * 1.4, 3.9, color=DHCP_C, both=True)
        ax.text((x + 1.5 + 4.6 + i * 1.4) / 2 + 0.15, 2.95, f"veth\n({pc}-port)",
                fontsize=7.5, color=GREY, ha="left")
    ax.text(6, 6.05, "One Linux kernel, four isolated network stacks", ha="center",
            fontsize=10, fontweight="bold")
    save(fig, "virtual_lan.png")


def fig_lease_states():
    fig, ax = canvas(12, 4.6, (9, 3.5))
    states = {"FREE\n(not in table)": (0.3, 1.8), "OFFERED\n30 s hold": (4.2, 1.8),
              "BOUND\nlease time": (8.1, 1.8), "DECLINED\n300 s": (8.1, -0.0 + 0.1)}
    for name, (x, y) in states.items():
        box(ax, x, y, 2.6, 1.2, name, fc=LIGHT["dhcp"] if "FREE" not in name else LIGHT["grey"],
            ec=DHCP_C, fs=9, bold=True)
    arrow(ax, 2.9, 2.6, 4.2, 2.6, color=DHCP_C)
    ax.text(3.55, 2.7, "DISCOVER\n/ OFFER", ha="center", va="bottom", fontsize=8)
    arrow(ax, 6.8, 2.6, 8.1, 2.6, color=DHCP_C)
    ax.text(7.45, 2.7, "REQUEST\n/ ACK", ha="center", va="bottom", fontsize=8)
    arrow(ax, 4.2, 2.1, 2.9, 2.1, color=GREY, ls="--")
    ax.text(3.55, 1.7, "30 s timeout or\nclient chose\nanother server", ha="center",
            va="top", fontsize=7.5, color=GREY)
    # BOUND -> FREE (over the top)
    ax.annotate("", xy=(1.6, 3.0), xytext=(9.4, 3.0),
                arrowprops=dict(arrowstyle="-|>", color=GREY, ls="--",
                                connectionstyle="arc3,rad=0.25"))
    ax.text(5.5, 4.35, "RELEASE or lease expiry", ha="center", fontsize=8, color=GREY)
    arrow(ax, 9.4, 1.8, 9.4, 1.3, color=GREY)
    ax.text(9.55, 1.55, "DECLINE", fontsize=8, va="center")
    arrow(ax, 8.1, 0.7, 1.6, 0.7, color=GREY, ls="--")
    ax.annotate("", xy=(1.6, 1.8), xytext=(1.6, 0.7), arrowprops=dict(arrowstyle="-|>",
                color=GREY, ls="--"))
    ax.text(4.8, 0.45, "after 300 s", ha="center", fontsize=7.5, color=GREY)
    save(fig, "lease_states.png")


def fig_dhcp_structure():
    fig, ax = canvas(14, 8.2, (10, 5.9))
    box(ax, 4.5, 7.0, 5, 1.0, "dhcp_packet.h\nDhcp_packet_t · constants · addOption · "
        "finishOptions · parseOptions", fc=LIGHT["dhcp"], ec=DHCP_C, fs=8.5, bold=False)
    # client column
    ax.text(3.2, 6.45, "Dhcp.cpp  (client)", ha="center", fontweight="bold", color=DHCP_C)
    cl = ["main(): parse args", "listInterfaces()  -> name + MAC",
          "createClientSocket()  :68, SO_BINDTODEVICE",
          "buildPacket(DISCOVER)", "exchange(...wanted=OFFER)",
          "buildPacket(REQUEST)  + opt 50, 54", "exchange(...wanted=ACK)",
          "printReply() / applyLease()"]
    for i, t in enumerate(cl):
        box(ax, 0.3, 5.7 - i * 0.72, 5.8, 0.55, t, fs=8)
        if i:
            arrow(ax, 3.2, 5.7 - (i - 1) * 0.72, 3.2, 5.7 - i * 0.72 + 0.55, lw=1)
    box(ax, 6.3, 2.75, 1.9, 1.4, "sendPacket()\nreceivePacket()\nparseReply()", fs=7.5,
        fc=LIGHT["grey"])
    arrow(ax, 6.1, 2.95, 6.3, 3.2, lw=1)
    arrow(ax, 6.1, 1.5, 6.3, 3.0, lw=1)
    # server column
    ax.text(11.0, 6.45, "DhcpServer.cpp  (server)", ha="center", fontweight="bold",
            color=DHCP_C)
    sv = ["main(): parse config, socket :67", "loop: recvfrom()", "handle(): parseOptions",
          "switch on option 53"]
    for i, t in enumerate(sv):
        box(ax, 8.6, 5.7 - i * 0.72, 4.8, 0.55, t, fs=8)
        if i:
            arrow(ax, 11.0, 5.7 - (i - 1) * 0.72, 11.0, 5.7 - i * 0.72 + 0.55, lw=1)
    cases = ["DISCOVER\nallocate()", "REQUEST\nACK / NAK", "RELEASE\nforget()",
             "DECLINE\nmark"]
    for i, t in enumerate(cases):
        box(ax, 8.6 + i * 1.22, 2.3, 1.12, 0.9, t, fs=7, fc=LIGHT["grey"])
        arrow(ax, 11.0, 3.54, 9.16 + i * 1.22, 3.2, lw=0.8)
    box(ax, 8.6, 0.6, 4.8, 1.1, "lease table  vector<Lease>\nMAC · IP · state · expires\n"
        "sendReply(): OFFER/ACK/NAK broadcast to :68", fs=7.5, fc=LIGHT["dhcp"], ec=DHCP_C)
    arrow(ax, 11.0, 2.3, 11.0, 1.7, lw=1)
    arrow(ax, 5.2, 7.0, 3.2, 6.75, lw=1, ls=":")
    arrow(ax, 8.8, 7.0, 11.0, 6.75, lw=1, ls=":")
    save(fig, "dhcp_structure.png")


# ------------------------------------------------------------------ ARP diagrams
def fig_arp_architecture():
    fig, ax = canvas(14, 7.4, (10, 5.3))
    box(ax, 4.6, 5.3, 4.8, 1.8, "lan_hub  (the switch)\nTCP server 127.0.0.1:9200\n"
        "select() over all host sockets\nREQUEST -> flood · REPLY -> port of tha",
        fc=LIGHT["arp"], ec=ARP_C, fs=8.5)
    for i, (name, ip, mac) in enumerate([("host A", "192.168.1.1", "02:..:01"),
                                         ("host B", "192.168.1.2", "02:..:02"),
                                         ("host C", "192.168.1.3", "02:..:03")]):
        x = 0.4 + i * 4.6
        ax.add_patch(FancyBboxPatch((x, 0.3), 4.0, 3.6, boxstyle="round,pad=0.02,rounding_size=0.1",
                                    fc="#fbfbfb", ec=GREY, lw=1.1))
        ax.text(x + 2.0, 3.6, f"{name}   {ip}   {mac}", ha="center", fontsize=8.5,
                fontweight="bold")
        box(ax, x + 0.2, 2.35, 1.75, 0.95, "main thread\nresolve · cache\ngrat · quit", fs=7)
        box(ax, x + 2.05, 2.35, 1.75, 0.95, "receiver\nthread\nhandleIncoming()", fs=7)
        box(ax, x + 0.2, 0.55, 3.6, 1.2, "ArpCache  map<IP, {MAC, expiry}>\n"
            "60 s TTL · mutex", fc=LIGHT["arp"], ec=ARP_C, fs=7.5)
        arrow(ax, x + 1.07, 2.35, x + 1.07, 1.75, lw=1, both=True)
        arrow(ax, x + 2.92, 2.35, x + 2.92, 1.75, lw=1, both=True)
        arrow(ax, x + 2.0, 3.9, 4.6 + 0.8 + i * 1.6, 5.3, color=ARP_C, both=True)
    ax.text(12.0, 4.75, "TCP connection =\ncable to a switch port\n(REG <ip> <mac>, then\n"
            "28-byte ARP packets)", ha="center", va="center", fontsize=8, color=GREY)
    save(fig, "arp_architecture.png")


def fig_arp_sequence():
    ladder("arp_sequence.png", ["host B .1.2", "lan_hub", "host A .1.1", "host C .1.3"], [
        (0, 1, "REQUEST", "who-has 192.168.1.1? tell 192.168.1.2 (tha = 0)", False),
        (1, [2, 3], "flood", "broadcast to every other port", True),
        (2, 1, "REPLY", "192.168.1.1 is-at 02:00:00:00:00:01", False),
        (1, 0, "unicast", "forwarded only to the port of tha = B's MAC", False),
    ], ARP_C, height_per=1.15, figsize=(9.5, 4.6), notes=[
        (0, -0.45, "cache miss"),
        (2, 1.45, "tpa = me: cache B, reply"),
        (3, 1.45, "not me: drop"),
        (0, 3.45, "cache 192.168.1.1 (60 s)"),
    ])


def fig_arp_structure():
    fig, ax = canvas(14, 5.4, (10, 3.9))
    box(ax, 0.2, 4.0, 4.2, 1.0, "portable_net.h\nsocket shims · recvAll()", fs=8,
        fc=LIGHT["grey"])
    box(ax, 4.9, 4.0, 4.2, 1.0, "arp_packet.h\nArpPacket (28 B) · makeRequest/Reply/\n"
        "Gratuitous · parse/print", fs=7.5, fc=LIGHT["arp"], ec=ARP_C)
    box(ax, 0.2, 0.4, 5.2, 2.8, "lan_hub.cpp\n\nmain(): bind :9200, listen\nloop: select()\n"
        "  accept -> new Client\n  REG -> registered, send OK\n  ARP: oper = ntohs(..)\n"
        "    REQUEST -> sendAll()\n    REPLY   -> sendToMac(tha)", fs=7.5)
    box(ax, 6.0, 0.4, 7.8, 2.8, "host.cpp\n\nmain(): connect, REG, start receiverThread, read "
        "commands\nresolve(): cache.lookup -> miss -> REQUEST -> poll cache 3 s\n"
        "receiverThread(): select + recvAll(28) -> handleIncoming()\n"
        "handleIncoming(): conflict? gratuitous? request-for-me? reply?\n"
        "ArpCache: lookup / update / refreshIfPresent / show", fs=7.5)
    for x in (2.3, 7.0):
        arrow(ax, x, 4.0, x if x < 5 else 9.9, 3.2, lw=1, ls=":")
    arrow(ax, 7.0, 4.0, 2.8, 3.2, lw=1, ls=":")
    arrow(ax, 2.3, 4.0, 9.9, 3.2, lw=1, ls=":")
    save(fig, "arp_structure.png")


# ------------------------------------------------------------------ result plots
def theory_success(p, attempts=3):
    q = (1 - p) ** 2                       # message and its reply both cross the lossy cable
    phase = 1 - (1 - q) ** attempts
    return phase ** 2


def theory_time_given_success(p, attempts=3, wait=4.096):
    q = (1 - p) ** 2
    ks = np.arange(1, attempts + 1)
    pk = q * (1 - q) ** (ks - 1)
    extra = (ks - 1) * wait                # each failed attempt costs one 4 s wait
    return 2 * (pk * extra).sum() / pk.sum()


def fig_dhcp_loss():
    path = os.path.join(RESULTS, "dhcp_loss.csv")
    if not os.path.exists(path):
        print("skip dhcp_loss.png (no CSV)")
        return
    df = pd.read_csv(path)
    g = df.groupby("loss_pct")
    loss = np.array(sorted(df.loss_pct.unique()))
    succ = g.success.mean().values
    ok = df[df.success == 1].groupby("loss_pct")
    t_ok = ok.seconds.mean().reindex(loss).values
    p_line = np.linspace(0, 0.55, 100)

    fig, axs = plt.subplots(1, 3, figsize=(12, 3.6))
    ax = axs[0]
    ax.plot(p_line * 100, [theory_success(p) * 100 for p in p_line], color=GREY, lw=1.2,
            label="model, 3 tries per step")
    k = g.success.sum().values
    nn = g.success.count().values
    z = 1.96
    ph = k / nn
    centre = (ph + z * z / (2 * nn)) / (1 + z * z / nn)
    half = z * np.sqrt(ph * (1 - ph) / nn + z * z / (4 * nn * nn)) / (1 + z * z / nn)
    ax.plot(p_line * 100, [(1 - (1 - (1 - p) ** 2) ** 3) * (1 - p) ** 2 * 100 for p in p_line],
            ":", color=GREY, lw=1.2, label="model, REQUEST sent once")
    ax.errorbar(loss, succ * 100, yerr=[(ph - (centre - half)) * 100, ((centre + half) - ph) * 100],
                fmt="o", color=DHCP_C, capsize=3, label="measured (95% CI)")
    ax.set(xlabel="packet loss on pc1's cable (%)", ylabel="leases obtained (%)",
           title="Success rate", ylim=(0, 105))
    ax.legend(fontsize=8, loc="lower left")
    ax.grid(alpha=0.3)

    ax = axs[1]
    ax.plot(p_line * 100, [theory_time_given_success(p) for p in p_line], color=GREY,
            lw=1.2, label="model (4.1 s per retry)")
    ax.plot(loss, t_ok, "o", color=DHCP_C, label="measured (successful runs)")
    ax.set(xlabel="packet loss on pc1's cable (%)", ylabel="seconds",
           title="Mean time to lease")
    ax.legend(fontsize=8, loc="upper left")
    ax.grid(alpha=0.3)

    ax = axs[2]
    d = g.discover_sent.mean().values
    r = g.request_sent.mean().values
    w = 3.2
    ax.bar(loss - w / 2, d, width=w, color=DHCP_C, label="DISCOVER")
    ax.bar(loss + w / 2, r, width=w, color="#7fc4c2", label="REQUEST")
    ax.set(xlabel="packet loss on pc1's cable (%)", ylabel="messages sent per run",
           title="Retransmissions", ylim=(0, 3.3))
    ax.legend(fontsize=8)
    ax.grid(alpha=0.3, axis="y")
    fig.tight_layout()
    save(fig, "dhcp_loss.png")


def fig_arp_traffic():
    path = os.path.join(RESULTS, "arp_traffic.csv")
    if not os.path.exists(path):
        print("skip arp_traffic.png (no CSV)")
        return
    df = pd.read_csv(path)
    n = df.hosts.values
    fig, axs = plt.subplots(1, 2, figsize=(10, 3.6))
    ax = axs[0]
    ax.plot(n, n * (n - 1), "--", color=GREY, label="lookups per round = N(N-1)")
    ax.plot(n, df.round1_requests, "o-", color=ARP_C, label="ARP requests, round 1")
    ax.plot(n, n * (n - 1) / 2, ":", color=ARP_C, label="N(N-1)/2")
    ax.plot(n, df.round2_requests, "s-", color="#2b6cb0", label="ARP requests, round 2")
    ax.set(xlabel="hosts N", ylabel="count", title="Requests on the LAN")
    ax.legend(fontsize=8)
    ax.grid(alpha=0.3)
    ax = axs[1]
    ax.plot(n, df.frames_delivered_round1, "o-", color=ARP_C,
            label="with target caching (measured requests)")
    ax.plot(n, n * (n - 1) * (n - 1) + n * (n - 1), "--", color=GREY,
            label="if targets did not cache the asker")
    ax.set(xlabel="hosts N", ylabel="frames", title="Frames delivered by the hub, round 1")
    ax.legend(fontsize=8)
    ax.grid(alpha=0.3)
    fig.tight_layout()
    save(fig, "arp_traffic.png")


if __name__ == "__main__":
    fig_dhcp_dora()
    fig_virtual_lan()
    fig_lease_states()
    fig_dhcp_structure()
    fig_arp_architecture()
    fig_arp_sequence()
    fig_arp_structure()
    fig_dhcp_loss()
    fig_arp_traffic()
