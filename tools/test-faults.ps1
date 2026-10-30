param([string]$Python='python',[int]$Seed=42)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$bin=Join-Path $root 'out/game/Release'
$logs=Join-Path $root "evidence/faults/seed-$Seed"
New-Item -ItemType Directory -Force $logs | Out-Null
$owned=@()
try {
    $server=Start-Process "$bin/outpost_server.exe" -ArgumentList '16' -PassThru -WindowStyle Hidden -RedirectStandardOutput "$logs/server.log" -RedirectStandardError "$logs/server-error.log"
    $owned+=$server
    $relay=Start-Process $Python -ArgumentList @("`"$PSScriptRoot/fault-relay.py`"",'--seconds','15','--seed',"$Seed") -PassThru -WindowStyle Hidden -RedirectStandardOutput "$logs/relay.log" -RedirectStandardError "$logs/relay-error.log"
    $owned+=$relay
    Start-Sleep -Milliseconds 700
    foreach($mode in @('start','idle')) {
        $probe=Start-Process "$bin/outpost_probe.exe" -ArgumentList @('127.0.0.1',$mode,'27016') -PassThru -WindowStyle Hidden -RedirectStandardOutput "$logs/$mode.log" -RedirectStandardError "$logs/$mode-error.log"
        $owned+=$probe
    }
    foreach($process in $owned){if(-not $process.WaitForExit(22000)){throw 'Timeout'};$process.Refresh();if($process.ExitCode -ne 0){throw "Process failed: inspect $logs"}}
    $stats=(Get-Content "$logs/relay.log" | Select-Object -Last 1) | ConvertFrom-Json
    if($stats.dropped -eq 0 -or $stats.duplicated -eq 0 -or $stats.reordered -eq 0){throw 'Impairments not exercised'}
    if((Get-Content "$logs/server.log" -Raw) -notmatch 'outcome=3'){throw 'Final cleanup missing'}
    $slots=@(foreach($mode in @('start','idle')){
        $line=Get-Content "$logs/$mode.log" -Raw
        if($line -notmatch 'slot=(\d+)'){throw 'Missing slot'}
        $Matches[1]
    })
    if($slots[0] -eq $slots[1]){throw 'Duplicate admitted slots'}
    Get-Content "$logs/start.log","$logs/idle.log","$logs/relay.log"
    Write-Output 'Fault scenario PASS'
} finally {foreach($process in $owned){if(-not $process.HasExited){Stop-Process -Id $process.Id}}}
