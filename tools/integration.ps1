param([string]$Bin = "$PSScriptRoot/../out/game/Debug", [string]$Logs = "$PSScriptRoot/../evidence/integration/repeat")
$ErrorActionPreference='Stop'
$Bin=(Resolve-Path $Bin).Path
New-Item -ItemType Directory -Force $Logs | Out-Null
$Logs=(Resolve-Path $Logs).Path
$owned=@()
try {
    $server=Start-Process "$Bin/outpost_server.exe" -ArgumentList '12' -PassThru -WindowStyle Hidden -RedirectStandardOutput "$Logs/server.log" -RedirectStandardError "$Logs/server-error.log"
    $owned+=$server
    Start-Sleep -Milliseconds 500
    if($server.HasExited){throw 'Server could not start; check port 27015 and server log'}
    $first=Start-Process "$Bin/outpost_probe.exe" -ArgumentList @('127.0.0.1','start') -PassThru -WindowStyle Hidden -RedirectStandardOutput "$Logs/first.log" -RedirectStandardError "$Logs/first-error.log"
    $owned+=$first
    $second=Start-Process "$Bin/outpost_probe.exe" -ArgumentList '127.0.0.1' -PassThru -WindowStyle Hidden -RedirectStandardOutput "$Logs/second.log" -RedirectStandardError "$Logs/second-error.log"
    $owned+=$second
    foreach($process in $owned){if(-not $process.WaitForExit(20000)){throw "Timeout: $($process.Id)"};$process.Refresh();if($process.ExitCode -ne 0){throw "Exit $($process.ExitCode): $($process.Id)"}}
    $a=Get-Content "$Logs/first.log" -Raw;$b=Get-Content "$Logs/second.log" -Raw
    if($a -notmatch 'slot=(\d+)'){throw 'Missing first slot'};$slotA=$Matches[1]
    if($b -notmatch 'slot=(\d+)'){throw 'Missing second slot'};if($slotA -eq $Matches[1]){throw 'Duplicate slots'}
    if((Get-Content "$Logs/server.log" -Raw) -notmatch 'outcome=3'){throw 'Last-human cleanup missing'}
    Write-Output $a;Write-Output $b;Write-Output 'Integration PASS: two slots, running snapshots, clean final departure'
} finally {foreach($process in $owned){if(-not $process.HasExited){Stop-Process -Id $process.Id}}}
