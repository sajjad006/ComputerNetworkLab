#!/bin/bash
# Virtual LAN for the DHCP demo (Linux, run with sudo).
#
# Builds isolated "computers" with Linux network namespaces, connected by a virtual switch:
#
#              router namespace
#        +---------------------------+
#        |  br0 (switch)             |
#        |  192.168.50.1/24          |   <- dhcp_server runs here
#        +--+-----------+---------+--+
#           |           |         |      veth "cables"
#        +--+--+     +--+--+   +--+--+
#        | pc1 |     | pc2 |   | pc3 |   eth0 starts with NO IP address
#        +-----+     +-----+   +-----+
#
# Your real network is not touched. `sudo ./demo.sh cleanup` removes everything.

set -e
cd "$(dirname "$0")"

NET=192.168.50
PCS="pc1 pc2 pc3"

usage() {
    cat <<EOF
Usage: sudo ./demo.sh <command>

  setup              build the virtual LAN (router + pc1, pc2, pc3)
  server [lease] [n] run the DHCP server in the router (lease 120 s, n=3 addresses)
                     e.g. "server 120 2" gives only 2 addresses, so pc3 sees "pool exhausted"
  client <pc>        run our DHCP client on <pc> and apply the IP it gets
  ping <pc> [ip]     ping from <pc> (default: the router) and show its ARP cache
  status             show every PC's IP address
  show               picture of the whole LAN: IPs, MACs, gateways, ARP caches, switch table
  watch              the same picture, refreshed every second (leave it open during the demo)
  shell <pc|router>  open a terminal INSIDE that machine (try: ip a, ip route, ip neigh, ping)
  capture            show DHCP/ARP/ping packets on the switch live (tcpdump)
  capture-file       save the packets to dhcp_demo.pcap for Wireshark (Ctrl+C to stop)
  udhcpc <pc>        get a lease with BusyBox udhcpc instead (a standard client)
  reset <pc>         remove <pc>'s IP so it can ask again
  cleanup            delete the virtual LAN

Typical demo (one terminal each):
  T1: sudo ./demo.sh setup && sudo ./demo.sh server
  T2: sudo ./demo.sh watch          (or: capture, to see the packets)
  T3: sudo ./demo.sh client pc1 ; sudo ./demo.sh client pc2 ; sudo ./demo.sh ping pc1
  T4: sudo ./demo.sh shell pc1      (then: ip a, ip route, ip neigh, ping 192.168.50.101)
EOF
}

need_lan() {
    ip netns list | grep -qw router || { echo "Run: sudo ./demo.sh setup"; exit 1; }
}
need_machine() {
    [[ " router $PCS " == *" $1 "* ]] || { echo "Unknown machine '$1' (use: router $PCS)"; exit 1; }
    need_lan
}
need_pc() {
    [[ " $PCS " == *" $1 "* ]] || { echo "Unknown PC '$1' (use: $PCS)"; exit 1; }
    need_lan
}

build() {
    # Rebuild only when a source file is newer than the binary.
    if [ ! -x dhcp_server ] || [ DhcpServer.cpp -nt dhcp_server ] || [ dhcp_packet.h -nt dhcp_server ]; then
        g++ -std=c++17 -O2 -o dhcp_server DhcpServer.cpp
    fi
    if [ ! -x dhcp_client ] || [ Dhcp.cpp -nt dhcp_client ] || [ dhcp_packet.h -nt dhcp_client ]; then
        g++ -std=c++17 -O2 -o dhcp_client Dhcp.cpp
    fi
}

# One machine's line for `show`: interface, IPv4, MAC, gateway, then its ARP cache.
show_machine() {
    local ns=$1 dev=$2
    local ip mac gw
    ip=$(ip -n "$ns" -4 -o addr show dev "$dev" | awk '{print $4}' | head -1)
    mac=$(ip -n "$ns" -br link show dev "$dev" | awk '{print $3}')
    gw=$(ip -n "$ns" route show default | awk '{print $3}' | head -1)
    printf '\e[1m%-7s\e[0m %-5s IP \e[1;36m%-18s\e[0m MAC %s' "$ns" "$dev" "${ip:-(no IP)}" "$mac"
    [ -n "$gw" ] && printf '   gateway %s' "$gw"
    echo
    local arp
    arp=$(ip -n "$ns" neigh show dev "$dev" | grep -v '^fe80' | awk '{printf "%s%s -> %s %s", (n++ ? " | " : ""), $1, ($2=="lladdr" ? $3 : "?"), $NF}')
    printf '          ARP cache: %s\n' "${arp:-(empty)}"
}

