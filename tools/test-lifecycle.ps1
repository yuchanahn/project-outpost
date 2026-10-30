param([string]$Bin = "$PSScriptRoot/../out/game/Release", [string]$Logs = "$PSScriptRoot/../evidence/integration/lifecycle")
$ErrorActionPreference='Stop'
$Bin=(Resolve-Path $Bin).Path
New-Item -ItemType Directory -Force $Logs | Out-Null
$Logs=(Resolve-Path $Logs).Path
$owned=@()
try {
    $server=Start-Process "$Bin/outpost_server.exe" -ArgumentList '12' -PassThru -WindowStyle Hidden -RedirectStandardOutput "$Logs/server.log" -RedirectStandardError "$Logs/server-error.log"
    $owned+=$server
    Start-Sleep -Milliseconds 500
    if($server.HasExited){throw 'Server failed to start'}
    $test=Start-Process "$Bin/net_lifecycle.exe" -PassThru -WindowStyle Hidden -RedirectStandardOutput "$Logs/client.log" -RedirectStandardError "$Logs/client-error.log"
    $owned+=$test
    foreach($process in $owned) {
        if(-not $process.WaitForExit(20000)){throw "Timeout: $($process.Id)"}
        $process.Refresh()
        if($process.ExitCode -ne 0){throw "Exit $($process.ExitCode): inspect $Logs"}
    }
    if((Get-Content "$Logs/server.log" -Raw) -notmatch 'outcome=3'){throw 'Last-human cleanup missing'}
    Get-Content "$Logs/client.log"
    Get-Content "$Logs/server.log"
} finally {foreach($process in $owned){if(-not $process.HasExited){Stop-Process -Id $process.Id}}}
