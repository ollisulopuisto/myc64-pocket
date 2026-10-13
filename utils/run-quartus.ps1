# Runs a Quartus compile on the shared Windows host (ristisana), following the
# rules agreed between the Pocket sessions:
#   - one registry file per running compile, E:\locks\<session>-<n>.txt, with
#     project dir, start time and PID; deleted when done
#   - start only with >= 6 GB free RAM and fewer than 4 live registry files
#     (stale = PID gone or older than 3 h; not counted, left to its owner)
#   - kill only own PIDs; never by image name; own directories only
# Called by build-remote.sh: powershell -File run-quartus.ps1 <quartus_sh> <project dir>
param([string]$Quartus, [string]$Project)

$regDir = 'E:\locks'
$reg = "$regDir\pocket-c64-1.txt"
New-Item -ItemType Directory -Force $regDir | Out-Null

function Get-LiveRuns {
  @(Get-ChildItem "$regDir\*.txt" -ErrorAction SilentlyContinue | Where-Object {
    if ($_.LastWriteTime -lt (Get-Date).AddHours(-3)) { return $false }
    $m = Select-String -Path $_.FullName -Pattern 'pid[:\s]+(\d+)' -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $m) { return $true }  # no PID recorded: count it
    [bool](Get-Process -Id $m.Matches[0].Groups[1].Value -ErrorAction SilentlyContinue)
  }).Count
}

while ($true) {
  $free = [int]((Get-CimInstance Win32_OperatingSystem).FreePhysicalMemory / 1MB)
  $runs = Get-LiveRuns
  if ($free -ge 6 -and $runs -lt 4) { break }
  Write-Host "waiting: $free GB free, $runs live runs in $regDir"
  Start-Sleep 30
}

$rc = 1
try {
  "session: Pocket Commodore (myc64-pocket)`r`nproject: $Project`r`nstart: $(Get-Date -Format s)`r`npid: $PID" | Set-Content $reg
  Set-Location $Project
  & $Quartus --flow compile ap_core.qpf
  $rc = $LASTEXITCODE
} finally {
  Remove-Item $reg -Force -ErrorAction SilentlyContinue
}
exit $rc
