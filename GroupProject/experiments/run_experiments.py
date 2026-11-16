#!/usr/bin/env python3
"""Experiments for the DHCP + ARP group project report.

    sudo python3 run_experiments.py [dhcp-loss] [capture] [arp-traffic] [arp-scenario]

With no arguments every experiment runs. Results go to ../results/.
Without sudo the script re-runs itself inside an unprivileged user + network namespace
(`unshare -rnm`), which behaves the same for everything measured here.

  dhcp-loss     DHCP lease time and success rate on the virtual LAN while netem drops a
                given percentage of packets on pc1's cable (both directions).
  capture       Raw capture on the virtual switch during two leases and two pings
                -> results/dhcp_arp_capture.pcap (+ lease table and LAN snapshot).
  arp-traffic   ARP requests seen by the hub when every host resolves every other host,
                twice, for N = 2..8 hosts (shows the effect of the cache).
  arp-scenario  Scripted run of the ARP demo (resolve, cache hit, timeout, gratuitous ARP,
                IP conflict) -> results/arp_*.log
"""
import csv
import os
import re
import signal
import socket
import struct
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
DHCP = os.path.join(ROOT, "dhcp")
ARP = os.path.join(ROOT, "arp")
RESULTS = os.path.join(ROOT, "results")
NET = "192.168.50"


def sh(cmd, **kw):
    return subprocess.run(cmd, shell=True, check=True, **kw)


def ensure_root():
    """Re-exec inside a user+net+mount namespace when not root."""
    if os.geteuid() == 0:
        if os.environ.get("CNLAB_USERNS") == "1":
            sh("mount -t tmpfs none /run")      # private /run so `ip netns` works
            sh("ip link set lo up")             # the ARP hub listens on 127.0.0.1
        return
    os.environ["CNLAB_USERNS"] = "1"
    os.execvp("unshare", ["unshare", "-rnm", "--propagation", "private",
                          sys.executable] + sys.argv)


# ------------------------------------------------------------------ DHCP helpers
def lan_setup():
    sh("./demo.sh setup", cwd=DHCP, stdout=subprocess.DEVNULL)


def lan_cleanup():
    subprocess.run("./demo.sh cleanup", shell=True, cwd=DHCP, stdout=subprocess.DEVNULL)


def start_server(log_path, lease=3600, last=102):
    log = open(log_path, "w")
    return subprocess.Popen(
        ["ip", "netns", "exec", "router", "./dhcp_server", "br0", f"{NET}.1",
         f"{NET}.100", f"{NET}.{last}", str(lease)],
        cwd=DHCP, stdout=log, stderr=subprocess.STDOUT)


def run_client(pc, apply=False):
    cmd = ["ip", "netns", "exec", pc, "./dhcp_client", "eth0"] + (["--apply"] if apply else [])
    t0 = time.monotonic()
    p = subprocess.run(cmd, cwd=DHCP, capture_output=True, text=True)
    return p.returncode, time.monotonic() - t0, p.stdout + p.stderr


def set_loss(pc, percent):
    for ns, dev in ((pc, "eth0"), ("router", f"{pc}-port")):
        subprocess.run(f"ip netns exec {ns} tc qdisc del dev {dev} root", shell=True,
                       stderr=subprocess.DEVNULL)
        if percent > 0:
            sh(f"ip netns exec {ns} tc qdisc add dev {dev} root netem loss {percent}%")


