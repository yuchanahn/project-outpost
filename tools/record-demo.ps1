param([string]$Ffmpeg='ffmpeg')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$bin=Join-Path $root 'out/game/Release'
$logs=Join-Path $root "evidence/demo/$(Get-Date -Format yyyyMMdd-HHmmss)"
$frames=Join-Path $logs 'frames'
New-Item -ItemType Directory -Force $frames | Out-Null
$owned=@();$prior=$env:OUTPOST_FRAMES
try {
    $server=Start-Process "$bin/outpost_server.exe" -ArgumentList '12' -PassThru -WindowStyle Hidden -RedirectStandardOutput "$logs/server.log" -RedirectStandardError "$logs/server-error.log"
    $owned+=$server
    Start-Sleep -Milliseconds 500
    $env:OUTPOST_FRAMES=$frames
    $first=Start-Process "$bin/outpost_client.exe" -ArgumentList @('127.0.0.1','smoke') -PassThru -RedirectStandardOutput "$logs/first.log" -RedirectStandardError "$logs/first-error.log"
    $owned+=$first
    $env:OUTPOST_FRAMES=$prior
    $second=Start-Process "$bin/outpost_client.exe" -ArgumentList @('127.0.0.1','smoke') -PassThru -RedirectStandardOutput "$logs/second.log" -RedirectStandardError "$logs/second-error.log"
    $owned+=$second
    foreach($process in $owned){if(-not $process.WaitForExit(20000)){throw 'Timeout'};$process.Refresh();if($process.ExitCode -ne 0){throw "Process failed: inspect $logs"}}
    & $Ffmpeg -hide_banner -loglevel error -y -framerate 10 -i "$frames/frame-%04d.bmp" -vf 'pad=ceil(iw/2)*2:ceil(ih/2)*2' -c:v libx264 -pix_fmt yuv420p "$logs/outpost.mp4"
    if($LASTEXITCODE -ne 0){throw 'Video encoding failed'}
    Write-Output "$logs/outpost.mp4"
} finally {$env:OUTPOST_FRAMES=$prior;foreach($process in $owned){if(-not $process.HasExited){Stop-Process -Id $process.Id}}}
