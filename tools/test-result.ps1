param([ValidateSet('Victory','Defeat')][string]$Outcome='Victory')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$bin=Join-Path $root 'out/game/Release'
$logs=Join-Path $root $(if($Outcome -eq 'Victory'){'evidence/result'}else{'evidence/defeat'})
$expected=if($Outcome -eq 'Victory'){1}else{2}
$mode=if($expected -eq 1){'defend'}else{'defeat'}
$count=if($expected -eq 1){2}else{4}
New-Item -ItemType Directory -Force $logs | Out-Null
$owned=@()
try {
    $server=Start-Process "$bin/outpost_server.exe" -ArgumentList '95' -PassThru -WindowStyle Hidden -RedirectStandardOutput "$logs/server.log" -RedirectStandardError "$logs/server-error.log"
    $owned+=$server
    Start-Sleep -Milliseconds 500
    $names=if($expected -eq 1){@('first','second')}else{@(1..$count | ForEach-Object {"client-$_"})}
    foreach($name in $names) {
        $probe=Start-Process "$bin/outpost_probe.exe" -ArgumentList @('127.0.0.1',$mode) -PassThru -WindowStyle Hidden -RedirectStandardOutput "$logs/$name.log" -RedirectStandardError "$logs/$name-error.log"
        $owned+=$probe
    }
    foreach($process in $owned){if(-not $process.WaitForExit(100000)){throw 'Timeout'};$process.Refresh();if($process.ExitCode -ne 0){throw "Process failed: inspect $logs"}}
    $results=@(foreach($name in $names) {
        $text=Get-Content "$logs/$name.log" -Raw
        if($text -notmatch "tick=(\d+).*core=([\d.]+) outcome=$expected"){throw 'Missing expected result'}
        "$($Matches[1]):$($Matches[2])"
    })
    if(@($results | Select-Object -Unique).Count -ne 1){throw 'Final tick/core differ'}
    if((Get-Content "$logs/server.log" -Raw) -notmatch "outcome=$expected"){throw 'Server result changed'}
    foreach($name in $names){Get-Content "$logs/$name.log"}
    Get-Content "$logs/server.log"
} finally {foreach($process in $owned){if(-not $process.HasExited){Stop-Process -Id $process.Id}}}
