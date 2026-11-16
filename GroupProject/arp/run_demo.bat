@echo off
echo Start lan_hub.exe in another window FIRST!
echo.
echo Host1: 192.168.1.1 / 02:00:00:00:00:01
echo Host2: 192.168.1.2 / 02:00:00:00:00:02  resolves Host1
echo.
start "ARP-Host1" cmd /k host.exe 192.168.1.1 02:00:00:00:00:01
timeout /t 1 >nul
start "ARP-Host2" cmd /k host.exe 192.168.1.2 02:00:00:00:00:02 192.168.1.1
echo Hosts started.
