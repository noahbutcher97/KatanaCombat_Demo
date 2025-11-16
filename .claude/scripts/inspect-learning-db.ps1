# Inspect learning database
$data = Get-Content ".claude/.context-learning.json" | ConvertFrom-Json

Write-Host "=== Learning Database Inspection ===" -ForegroundColor Cyan
Write-Host ""

Write-Host "Patterns:" -ForegroundColor Yellow
$data.patterns.PSObject.Properties | ForEach-Object {
    Write-Host "  - $($_.Name): mode=$($_.Value.mode), success=$($_.Value.bayesian.successCount), failure=$($_.Value.bayesian.failureCount)"
}
$patternCount = @($data.patterns.PSObject.Properties).Count
Write-Host "  Total: $patternCount patterns" -ForegroundColor Green

Write-Host ""
Write-Host "Correlations:" -ForegroundColor Yellow
$data.correlations.PSObject.Properties | ForEach-Object {
    Write-Host "  - $($_.Name)"
}
$corrCount = @($data.correlations.PSObject.Properties).Count
Write-Host "  Total: $corrCount correlations" -ForegroundColor Green

Write-Host ""
Write-Host "Global Stats:" -ForegroundColor Yellow
Write-Host "  Total Switches: $($data.globalStats.totalSwitches)"
Write-Host "  Overall Accuracy: $([Math]::Round($data.globalStats.autoSwitchAccuracy * 100, 1))%" -ForegroundColor $(if ($data.globalStats.autoSwitchAccuracy -gt 0.8) { "Green" } elseif ($data.globalStats.autoSwitchAccuracy -gt 0.6) { "Yellow" } else { "Red" })

# Extract values with defaults for new fields
$implicitSuccess = if ($data.globalStats.PSObject.Properties.Name -contains 'implicitSuccess') { $data.globalStats.implicitSuccess } else { 0 }
$explicitSuccess = if ($data.globalStats.PSObject.Properties.Name -contains 'explicitSuccess') { $data.globalStats.explicitSuccess } else { 0 }
$explicitFailure = if ($data.globalStats.PSObject.Properties.Name -contains 'explicitFailure') { $data.globalStats.explicitFailure } else { 0 }

Write-Host ""
Write-Host "  Breakdown:" -ForegroundColor Cyan
Write-Host "    Success: $($data.globalStats.totalSuccess) patterns accepted (not rejected)" -ForegroundColor Green
Write-Host "    Failure: $($data.globalStats.totalFailure) patterns rejected" -ForegroundColor $(if ($data.globalStats.totalFailure -gt 0) { "Red" } else { "Green" })

Write-Host ""
Write-Host "  Feedback Detail:" -ForegroundColor Cyan
$totalFeedback = $explicitSuccess + $explicitFailure
if ($totalFeedback -gt 0) {
    $explicitAccuracy = [Math]::Round($explicitSuccess / $totalFeedback * 100, 1)
    Write-Host "    Total Explicit Feedback: $totalFeedback events" -ForegroundColor Gray
    Write-Host "      Confirmed Correct: $explicitSuccess" -ForegroundColor Green
    Write-Host "      Rejected: $explicitFailure" -ForegroundColor Red
    Write-Host "    Feedback Accuracy: $explicitAccuracy%" -ForegroundColor Cyan
} else {
    Write-Host "    No explicit feedback given yet" -ForegroundColor Gray
}

$implicitCount = $data.globalStats.totalSwitches - $totalFeedback
if ($implicitCount -gt 0) {
    Write-Host ""
    Write-Host "  Implicit Patterns (no feedback): $implicitCount switches" -ForegroundColor Gray
}
