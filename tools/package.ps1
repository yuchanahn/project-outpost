$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$bin=Join-Path $root 'out/game/Release'
$destination=Join-Path $root "out/dist/Outpost-$(Get-Date -Format yyyyMMdd-HHmmss)"
New-Item -ItemType Directory $destination | Out-Null
foreach($name in @('server','client','probe')){Copy-Item -LiteralPath "$bin/outpost_$name.exe" -Destination $destination}
Copy-Item -LiteralPath "$root/docs/lan-check.md","$root/docs/dependencies.md","$root/docs/measurements.md" -Destination $destination
Get-ChildItem $destination -File | Get-FileHash -Algorithm SHA256 | Select-Object @{Name='File';Expression={Split-Path $_.Path -Leaf}},Hash | ConvertTo-Json | Set-Content "$destination/hashes.json"
Compress-Archive -LiteralPath $destination -DestinationPath "$destination.zip"
Write-Output "$destination.zip"