show() {
    printf '\e[1mVirtual LAN %s.0/24\e[0m   %s\n\n' "$NET" "$(date +%T)"
    show_machine router br0
    for pc in $PCS; do show_machine "$pc" eth0; done
    # The switch learns which MAC is behind which port by watching source MACs.
    echo
    printf '\e[1mSwitch (br0) MAC table\e[0m: which MAC was seen on which port\n'
    ip netns exec router bridge fdb show br br0 | grep -v permanent | grep -E '^02:' |
        awk '{printf "          %s  on  %s\n", $1, $3}' | sort -k3
}

cleanup() {
    for ns in router $PCS; do ip netns del "$ns" 2>/dev/null || true; done
}

[ "$EUID" -eq 0 ] || { [ -z "$1" ] && usage; echo "Please run with sudo."; exit 1; }

case "$1" in
setup)
    build
    cleanup
    # The router: a bridge (virtual switch) that also has the server's IP.
    ip netns add router
    ip -n router link set lo up
    ip -n router link add br0 type bridge
    ip -n router addr add $NET.1/24 dev br0
    ip -n router link set br0 up
    # Each PC: a veth pair is a cable. One end is the PC's eth0, the other is a switch port.
    i=0
    for pc in $PCS; do
        i=$((i + 1))
        ip netns add "$pc"
        ip -n "$pc" link set lo up
        ip link add "tmp-$pc" type veth peer name "$pc-port"
        ip link set "tmp-$pc" netns "$pc"
        ip -n "$pc" link set "tmp-$pc" name eth0
        ip -n "$pc" link set eth0 address "02:00:00:00:00:0$i"
        ip -n "$pc" link set eth0 up
        # Our client uses a normal UDP socket, so the kernel must accept the OFFER before
        # the PC has an IP. Turn off the reverse-path check inside this namespace only.
        ip netns exec "$pc" sysctl -qw net.ipv4.conf.all.rp_filter=0 net.ipv4.conf.eth0.rp_filter=0
        ip link set "$pc-port" netns router
        ip -n router link set "$pc-port" master br0 up
    done
    echo "Virtual LAN ready: router ($NET.1) + $PCS (MACs 02:00:00:00:00:01..03, no IPs yet)."
    echo "Next: sudo ./demo.sh server"
    ;;
server)
    need_lan; build
    size=${3:-3}
    ip netns exec router ./dhcp_server br0 $NET.1 $NET.100 $NET.$((99 + size)) "${2:-120}"
    ;;
client)
    need_pc "$2"; build
    ip netns exec "$2" ./dhcp_client eth0 --apply
    echo; echo "== $2 now has:"
    ip -n "$2" -br addr show eth0
    ip -n "$2" route
    ;;
udhcpc)
    need_pc "$2"
    ip netns exec "$2" busybox udhcpc -i eth0 -f -q -n -s "$PWD/udhcpc_hook.sh"
    ip -n "$2" -br addr show eth0
    ;;
ping)
    need_pc "$2"
    ip netns exec "$2" ping -c 3 "${3:-$NET.1}" || true
    echo; echo "== ARP cache of $2 (ip neigh):"
    ip -n "$2" neigh
    ;;
show)
    need_lan
    show
    ;;
watch)
    need_lan
    exec watch -c -t -n 1 "$PWD/$(basename "$0")" show
    ;;
shell)
    need_machine "$2"
    echo "You are now inside $2. Try: ip a | ip route | ip neigh | ping 192.168.50.1 | exit"
    ip netns exec "$2" env PS1="\[\e[1;32m\][$2]\[\e[0m\] \W # " bash --norc -i
    ;;
status)
    need_lan
    for pc in $PCS; do printf '%-4s ' "$pc"; ip -n "$pc" -br addr show eth0; done
    ;;
reset)
    need_pc "$2"
    ip -n "$2" addr flush dev eth0
    ip -n "$2" neigh flush dev eth0
    echo "$2 has no IP address now."
    ;;
capture)
    need_lan
    ip netns exec router tcpdump -Z root -l -n -e -i br0 'port 67 or port 68 or arp or icmp'
    ;;
capture-file)
    need_lan
    ip netns exec router tcpdump -Z root -n -i br0 -w dhcp_demo.pcap 'port 67 or port 68 or arp or icmp' || true
    chmod a+r dhcp_demo.pcap 2>/dev/null || true
    echo "Saved dhcp_demo.pcap. Open it with: wireshark dhcp_demo.pcap"
    ;;
cleanup)
    cleanup
    rm -f dhcp_demo.pcap
    echo "Virtual LAN removed."
    ;;
*)
    usage
    ;;
esac
