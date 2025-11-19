# Test holistic system latency (with Bayesian learning v2.0)

$testCases = @(
    @{ file = "Source/KatanaCombat/Public/Animation/AnimNotify_Phase.h"; conv = "Implementing AnimNotify phase transitions" }
    @{ file = "Source/KatanaCombat/Private/Core/CombatComponent.cpp"; conv = "Fixing combo system input buffering" }
    @{ file = "Source/KatanaCombat/Public/Data/AttackData.h"; conv = "Configuring attack properties" }
)

Write-Host "Testing Holistic Mode Detector Latency (v2.0 Bayesian)" -ForegroundColor Cyan
Write-Host "======================================================" -ForegroundColor Cyan
Write-Host ""

$totalTime = 0
$iterations = 5

foreach ($test in $testCases) {
    $times = @()

    for ($i = 0; $i -lt $iterations; $i++) {
        $start = Get-Date
        & "$PSScriptRoot/holistic-mode-detector.ps1" -FilePath $test.file -ConversationText $test.conv | Out-Null
        $end = Get-Date
        $latency = ($end - $start).TotalMilliseconds
        $times += $latency
    }

    $avgLatency = [Math]::Round(($times | Measure-Object -Average).Average, 1)
    $minLatency = [Math]::Round(($times | Measure-Object -Minimum).Minimum, 1)
    $maxLatency = [Math]::Round(($times | Measure-Object -Maximum).Maximum, 1)
    $totalTime += $avgLatency

    $fileName = [System.IO.Path]::GetFileName($test.file)
    Write-Host "$fileName`: avg=$avgLatency ms, min=$minLatency ms, max=$maxLatency ms" -ForegroundColor Gray
}

$overallAvg = [Math]::Round($totalTime / $testCases.Count, 1)

Write-Host ""
Write-Host "Overall Average Latency: $overallAvg ms" -ForegroundColor Green
Write-Host ""

if ($overallAvg -lt 100) {
    Write-Host "[OK] Latency is excellent (<100ms)" -ForegroundColor Green
} elseif ($overallAvg -lt 200) {
    Write-Host "[OK] Latency is acceptable (<200ms)" -ForegroundColor Yellow
} else {
    Write-Host "[WARNING] Latency needs optimization (>200ms)" -ForegroundColor Red
}

Write-Host ""
Write-Host "System Breakdown:" -ForegroundColor Cyan
Write-Host "  File Detection: ~10ms" -ForegroundColor Gray
Write-Host "  Conversation Analysis: ~15ms" -ForegroundColor Gray
Write-Host "  Bayesian Learning Query: ~8ms" -ForegroundColor Gray
Write-Host "  Aggregation: ~5ms" -ForegroundColor Gray
Write-Host "  ----------------------------" -ForegroundColor Gray
Write-Host "  Total Expected: ~38ms" -ForegroundColor Gray
Write-Host "  Actual Average: $overallAvg ms" -ForegroundColor $(if ($overallAvg -lt 100) { "Green" } else { "Yellow" })
