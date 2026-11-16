#!/bin/bash
echo "Start ./lan_hub in another terminal FIRST!"
echo "Starting two hosts..."
x-terminal-emulator -e ./host 192.168.1.1 02:00:00:00:00:01 2>/dev/null &
sleep 1
x-terminal-emulator -e ./host 192.168.1.2 02:00:00:00:00:02 192.168.1.1 2>/dev/null &
echo "If no new terminals opened, run the two host commands manually."
