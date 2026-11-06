# ==============================================================================
# Gateway (GW) Network Protocol - Multi-Client Concurrent Stress & Loss Checker
# ==============================================================================
param (
    [int]$NumClients = 5,
    [int]$PacketsPerClient = 50,
    [int]$Port = 13400,
    [string]$ServerIp = "127.0.0.1",
    [switch]$VerboseLog
)

$ErrorActionPreference = "Stop"

Write-Host "============================================================" -ForegroundColor Cyan
Write-Host " Gateway Multi-Client Packet Transfer & Loss Test" -ForegroundColor Cyan
Write-Host "   Target:             ${ServerIp}:${Port}" -ForegroundColor Gray
Write-Host "   Simultaneous Clients: $NumClients" -ForegroundColor Gray
Write-Host "   Packets per Client: $PacketsPerClient" -ForegroundColor Gray
Write-Host "   Total Packets:      $($NumClients * $PacketsPerClient)" -ForegroundColor Gray
Write-Host "============================================================`n" -ForegroundColor Cyan

# 1. Start Server in background if not already listening
$serverLog = "$PSScriptRoot\server_multi.log"
if (Test-Path $serverLog) { Remove-Item $serverLog -Force }

$serverRunning = Get-NetTCPConnection -LocalPort $Port -ErrorAction SilentlyContinue
$serverProc = $null

if (-not $serverRunning) {
    Write-Host "[SETUP] Launching Gateway Server on port $Port..." -ForegroundColor Yellow
    $serverArgs = "--port $Port --log-file `"$serverLog`""
    if ($VerboseLog) { $serverArgs += " --verbose" }
    $serverProc = Start-Process -FilePath "$PSScriptRoot\gw_server.exe" -ArgumentList $serverArgs -PassThru -NoNewWindow
    Start-Sleep -Milliseconds 600
} else {
    Write-Host "[SETUP] Gateway Server already listening on port $Port." -ForegroundColor Green
}

# 2. Launch clients simultaneously using PowerShell Jobs
Write-Host "[EXEC] Spawning $NumClients simultaneous client processes..." -ForegroundColor Yellow

$jobs = @()
$startTime = [System.Diagnostics.Stopwatch]::StartNew()

for ($i = 1; $i -le $NumClients; $i++) {
    $cid = "$i"
    $jobs += Start-Job -ScriptBlock {
        param($exe, $ip, $p, $cid, $cnt)
        & $exe --ip $ip --port $p --client-id $cid --count $cnt
    } -ArgumentList "$PSScriptRoot\gw_client.exe", $ServerIp, $Port, $cid, $PacketsPerClient
}

# 3. Wait for all clients to finish
Write-Host "[EXEC] Waiting for all client jobs to finish transfer..." -ForegroundColor Yellow
$outputs = $jobs | Wait-Job | Receive-Job
$jobs | Remove-Job
$startTime.Stop()

# 4. Parse client outputs
Write-Host "`n--- Client Execution Logs ---" -ForegroundColor Cyan
$outputs | ForEach-Object { Write-Host $_ }

$totalSent = 0
$totalReceived = 0
$totalLost = 0

foreach ($line in $outputs) {
    if ($line -match "Summary: Sent=(\d+) \| Received=(\d+) \| Lost=(\d+)") {
        $totalSent += [int]$matches[1]
        $totalReceived += [int]$matches[2]
        $totalLost += [int]$matches[3]
    }
}

# 5. Stop server if we started it
if ($serverProc -and -not $serverProc.HasExited) {
    Write-Host "`n[CLEANUP] Stopping background Gateway Server..." -ForegroundColor Yellow
    Stop-Process -Id $serverProc.Id -Force
}

# 6. Final Summary and Loss Rate
$lossRate = if ($totalSent -gt 0) { ($totalLost / $totalSent) * 100.0 } else { 0.0 }
$throughput = if ($startTime.Elapsed.TotalSeconds -gt 0) { [math]::Round($totalSent / $startTime.Elapsed.TotalSeconds, 1) } else { 0 }

Write-Host "`n============================================================" -ForegroundColor Cyan
Write-Host " Multi-Client Test Results & Loss Verification" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "  Concurrent Clients:      $NumClients"
Write-Host "  Elapsed Time:            $([math]::Round($startTime.Elapsed.TotalSeconds, 2)) seconds"
Write-Host "  Throughput:              $throughput packets/sec"
Write-Host "  Total Packets Sent:      $totalSent"
Write-Host "  Total Packets Received:  $totalReceived"
if ($totalLost -eq 0) {
    Write-Host "  Total Packets Lost:      $totalLost (0.00% packet loss - ZERO-DROP OK)" -ForegroundColor Green
} else {
    Write-Host "  Total Packets Lost:      $totalLost ($([math]::Round($lossRate, 2))% packet loss)" -ForegroundColor Red
}

if (Test-Path $serverLog) {
    Write-Host "`n  Server Log Written to:   $serverLog" -ForegroundColor Gray
}
Write-Host "============================================================`n" -ForegroundColor Cyan
