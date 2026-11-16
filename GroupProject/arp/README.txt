ARP Mini Project — Cross Platform (Windows / Linux / macOS)
===========================================================

Address Resolution Protocol (main ARP):
  Request (broadcast), Reply (unicast), Cache + timeout, Gratuitous ARP

Files:
  portable_net.h   Socket helpers for Win / Unix
  arp_packet.h     ARP message format
  lan_hub.cpp      LAN hub (broadcast domain)
  host.cpp         Host with ARP cache
  Makefile
  run_demo.bat     Windows demo
  README.txt

------------------ COMPILE ------------------

Windows (MinGW):
  g++ -std=c++17 -o lan_hub.exe lan_hub.cpp -lws2_32
  g++ -std=c++17 -o host.exe host.cpp -lws2_32
  OR:  make windows

Linux:
  g++ -std=c++17 -o lan_hub lan_hub.cpp
  g++ -std=c++17 -o host host.cpp -pthread
  OR:  make linux

macOS:
  g++ -std=c++17 -o lan_hub lan_hub.cpp
  g++ -std=c++17 -o host host.cpp -pthread
  OR:  make mac

------------------ RUN ------------------

Terminal 1:
  Windows:  .\lan_hub.exe
  Linux/Mac: ./lan_hub

Terminal 2:
  Windows:  .\host.exe 192.168.1.1 02:00:00:00:00:01
  Linux/Mac: ./host 192.168.1.1 02:00:00:00:00:01

Terminal 3:
  Windows:  .\host.exe 192.168.1.2 02:00:00:00:00:02 192.168.1.1
  Linux/Mac: ./host 192.168.1.2 02:00:00:00:00:02 192.168.1.1

Host commands:  resolve <ip> | cache | grat | quit

Try: in Terminal 2 type "grat" -> Terminal 3 refreshes its cache entry.
     Start a 3rd host with the SAME IP and a different MAC and type "grat"
     -> the original host prints an IP-conflict warning.

All hosts must run on the SAME machine (hub is localhost:9200).
