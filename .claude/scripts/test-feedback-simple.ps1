# Simple feedback test

& "$PSScriptRoot/learning-tracker.ps1" -Action reset | Out-Null
Write-Host "Reset database" -ForegroundColor Gray

& "$PSScriptRoot/learning-tracker.ps1" -Action record -Pattern "TestPattern" -Mode "test" -FileConfidence 0.9 -ConversationConfidence 0.8 | Out-Null
Write-Host "Recorded pattern" -ForegroundColor Gray

Write-Host "Calling feedback..." -ForegroundColor Yellow
$feedbackResult = & "$PSScriptRoot/learning-tracker.ps1" -Action feedback -Pattern "TestPattern" -Mode "test" -Success $true 2>&1
Write-Host "Feedback result: $feedbackResult" -ForegroundColor Cyan

Write-Host "Querying pattern..." -ForegroundColor Yellow
$queryResult = & "$PSScriptRoot/learning-tracker.ps1" -Action query -Pattern "TestPattern"
Write-Host "Query result:" -ForegroundColor Cyan
$queryResult
