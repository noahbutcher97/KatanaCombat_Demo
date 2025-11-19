# Test deserialization issue
& "$PSScriptRoot/learning-tracker.ps1" -Action reset 2>&1 | Out-Null

# First record
Write-Host "Recording AnimNotify_Phase..." -ForegroundColor Yellow
& "$PSScriptRoot/learning-tracker.ps1" -Action record -Pattern "AnimNotify_Phase" -Mode "animation" -FileConfidence 0.95 -ConversationConfidence 0.0 2>&1 | Out-Null

# Load and inspect
$data = Get-Content ".claude/.context-learning.json" | ConvertFrom-Json
Write-Host "`nAfter first record:" -ForegroundColor Cyan
Write-Host "  Patterns type: $($data.patterns.GetType().FullName)"
Write-Host "  AnimNotify_Phase type: $($data.patterns.AnimNotify_Phase.GetType().FullName)"
Write-Host "  Bayesian type: $($data.patterns.AnimNotify_Phase.bayesian.GetType().FullName)"
Write-Host "  Has successCount property: $($data.patterns.AnimNotify_Phase.bayesian.PSObject.Properties.Name -contains 'successCount')"
Write-Host "  successCount value: $($data.patterns.AnimNotify_Phase.bayesian.successCount)"

# Second record (this is where it fails in E2E test)
Write-Host "`nRecording CombatComponent..." -ForegroundColor Yellow
try {
    & "$PSScriptRoot/learning-tracker.ps1" -Action record -Pattern "CombatComponent" -Mode "combat-logic" -FileConfidence 0.6 -ConversationConfidence 0.0 2>&1
} catch {
    Write-Host "ERROR: $_" -ForegroundColor Red
}

# Load and inspect again
$data2 = Get-Content ".claude/.context-learning.json" | ConvertFrom-Json
Write-Host "`nAfter second record:" -ForegroundColor Cyan
Write-Host "  Pattern count: $($data2.patterns.PSObject.Properties.Count)"
