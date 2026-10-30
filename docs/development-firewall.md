# Development firewall

The application listens on UDP 27015. Keep `out/game/Release/outpost_server.exe` at a stable path across rebuilds. Windows firewall permissions are tied to executable paths; Debug and Release use different paths.

For LAN, run `./tools/firewall.ps1 -Configuration Release` once from an administrator PowerShell. It creates an inbound rule for that exact executable, UDP 27015, Private profile and LocalSubnet. The script does not elevate itself. A UAC elevation prompt is distinct from a firewall access prompt.

The user handled the observed firewall dialog directly. The agent did not operate that security prompt or execute the firewall setup script. The resulting rule/profile selection has not been independently verified. Separate-PC LAN remains untested.
