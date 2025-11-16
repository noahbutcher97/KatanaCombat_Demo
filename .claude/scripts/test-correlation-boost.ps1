# Test Correlation Boost Integration

Write-Host "Testing Correlation Boost in Holistic Detector" -ForegroundColor Cyan
Write-Host "===============================================" -ForegroundColor Cyan
Write-Host ""

# First record some correlations for AnimNotify
Write-Host "[Setup] Recording correlations for AnimNotify pattern..." -ForegroundColor Yellow
& "$PSScriptRoot/learning-tracker.ps1" -Action correlate -Pattern "AnimNotify" -Mode "animation" -Topics @("montage", "blending", "phase") | Out-Null
& "$PSScriptRoot/learning-tracker.ps1" -Action correlate -Pattern "AnimNotify" -Mode "animation" -Topics @("montage", "animation") | Out-Null
Write-Host "  Correlations recorded" -ForegroundColor Gray
Write-Host ""

# Test 1: Without conversation text (no boost)
Write-Host "[Test 1] Detection without conversation text (no boost)" -ForegroundColor Yellow
& "$PSScriptRoot/holistic-mode-detector.ps1" -FilePath "Source/KatanaCombat/Public/Animation/AnimNotify.h" -ShowDetails
Write-Host ""

# Test 2: With conversation text containing correlated topics (should boost)
Write-Host "[Test 2] Detection WITH conversation containing correlated topics" -ForegroundColor Yellow
& "$PSScriptRoot/holistic-mode-detector.ps1" `
    -FilePath "Source/KatanaCombat/Public/Animation/AnimNotify.h" `
    -ConversationText "I'm implementing montage blending with phase transitions for the animation system" `
    -ShowDetails
Write-Host ""

# Test 3: With conversation text but non-correlated topics (minimal boost)
Write-Host "[Test 3] Detection with non-correlated conversation topics" -ForegroundColor Yellow
& "$PSScriptRoot/holistic-mode-detector.ps1" `
    -FilePath "Source/KatanaCombat/Public/Animation/AnimNotify.h" `
    -ConversationText "Working on testing and debugging combat mechanics" `
    -ShowDetails
