# Test Bayesian System Latency

$testCases = @(
    @{ pattern = "AnimNotify_Phase" }
    @{ pattern = "CombatComponent" }
    @{ pattern = "AttackData" }
)

Write-Host "Testing Bayesian Learning System Latency" -ForegroundColor Cyan
Write-Host "=========================================" -ForegroundColor Cyan
Write-Host ""

$totalTime = 0
$iterations = 10

foreach ($test in $testCases) {
    $times = @()

    for ($i = 0; $i -lt $iterations; $i++) {
        $start = Get-Date
        & "$PSScriptRoot/learning-tracker.ps1" -Action query -Pattern $test.pattern | Out-Null
        $end = Get-Date
        $latency = ($end - $start).TotalMilliseconds
        $times += $latency
    }

    $avgLatency = [Math]::Round(($times | Measure-Object -Average).Average, 1)
    $minLatency = [Math]::Round(($times | Measure-Object -Minimum).Minimum, 1)
    $maxLatency = [Math]::Round(($times | Measure-Object -Maximum).Maximum, 1)
    $totalTime += $avgLatency

    Write-Host "$($test.pattern): avg=$avgLatency ms, min=$minLatency ms, max=$maxLatency ms" -ForegroundColor Gray
}

$overallAvg = [Math]::Round($totalTime / $testCases.Count, 1)

Write-Host ""
Write-Host "Overall Average Latency: $overallAvg ms" -ForegroundColor Green
Write-Host ""

if ($overallAvg -lt 50) {
    Write-Host "[OK] Latency is excellent (<50ms)" -ForegroundColor Green
} elseif ($overallAvg -lt 100) {
    Write-Host "[OK] Latency is good (<100ms)" -ForegroundColor Yellow
} else {
    Write-Host "[WARNING] Latency needs optimization (>100ms)" -ForegroundColor Red
}

Write-Host ""
Write-Host "Comparison to target:" -ForegroundColor Cyan
Write-Host "  Phase 1 Target: <50ms" -ForegroundColor Gray
Write-Host "  Current: $overallAvg ms" -ForegroundColor $(if ($overallAvg -lt 50) { "Green" } else { "Yellow" })
Write-Host "  Overall Budget: <150ms (with holistic detector)" -ForegroundColor Gray
