# Two-PC LAN check — not yet executed

Use two Windows PCs on the same reachable private LAN. Release executables use the Microsoft C++ runtime; a machine with the same MSVC development setup already has it. Missing runtime DLL errors must be resolved before treating a network attempt as a network failure.

1. Build Release and run `tools/package.ps1`. Copy the resulting ZIP to the second PC and extract it. Keep executable paths stable.
2. On the server PC run `outpost_server.exe` in a terminal. Find its LAN IPv4 address with `ipconfig`. For LAN, the server executable must be allowed inbound on UDP 27015 for the intended network. The repository's `tools/firewall.ps1` prepares a narrow Private/LocalSubnet rule for the original build path when explicitly run in administrator PowerShell; a copied bundle has a different executable path. Handle Windows security permission dialogs directly.
3. On the server PC run `outpost_client.exe 127.0.0.1`. On the second PC run `outpost_client.exe <server-LAN-IPv4>`. Enter starts only after both have joined. Confirm distinct cyan local drones and matching wave/core. Move and fire from both machines.
4. Close the second PC's client. Confirm its drone disappears and is not replaced by a bot. Try launching it again while the first human remains: in-progress admission must fail. Close the final client; restart the server for another match.
5. Repeat with the second client's process terminated without quit; wait more than five seconds for final vacancy. A lost network may take the client's six-second receive-idle policy to display disconnection.
6. Save server/client stdout and stderr by redirecting to files, and record both OS/builds, executable hashes, network profile/address, observations and actual screenshots. Ctrl+C stops a directly launched server.

Example from the extracted bundle directory:

```powershell
./outpost_server.exe > server.log 2> server-error.log
# Different terminal / other PC:
./outpost_client.exe 192.168.1.20 > client.log 2> client-error.log
```

Substitute the actual IPv4 address. A UDP port is used; a TCP connection test does not establish UDP reachability. This checklist is a pending hardware validation procedure, not a passing LAN result.
