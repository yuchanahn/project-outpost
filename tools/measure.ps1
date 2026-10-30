param([ValidateSet('Load','Render')][string]$Kind='Load',[ValidateRange(1,32)][int]$Window=1)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$bin=Join-Path $root 'out/game/Release'
$logs=Join-Path $root "evidence/$($Kind.ToLower())"
if($Kind -eq 'Load' -and $Window -ne 1){$logs=Join-Path $logs "window-$Window"}
New-Item -ItemType Directory -Force $logs | Out-Null
if($Kind -eq 'Load') {
    foreach($count in @(8,32,64)) {
        $server=Start-Process "$bin/outpost_net_bench.exe" -ArgumentList 'server' -PassThru -WindowStyle Hidden -RedirectStandardOutput "$logs/server-$count.log" -RedirectStandardError "$logs/server-$count-error.log"
        try {
            Start-Sleep -Milliseconds 500
            & "$bin/outpost_net_bench.exe" $count $Window *> "$logs/clients-$count.log"
            $code=$LASTEXITCODE
            if(-not $server.WaitForExit(15000)){throw 'Server timeout'}
            $server.Refresh()
            if($code -ne 0 -or $server.ExitCode -ne 0){throw "Load failed: inspect $logs"}
            Get-Content "$logs/clients-$count.log","$logs/server-$count.log"
        } finally {if(-not $server.HasExited){Stop-Process -Id $server.Id}}
    }
} else {
    $priorCapture=$env:OUTPOST_CAPTURE
    try {
        foreach($mode in @('baseline','instanced')) {
            $env:OUTPOST_CAPTURE="$logs/$mode.bmp"
            & "$bin/outpost_render_bench.exe" $mode *> "$logs/$mode.log"
            if($LASTEXITCODE -ne 0){throw "Render failed: inspect $logs"}
            Get-Content "$logs/$mode.log"
        }
        if((Get-FileHash "$logs/baseline.bmp").Hash -ne (Get-FileHash "$logs/instanced.bmp").Hash){throw 'Framebuffer mismatch'}
    } finally {$env:OUTPOST_CAPTURE=$priorCapture}
}
