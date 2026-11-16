#!/bin/sh
# Called by BusyBox udhcpc (see demo.sh). $1 is the event; the lease comes in env vars.
case "$1" in
    deconfig)
        ip addr flush dev "$interface"
        ;;
    bound|renew)
        echo "udhcpc got: ip=$ip mask=$mask router=$router dns=$dns lease=$lease"
        ip addr flush dev "$interface"
        ip addr add "$ip/$mask" dev "$interface"
        [ -n "$router" ] && ip route replace default via "${router%% *}" dev "$interface"
        ;;
esac
exit 0
