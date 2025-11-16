# Test Bayesian Learning System

Write-Host "Testing Bayesian Learning System" -ForegroundColor Cyan
Write-Host "================================" -ForegroundColor Cyan
Write-Host ""

# Test 1: Record 5 successful uses
Write-Host "[Test 1] Recording 5 successful uses of AnimNotify_Phase -> animation" -ForegroundColor Yellow
for ($i = 1; $i -le 5; $i++) {
    & "$PSScriptRoot/learning-tracker.ps1" -Action feedback -Pattern "AnimNotify_Phase" -Mode "animation" -Success $true | Out-Null
    Write-Host "  Success $i recorded" -ForegroundColor Gray
}

# Query confidence
$result1 = & "$PSScriptRoot/learning-tracker.ps1" -Action query -Pattern "AnimNotify_Phase" | ConvertFrom-Json
Write-Host "  Confidence after 5 successes: $([Math]::Round($result1.confidence * 100, 1))%" -ForegroundColor Green
Write-Host "  Bayesian mean: $([Math]::Round($result1.bayesian.mean * 100, 1))%, Uncertainty: $([Math]::Round($result1.bayesian.uncertainty * 100, 0))%" -ForegroundColor Gray
Write-Host ""

# Test 2: Record 2 failures
Write-Host "[Test 2] Recording 2 failures (user overrode suggestion)" -ForegroundColor Yellow
& "$PSScriptRoot/learning-tracker.ps1" -Action feedback -Pattern "AnimNotify_Phase" -Mode "animation" -Success $false | Out-Null
& "$PSScriptRoot/learning-tracker.ps1" -Action feedback -Pattern "AnimNotify_Phase" -Mode "animation" -Success $false | Out-Null
Write-Host "  2 failures recorded" -ForegroundColor Gray

# Query confidence
$result2 = & "$PSScriptRoot/learning-tracker.ps1" -Action query -Pattern "AnimNotify_Phase" | ConvertFrom-Json
Write-Host "  Confidence after 2 failures: $([Math]::Round($result2.confidence * 100, 1))%" -ForegroundColor Yellow
Write-Host "  Success/Failure: $($result2.bayesian.successCount)/$($result2.bayesian.failureCount)" -ForegroundColor Gray
Write-Host ""

# Test 3: Temporal decay simulation
Write-Host "[Test 3] Simulating 7 days of decay" -ForegroundColor Yellow
Write-Host "  Note: Actual decay happens automatically based on lastUsed timestamp" -ForegroundColor Gray
Write-Host "  After 7 days at 5% decay rate: $([Math]::Round([Math]::Pow(0.95, 7) * 100, 1))% of original" -ForegroundColor Gray
Write-Host "  After 30 days: $([Math]::Round([Math]::Pow(0.95, 30) * 100, 1))% of original" -ForegroundColor Gray
Write-Host ""

# Test 4: New pattern with correction
Write-Host "[Test 4] Recording user correction (CombatComponent -> combat-logic)" -ForegroundColor Yellow
& "$PSScriptRoot/learning-tracker.ps1" -Action feedback -Pattern "CombatComponent" -Mode "animation" -Success $false -ActualMode "combat-logic" | Out-Null
Write-Host "  Correction recorded" -ForegroundColor Gray

$result3 = & "$PSScriptRoot/learning-tracker.ps1" -Action query -Pattern "CombatComponent" | ConvertFrom-Json
Write-Host "  Learned mode: $($result3.mode)" -ForegroundColor Green
Write-Host "  Confidence: $([Math]::Round($result3.confidence * 100, 1))%" -ForegroundColor Green
Write-Host ""

# Final status
Write-Host "[Final Status]" -ForegroundColor Cyan
& "$PSScriptRoot/learning-tracker.ps1" -Action status
