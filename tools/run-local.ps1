param([ValidateRange(1,4)][int]$Clients=1,[ValidateSet('Debug','Release')][string]$Configuration='Release')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$bin=Join-Path $root "out/game/$Configuration"
if(-not(Test-Path "$bin/outpost_server.exe")){throw 'Build the selected configuration first. See README.'}
$owned=@()
try {
    $server=Start-Process "$bin/outpost_server.exe" -WindowStyle Hidden -PassThru
    $owned+=$server
    Start-Sleep -Milliseconds 500
    if($server.HasExited){throw 'Server failed to start; check UDP port 27015.'}
    Write-Host "Server PID $($server.Id). Close all game windows to stop this local session."
    $players=@()
    for($i=0;$i -lt $Clients;$i++) {
        $player=Start-Process "$bin/outpost_client.exe" -ArgumentList '127.0.0.1' -PassThru
        $owned+=$player
        $players+=$player
    }
    while(@($players | Where-Object {-not $_.HasExited}).Count -gt 0) {
        if($server.HasExited){throw 'Server stopped unexpectedly.'}
        Start-Sleep -Milliseconds 250
    }
} finally {foreach($process in $owned){if(-not $process.HasExited){Stop-Process -Id $process.Id}}}