# ------------------------------------------------------------------ Experiment: DHCP under loss
def exp_dhcp_loss(levels=(0, 10, 20, 30, 40, 50), trials=30):
    lan_setup()
    server = start_server(os.path.join(RESULTS, "dhcp_loss_server.log"))
    time.sleep(0.5)
    rows = []
    try:
        for loss in levels:
            set_loss("pc1", loss)
            for t in range(trials):
                code, secs, out = run_client("pc1")
                rows.append({
                    "loss_pct": loss, "trial": t, "success": int(code == 0),
                    "seconds": round(secs, 4),
                    "discover_sent": out.count("Sent DHCPDISCOVER"),
                    "request_sent": out.count("Sent DHCPREQUEST"),
                })
                ok = sum(r["success"] for r in rows if r["loss_pct"] == loss)
                print(f"loss {loss:2d}%  trial {t + 1:2d}/{trials}  "
                      f"{'ok ' if code == 0 else 'FAIL'} {secs:6.3f}s  ({ok} ok so far)",
                      flush=True)
            set_loss("pc1", 0)
    finally:
        server.terminate()
        server.wait()
        lan_cleanup()
    with open(os.path.join(RESULTS, "dhcp_loss.csv"), "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0]))
        w.writeheader()
        w.writerows(rows)


# ------------------------------------------------------------------ Experiment: packet capture
def sniff(ns_iface, pcap_path, stop_flag_path):
    """Child process: raw AF_PACKET capture written as a pcap file."""
    s = socket.socket(socket.AF_PACKET, socket.SOCK_RAW, socket.htons(0x0003))
    s.bind((ns_iface, 0))
    s.settimeout(0.2)
    with open(pcap_path, "wb") as f:
        f.write(struct.pack("<IHHiIII", 0xA1B2C3D4, 2, 4, 0, 0, 65535, 1))
        while not os.path.exists(stop_flag_path):
            try:
                data = s.recv(65535)
            except socket.timeout:
                continue
            ts = time.time()
            f.write(struct.pack("<IIII", int(ts), int((ts % 1) * 1e6), len(data), len(data)))
            f.write(data)
            f.flush()


def exp_capture():
    lan_setup()
    pcap = os.path.join(RESULTS, "dhcp_arp_capture.pcap")
    stop = os.path.join(RESULTS, ".stop_capture")
    if os.path.exists(stop):
        os.remove(stop)
    server = start_server(os.path.join(RESULTS, "capture_server.log"), lease=120)
    sniffer = subprocess.Popen(["ip", "netns", "exec", "router", sys.executable, __file__,
                                "_sniff", "br0", pcap, stop])
    time.sleep(1)
    try:
        transcript = []
        for pc in ("pc1", "pc2"):
            code, secs, out = run_client(pc, apply=True)
            transcript.append(f"$ sudo ./demo.sh client {pc}\n{out}")
        for cmd in ("ping pc1", f"ping pc2 {NET}.100"):
            out = subprocess.run(f"./demo.sh {cmd}", shell=True, cwd=DHCP,
                                 capture_output=True, text=True).stdout
            transcript.append(f"$ sudo ./demo.sh {cmd}\n{out}")
        show = subprocess.run("./demo.sh show", shell=True, cwd=DHCP,
                              capture_output=True, text=True).stdout
        transcript.append("$ sudo ./demo.sh show\n" + re.sub(r"\x1b\[[0-9;]*m", "", show))
        time.sleep(0.5)
        with open(os.path.join(RESULTS, "capture_transcript.txt"), "w") as f:
            f.write("\n".join(transcript))
    finally:
        open(stop, "w").close()
        sniffer.wait()
        os.remove(stop)
        server.terminate()
        server.wait()
        lan_cleanup()
    print("capture written:", pcap)


# ------------------------------------------------------------------ ARP helpers
def arp_build():
    sh("make linux", cwd=ARP, stdout=subprocess.DEVNULL)


def start_hub(log_path):
    log = open(log_path, "w")
    hub = subprocess.Popen(["stdbuf", "-o0", "./lan_hub"], cwd=ARP, stdout=log,
                           stderr=subprocess.STDOUT)
    time.sleep(0.4)
    return hub


def start_host(ip, mac, log_path, peer=None):
    log = open(log_path, "w")
    args = ["stdbuf", "-o0", "./host", ip, mac] + ([peer] if peer else [])
    h = subprocess.Popen(args, cwd=ARP, stdin=subprocess.PIPE, stdout=log,
                         stderr=subprocess.STDOUT, text=True)
    time.sleep(0.3)
    return h


def send(host, line, wait=0.15):
    host.stdin.write(line + "\n")
    host.stdin.flush()
    time.sleep(wait)


def stop_all(procs):
    for p in procs:
        if p.poll() is None:
            if p.stdin:
                try:
                    send(p, "quit", 0)
                except (BrokenPipeError, ValueError):
                    pass
            p.terminate()
    for p in procs:
        p.wait()


def count_hub(log_path):
    text = open(log_path).read()
    return text.count("ARP REQUEST"), text.count("ARP REPLY")


# ------------------------------------------------------------------ Experiment: ARP traffic
def exp_arp_traffic(max_hosts=8):
    arp_build()
    rows = []
    tmp = os.path.join(RESULTS, ".arp_tmp")
    os.makedirs(tmp, exist_ok=True)
    for n in range(2, max_hosts + 1):
        hub_log = os.path.join(tmp, "hub.log")
        hub = start_hub(hub_log)
        hosts = [start_host(f"192.168.1.{i + 1}", f"02:00:00:00:00:{i + 1:02x}",
                            os.path.join(tmp, f"h{i}.log")) for i in range(n)]
        time.sleep(0.3)
        per_round = []
        for rnd in (1, 2):
            before = count_hub(hub_log)
            for i in range(n):
                for j in range(n):
                    if i != j:
                        send(hosts[i], f"resolve 192.168.1.{j + 1}", 0.08)
            time.sleep(0.3)
            after = count_hub(hub_log)
            per_round.append((after[0] - before[0], after[1] - before[1]))
        hits = sum(open(os.path.join(tmp, f"h{i}.log")).read().count("cache hit")
                   for i in range(n))
        stop_all(hosts + [hub])
        rows.append({
            "hosts": n, "lookups_per_round": n * (n - 1),
            "round1_requests": per_round[0][0], "round1_replies": per_round[0][1],
            "round2_requests": per_round[1][0], "round2_replies": per_round[1][1],
            "cache_hits_total": hits,
            "frames_delivered_round1": per_round[0][0] * (n - 1) + per_round[0][1],
        })
        print(rows[-1], flush=True)
    with open(os.path.join(RESULTS, "arp_traffic.csv"), "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0]))
        w.writeheader()
        w.writerows(rows)
    sh("make clean", cwd=ARP, stdout=subprocess.DEVNULL)


# ------------------------------------------------------------------ Experiment: ARP scenario logs
def exp_arp_scenario():
    arp_build()
    L = lambda name: os.path.join(RESULTS, f"arp_{name}.log")
    hub = start_hub(L("hub"))
    a = start_host("192.168.1.1", "02:00:00:00:00:01", L("hostA"))
    b = start_host("192.168.1.2", "02:00:00:00:00:02", L("hostB"), peer="192.168.1.1")
    time.sleep(1.5)                       # B resolves A automatically
    send(b, "resolve 192.168.1.1", 0.3)   # cache hit
    send(b, "resolve 192.168.1.9", 3.5)   # timeout
    send(b, "cache", 0.3)
    send(a, "grat", 0.5)                  # B refreshes its entry
    fake = start_host("192.168.1.1", "02:00:00:00:00:99", L("hostFake"))
    send(fake, "grat", 0.5)               # A: conflict, B: entry overwritten
    send(b, "cache", 0.3)
    stop_all([fake])

    # Framing: two ARP requests glued together in ONE send() must both be answered.
    c = socket.create_connection(("127.0.0.1", 9200))
    c.sendall(b"REG 192.168.1.4 02:00:00:00:00:04\n")
    c.recv(32)
    mac, me = bytes.fromhex("020000000004"), bytes([192, 168, 1, 4])
    req = lambda t: (struct.pack("!HHBBH", 1, 0x0800, 6, 4, 1) + mac + me + bytes(6)
                     + bytes([192, 168, 1, t]))
    c.sendall(req(1) + req(2))
    time.sleep(1.0)
    c.settimeout(0.5)
    got = b""
    try:
        while True:
            chunk = c.recv(1024)
            if not chunk:
                break
            got += chunk
    except socket.timeout:
        pass
    c.close()
    replies = [got[i:i + 28] for i in range(0, len(got), 28)]
    answered = sorted(".".join(map(str, r[14:18])) for r in replies
                      if struct.unpack("!H", r[6:8])[0] == 2)
    with open(L("framing"), "w") as f:
        f.write(f"two requests in one send(): replies from {answered}\n")
    print("framing test: replies from", answered)
    stop_all([a, b, hub])
    sh("make clean", cwd=ARP, stdout=subprocess.DEVNULL)
    print("ARP scenario logs written to", RESULTS)


# ------------------------------------------------------------------ Experiment: DHCP functional tests
def raw_dhcp(mac_text, msg_type, requested=None, server_id=None, ciaddr=None, truncate=False):
    """Child process inside a PC namespace: send one hand-made DHCP message from port 68
    and print the type of the reply ('none' if nothing arrives within 2 s)."""
    mac = bytes.fromhex(mac_text.replace(":", ""))
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_BINDTODEVICE, b"eth0")
    s.bind(("0.0.0.0", 68))
    s.settimeout(2)
    ci = socket.inet_aton(ciaddr) if ciaddr else bytes(4)
    hdr = (struct.pack("!BBBBIHH", 1, 1, 6, 0, 0xC0FFEE, 0, 0x8000) + ci + bytes(12) + mac
           + bytes(10) + bytes(192) + struct.pack("!I", 0x63825363))
    opts = bytes([53, 1, msg_type])
    if requested:
        opts += bytes([50, 4]) + socket.inet_aton(requested)
    if server_id:
        opts += bytes([54, 4]) + socket.inet_aton(server_id)
    pkt = hdr + opts + (bytes([55, 40]) if truncate else bytes([255]))  # bad length, no END
    s.sendto(pkt, ("255.255.255.255", 67))
    try:
        while True:
            data = s.recv(1500)
            if data[4:8] == struct.pack("!I", 0xC0FFEE):
                i = data.index(bytes([53, 1]), 240)
                print({2: "OFFER", 5: "ACK", 6: "NAK"}.get(data[i + 2], data[i + 2]))
                return
    except socket.timeout:
        print("none")


def raw(pc, mac, msg_type, **kw):
    args = ["ip", "netns", "exec", pc, sys.executable, __file__, "_raw", mac, str(msg_type)]
    for k, v in kw.items():
        args += [f"--{k}", str(v)]
    return subprocess.run(args, capture_output=True, text=True).stdout.strip()


def pc_ip(pc):
    out = subprocess.run(f"ip -n {pc} -4 -o addr show dev eth0", shell=True,
                         capture_output=True, text=True).stdout.split()
    return out[3].split("/")[0] if len(out) > 3 else "(none)"


def exp_dhcp_functional():
    rows = []

    def check(tid, what, expected, observed):
        ok = expected == observed
        rows.append({"id": tid, "test": what, "expected": expected, "observed": observed,
                     "result": "PASS" if ok else "FAIL"})
        print(f"{tid:4s} {'PASS' if ok else 'FAIL'}  {what}: expected {expected}, got {observed}",
              flush=True)

    def lease(pc):
        code, _, out = run_client(pc, apply=True)
        return pc_ip(pc) if code == 0 else "no lease"

    log = os.path.join(RESULTS, "dhcp_functional_server.log")
    lan_setup()
    server = start_server(log, lease=120)
    time.sleep(0.5)
    try:
        check("F1", "pc1 obtains a lease", f"{NET}.100", lease("pc1"))
        check("F2", "pc2 (different MAC) gets the next address", f"{NET}.101", lease("pc2"))
        subprocess.run("./demo.sh reset pc1", shell=True, cwd=DHCP, stdout=subprocess.DEVNULL)
        check("F3", "pc1 asks again: same MAC, same address", f"{NET}.100", lease("pc1"))
        out = subprocess.run("./demo.sh udhcpc pc3", shell=True, cwd=DHCP,
                             capture_output=True, text=True).stdout
        check("F4", "BusyBox udhcpc (independent client) on pc3", f"{NET}.102", pc_ip("pc3"))
        m = "02:00:00:00:00:aa"
        check("F5", "REQUEST for an address not reserved for this MAC", "NAK",
              raw("pc3", m, 3, requested=f"{NET}.77", server_id=f"{NET}.1"))
        check("F6", "REQUEST naming another server (offer withdrawn)", "none",
              raw("pc3", m, 3, requested=f"{NET}.77", server_id=f"{NET}.9"))
        check("F7", "INIT-REBOOT REQUEST from an unknown client (RFC: stay silent)", "none",
              raw("pc3", m, 3, requested=f"{NET}.50"))
        check("F8", "malformed packet (option runs past the end) is ignored", "none",
              raw("pc3", m, 1, truncate=True))
        check("F9", "pool full (3 of 3 bound): DISCOVER from a 4th MAC", "none",
              raw("pc3", m, 1))
        raw("pc2", "02:00:00:00:00:02", 7, ciaddr=f"{NET}.101", server_id=f"{NET}.1")
        time.sleep(0.3)
        check("F10", "after pc2 sends RELEASE, the 4th MAC gets an OFFER", "OFFER",
              raw("pc3", m, 1))
        raw("pc3", "02:00:00:00:00:03", 4, requested=f"{NET}.102", server_id=f"{NET}.1")
        time.sleep(0.3)
        subprocess.run("./demo.sh reset pc3", shell=True, cwd=DHCP, stdout=subprocess.DEVNULL)
        check("F11", "after pc3 DECLINEs .102, .102 is blocked (pool now full)", "no lease",
              lease("pc3"))
        server_log = open(log).read()
        check("F12", "server logged the DECLINE and the pool-exhausted case", "yes",
              "yes" if "DECLINED" in server_log and "pool exhausted" in server_log else "no")
        check("F13", "client with a non-existent interface exits with an error", "1",
              str(subprocess.run(["ip", "netns", "exec", "pc1", "./dhcp_client", "nosuch"],
                                 cwd=DHCP, capture_output=True).returncode))
    finally:
        server.terminate()
        server.wait()
        lan_cleanup()
    with open(os.path.join(RESULTS, "dhcp_functional.csv"), "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0]))
        w.writeheader()
        w.writerows(rows)


def main():
    if len(sys.argv) > 1 and sys.argv[1] == "_sniff":
        sniff(*sys.argv[2:5])
        return
    if len(sys.argv) > 1 and sys.argv[1] == "_raw":
        mac, mtype, rest = sys.argv[2], int(sys.argv[3]), sys.argv[4:]
        kw = {rest[i][2:]: rest[i + 1] for i in range(0, len(rest), 2)}
        kw["truncate"] = kw.get("truncate") == "True"
        raw_dhcp(mac, mtype, **kw)
        return
    ensure_root()
    os.makedirs(RESULTS, exist_ok=True)
    wanted = sys.argv[1:] or ["dhcp-functional", "dhcp-loss", "capture", "arp-traffic",
                              "arp-scenario"]
    table = {"dhcp-functional": exp_dhcp_functional, "dhcp-loss": exp_dhcp_loss,
             "capture": exp_capture, "arp-traffic": exp_arp_traffic,
             "arp-scenario": exp_arp_scenario}
    for name in wanted:
        print(f"=== {name}", flush=True)
        table[name]()
    # binaries built by demo.sh during the run
    for f in ("dhcp_client", "dhcp_server"):
        p = os.path.join(DHCP, f)
        if os.path.exists(p):
            os.remove(p)


if __name__ == "__main__":
    signal.signal(signal.SIGINT, signal.default_int_handler)
    main()
