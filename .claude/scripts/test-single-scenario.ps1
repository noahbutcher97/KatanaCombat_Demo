# Test single scenario
& "$PSScriptRoot/learning-tracker.ps1" -Action reset 2>&1 | Out-Null

$result = & "$PSScriptRoot/holistic-mode-detector.ps1" -FilePath "Source/Core/CombatComponent.cpp" 2>&1 | Where-Object { $_ -match '^\s*\{' } | Select-Object -Last 1 | ConvertFrom-Json

Write-Host "Mode: $($result.suggestedMode)"
Write-Host "Conf: $($result.confidence)"

if ($result.suggestedMode -match "combat") {
    Write-Host "Recording pattern..." -ForegroundColor Yellow
    $recordResult = & "$PSScriptRoot/learning-tracker.ps1" -Action record -Pattern "CombatComponent" -Mode "combat-logic" -FileConfidence $result.confidence -ConversationConfidence 0.0 2>&1
    Write-Host "Record result:"
    $recordResult

    Write-Host "`nCorrelating topics..." -ForegroundColor Yellow
    $correlateResult = & "$PSScriptRoot/learning-tracker.ps1" -Action correlate -Pattern "CombatComponent" -Mode "combat-logic" -Topics @("combo", "attack", "input") 2>&1
    Write-Host "Correlate result:"
    $correlateResult
}
