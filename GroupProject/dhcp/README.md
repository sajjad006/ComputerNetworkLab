# DHCP client + server

| File | What it is |
|---|---|
| `dhcp_packet.h` | DHCP packet layout and option helpers, shared by both programs |
| `Dhcp.cpp` | DHCP **client**: DISCOVER → OFFER → REQUEST → ACK (Linux / macOS / Windows) |
| `DhcpServer.cpp` | DHCP **server**: address pool, lease table, OFFER / ACK / NAK, RELEASE, DECLINE (Linux / macOS) |
| `demo.sh` | Builds a virtual LAN on Linux (router + 3 PCs) and runs the demo |
| `udhcpc_hook.sh` | Helper for `demo.sh udhcpc` (BusyBox's DHCP client against our server) |

## Demo on a virtual LAN (Linux, recommended)
Nothing on your real network is touched. Open three terminals in this folder:
```
T1:  sudo ./demo.sh setup          # router + pc1, pc2, pc3 (no IPs yet)
     sudo ./demo.sh server         # DHCP server on the router, prints its lease table
T2:  sudo ./demo.sh watch          # live picture: every machine's IP, MAC, ARP cache, switch table
     (or: sudo ./demo.sh capture     # live packets on the virtual switch: DHCP + ARP + ping)
T3:  sudo ./demo.sh client pc1     # pc1 gets 192.168.50.100 and configures eth0
     sudo ./demo.sh client pc2     # pc2 gets 192.168.50.101
     sudo ./demo.sh ping pc1       # ping the router, then show pc1's ARP cache
     sudo ./demo.sh ping pc2 192.168.50.100   # PC to PC
```
To get a terminal inside one machine: `sudo ./demo.sh shell pc1` (or `router`), then use
`ip a`, `ip route`, `ip neigh`, `ping 192.168.50.101`, `exit`.

More: `server 120 2` (only 2 addresses, so pc3 sees "pool exhausted"), `reset pc1` then
`client pc1` (same MAC gets the same IP back), `udhcpc pc3` (a standard client works with
our server), `capture-file` (save to `dhcp_demo.pcap` and open in Wireshark), `status`,
and finally `cleanup`.

## Client against a real router
```
g++ -std=c++17 -o dhcp_client Dhcp.cpp                       # macOS / Linux
g++ -std=c++17 -o dhcp_client Dhcp.cpp -lws2_32 -liphlpapi   # Windows (MinGW)
sudo ./dhcp_client                 # lists interfaces and asks which one
sudo ./dhcp_client en0             # macOS Wi-Fi (Linux: wlan0 / eth0, Windows: "Wi-Fi")
sudo ./dhcp_client en0 aa:bb:cc:dd:ee:ff   # only if macOS hides the interface MAC
```
It prints the OFFER and ACK and does not change your settings (unless `--apply`, Linux only).

**Never run `dhcp_server` on a shared network** (college / office Wi-Fi): it would hand
out addresses to other people's devices. Use the virtual LAN.
