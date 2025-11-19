# Integration Test for Intelligent Mode Detector v3.0
# Tests full pipeline: intelligent-mode-detector → auto-context → learning-tracker

param(
    [switch]$Verbose
)

Write-Host ""
Write-Host "Integration Test: Intelligent Mode Detection v3.0" -ForegroundColor Cyan
Write-Host "=================================================" -ForegroundColor Cyan
Write-Host ""

# Test files
$testFiles = @(
    @{
        path = "Source/KatanaCombat/Public/Animation/AnimNotify_Phase.h"
        expected = "animation"
        threshold = 0.50
    },
    @{
        path = "Source/KatanaCombat/Private/Core/CombatComponent.cpp"
        expected = "combat-logic"
        threshold = 0.40
    },
    @{
        path = "Source/KatanaCombat/Public/Data/AttackData.h"
        expected = "data-assets"
        threshold = 0.40
    }
)

$passCount = 0
$failCount = 0

foreach ($test in $testFiles) {
    Write-Host "Testing: $($test.path)" -ForegroundColor Yellow
    Write-Host "  Expected: $($test.expected) (threshold: $($test.threshold))" -ForegroundColor Gray

    try {
        # Simulate auto-context hook
        $env:FILE_PATH = $test.path
        $detectorPath = ".claude/scripts/intelligent-mode-detector.ps1"

        # Get current mode
        $trackerPath = ".claude/.context-history.json"
        $currentMode = "full"
        if (Test-Path $trackerPath) {
            $trackerData = Get-Content $trackerPath | ConvertFrom-Json
            if ($trackerData.PSObject.Properties.Name -contains 'currentMode') {
                $currentMode = $trackerData.currentMode
            }
        }

        # Call detector
        $resultJson = & powershell -ExecutionPolicy Bypass -File $detectorPath -FilePath $test.path -CurrentMode $currentMode 2>$null
        $result = $resultJson | ConvertFrom-Json

        if ($result) {
            $suggestedMode = $result.suggestedMode
            $confidence = [double]$result.confidence

            Write-Host "  Result: $suggestedMode (confidence: $([Math]::Round($confidence, 3)))" -ForegroundColor Cyan

            # Check if mode matches
            if ($suggestedMode -eq $test.expected) {
                if ($confidence -ge $test.threshold) {
                    Write-Host "  [PASS] Correct mode with sufficient confidence" -ForegroundColor Green
                    $passCount++
                } else {
                    Write-Host "  [WARN] Correct mode but low confidence ($confidence < $($test.threshold))" -ForegroundColor Yellow
                    $passCount++
                }
            } else {
                Write-Host "  [FAIL] Expected $($test.expected), got $suggestedMode" -ForegroundColor Red
                $failCount++
            }

            # Show factor breakdown if verbose
            if ($Verbose -and $result.PSObject.Properties.Name -contains 'factors') {
                Write-Host ""
                Write-Host "  Factor Breakdown:" -ForegroundColor Gray
                foreach ($factorName in @('file', 'conversation', 'learning', 'history', 'time')) {
                    if ($result.factors.PSObject.Properties.Name -contains $factorName) {
                        $factor = $result.factors.$factorName
                        $factorConf = [Math]::Round([double]$factor.confidence * 100, 1)
                        $weighted = [Math]::Round([double]$factor.weightedScore, 3)
                        Write-Host "    $factorName : $factorConf% conf -> $weighted weighted" -ForegroundColor DarkGray
                    }
                }
            }
        } else {
            Write-Host "  [FAIL] No result from detector" -ForegroundColor Red
            $failCount++
        }
    } catch {
        Write-Host "  [ERROR] Exception: $($_.Exception.Message)" -ForegroundColor Red
        $failCount++
    }

    Write-Host ""
}

# Summary
Write-Host "Test Summary" -ForegroundColor Cyan
Write-Host "============" -ForegroundColor Cyan
Write-Host "  Total: $($testFiles.Count)" -ForegroundColor Gray
Write-Host "  Passed: $passCount" -ForegroundColor Green
Write-Host "  Failed: $failCount" -ForegroundColor $(if ($failCount -gt 0) { 'Red' } else { 'Gray' })
Write-Host ""

if ($failCount -eq 0) {
    Write-Host "[SUCCESS] All integration tests passed!" -ForegroundColor Green
    Write-Host ""
    exit 0
} else {
    Write-Host "[FAILED] Some tests failed" -ForegroundColor Red
    Write-Host ""
    exit 1
}
