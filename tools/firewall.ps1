# Run explicitly once in an administrator PowerShell. Does not elevate itself.
param([ValidateSet('Debug','Release')][string]$Configuration='Release')
$ErrorActionPreference='Stop'
$program=(Resolve-Path "$PSScriptRoot/../out/game/$Configuration/outpost_server.exe").Path
$identity=[Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()
if(-not $identity.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)){throw 'Run this script from an administrator PowerShell once.'}
$name="Outpost-Development-$Configuration"
if(Get-NetFirewallRule -Name $name -ErrorAction SilentlyContinue){Write-Output "Rule already exists: $name";exit 0}
New-NetFirewallRule -Name $name -DisplayName $name -Direction Inbound -Action Allow -Program $program -Protocol UDP -LocalPort 27015 -Profile Private -RemoteAddress LocalSubnet
