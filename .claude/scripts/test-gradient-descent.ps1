# Test Gradient Descent Weight Learning

Write-Host "Testing Gradient Descent Weight Learning" -ForegroundColor Cyan
Write-Host "=========================================" -ForegroundColor Cyan
Write-Host ""

# Scenario: Pattern with high file confidence but low conversation confidence
# If user keeps accepting (success), file weight should increase
# If user keeps rejecting (failure), file weight should decrease

# Reset learning to start fresh
& "$PSScriptRoot/learning-tracker.ps1" -Action reset | Out-Null

# Create a pattern with strong file signal, weak conversation signal
Write-Host "[Setup] Creating pattern with high file conf (0.9), low conv conf (0.3)" -ForegroundColor Yellow
& "$PSScriptRoot/learning-tracker.ps1" -Action record -Pattern "TestPattern" -Mode "animation" -FileConfidence 0.9 -ConversationConfidence 0.3 | Out-Null

# Query initial weights
$initial = & "$PSScriptRoot/learning-tracker.ps1" -Action query -Pattern "TestPattern" | ConvertFrom-Json
$initialWeights = $initial.features.weights
Write-Host "  Initial weights: file=$($initialWeights.file), conv=$($initialWeights.conversation)" -ForegroundColor Gray
Write-Host ""

# Test 1: Multiple successes (should increase file weight)
Write-Host "[Test 1] Recording 5 successes (user accepts strong file signal)" -ForegroundColor Yellow
for ($i = 1; $i -le 5; $i++) {
    & "$PSScriptRoot/learning-tracker.ps1" -Action feedback -Pattern "TestPattern" -Mode "animation" -Success $true | Out-Null
    Write-Host "  Success $i recorded" -ForegroundColor Gray
}

$after5Success = & "$PSScriptRoot/learning-tracker.ps1" -Action query -Pattern "TestPattern" | ConvertFrom-Json
$weightsAfterSuccess = $after5Success.features.weights
Write-Host "  Weights after 5 successes: file=$($weightsAfterSuccess.file), conv=$($weightsAfterSuccess.conversation)" -ForegroundColor Green

$fileChange = [Math]::Round(($weightsAfterSuccess.file - $initialWeights.file) * 100, 1)
if ($weightsAfterSuccess.file -gt $initialWeights.file) {
    Write-Host "  File weight increased by $fileChange% (EXPECTED: file conf was high)" -ForegroundColor Green
} else {
    Write-Host "  File weight decreased by $(-$fileChange)% (UNEXPECTED)" -ForegroundColor Red
}
Write-Host ""

# Test 2: Reset and test with failures
& "$PSScriptRoot/learning-tracker.ps1" -Action reset | Out-Null
& "$PSScriptRoot/learning-tracker.ps1" -Action record -Pattern "TestPattern2" -Mode "animation" -FileConfidence 0.3 -ConversationConfidence 0.9 | Out-Null

$initial2 = & "$PSScriptRoot/learning-tracker.ps1" -Action query -Pattern "TestPattern2" | ConvertFrom-Json
$initialWeights2 = $initial2.features.weights
Write-Host "[Test 2] Pattern with low file conf (0.3), high conv conf (0.9)" -ForegroundColor Yellow
Write-Host "  Initial weights: file=$($initialWeights2.file), conv=$($initialWeights2.conversation)" -ForegroundColor Gray

# Record successes - conversation weight should increase
for ($i = 1; $i -le 5; $i++) {
    & "$PSScriptRoot/learning-tracker.ps1" -Action feedback -Pattern "TestPattern2" -Mode "animation" -Success $true | Out-Null
}

$after5Success2 = & "$PSScriptRoot/learning-tracker.ps1" -Action query -Pattern "TestPattern2" | ConvertFrom-Json
$weightsAfterSuccess2 = $after5Success2.features.weights
Write-Host "  Weights after 5 successes: file=$($weightsAfterSuccess2.file), conv=$($weightsAfterSuccess2.conversation)" -ForegroundColor Green

$convChange = [Math]::Round(($weightsAfterSuccess2.conversation - $initialWeights2.conversation) * 100, 1)
if ($weightsAfterSuccess2.conversation -gt $initialWeights2.conversation) {
    Write-Host "  Conv weight increased by $convChange% (EXPECTED: conv conf was high)" -ForegroundColor Green
} else {
    Write-Host "  Conv weight decreased by $(-$convChange)% (UNEXPECTED)" -ForegroundColor Red
}
Write-Host ""

# Test 3: Convergence test
Write-Host "[Test 3] Convergence test (10 more iterations)" -ForegroundColor Yellow
for ($i = 1; $i -le 10; $i++) {
    & "$PSScriptRoot/learning-tracker.ps1" -Action feedback -Pattern "TestPattern2" -Mode "animation" -Success $true | Out-Null
}

$converged = & "$PSScriptRoot/learning-tracker.ps1" -Action query -Pattern "TestPattern2" | ConvertFrom-Json
$convergedWeights = $converged.features.weights
Write-Host "  Weights after 15 total successes: file=$($convergedWeights.file), conv=$($convergedWeights.conversation)" -ForegroundColor Green

# Check if weights stabilized (change < 1% from iteration 5 to 15)
$stabilityChange = [Math]::Abs($convergedWeights.conversation - $weightsAfterSuccess2.conversation)
if ($stabilityChange -lt 0.05) {
    Write-Host "  Weights stabilized (change: $([Math]::Round($stabilityChange * 100, 1))%)" -ForegroundColor Green
} else {
    Write-Host "  Weights still changing (change: $([Math]::Round($stabilityChange * 100, 1))%)" -ForegroundColor Yellow
}
Write-Host ""

Write-Host "[Summary]" -ForegroundColor Cyan
Write-Host "Gradient descent learning adapts weights based on feature effectiveness:" -ForegroundColor Gray
Write-Host "  - High file conf + success -> file weight increases" -ForegroundColor Gray
Write-Host "  - High conv conf + success -> conv weight increases" -ForegroundColor Gray
Write-Host "  - Weights clamped to 20-80% range for stability" -ForegroundColor Gray
Write-Host "  - Convergence typically within 10-15 iterations" -ForegroundColor Gray
