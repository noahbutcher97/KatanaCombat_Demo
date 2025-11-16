# Debug E2E failing scenario

& "$PSScriptRoot/learning-tracker.ps1" -Action reset 2>&1 | Out-Null
Write-Host "Reset complete" -ForegroundColor Gray

& "$PSScriptRoot/learning-tracker.ps1" -Action record -Pattern "AttackData" -Mode "combat-logic" -FileConfidence 0.7 -ConversationConfidence 0.0 2>&1 | Out-Null
Write-Host "Recorded pattern" -ForegroundColor Gray

Write-Host "Calling feedback with Success=false..." -ForegroundColor Yellow
$result = & "$PSScriptRoot/learning-tracker.ps1" -Action feedback -Pattern "AttackData" -Mode "combat-logic" -Success $false -ActualMode "data-assets" 2>&1
Write-Host "Feedback result:" -ForegroundColor Cyan
$result

Write-Host "`nQuerying pattern..." -ForegroundColor Yellow
$query = & "$PSScriptRoot/learning-tracker.ps1" -Action query -Pattern "AttackData" | ConvertFrom-Json
Write-Host "Query result:" -ForegroundColor Cyan
$query | ConvertTo-Json -Depth 5
