# Debug global accuracy tracking

Write-Host "Testing global accuracy..." -ForegroundColor Cyan

# Reset and create pattern
& "$PSScriptRoot/learning-tracker.ps1" -Action reset 2>&1 | Out-Null
& "$PSScriptRoot/learning-tracker.ps1" -Action record -Pattern "TestPattern" -Mode "test" -FileConfidence 0.9 -ConversationConfidence 0.8 2>&1 | Out-Null

# Record 2 successes
& "$PSScriptRoot/learning-tracker.ps1" -Action feedback -Pattern "TestPattern" -Mode "test" -Success $true 2>&1 | Out-Null
& "$PSScriptRoot/learning-tracker.ps1" -Action feedback -Pattern "TestPattern" -Mode "test" -Success $true 2>&1 | Out-Null

# Record 1 failure
& "$PSScriptRoot/learning-tracker.ps1" -Action feedback -Pattern "TestPattern" -Mode "test" -Success $false 2>&1 | Out-Null

# Check data
$data = Get-Content ".claude/.context-learning.json" | ConvertFrom-Json

Write-Host "GlobalStats exists: $($data.PSObject.Properties.Name -contains 'globalStats')" -ForegroundColor Yellow
Write-Host "AutoSwitchAccuracy property exists: $($data.globalStats.PSObject.Properties.Name -contains 'autoSwitchAccuracy')" -ForegroundColor Yellow
Write-Host "TotalSuccess: $($data.globalStats.totalSuccess)" -ForegroundColor Yellow
Write-Host "TotalFailure: $($data.globalStats.totalFailure)" -ForegroundColor Yellow
Write-Host "AutoSwitchAccuracy value: '$($data.globalStats.autoSwitchAccuracy)'" -ForegroundColor Yellow
Write-Host "AutoSwitchAccuracy type: $($data.globalStats.autoSwitchAccuracy.GetType().FullName)" -ForegroundColor Yellow

$accuracy = [double]$data.globalStats.autoSwitchAccuracy
Write-Host "Accuracy as double: $accuracy" -ForegroundColor Yellow
Write-Host "Expected range: 0.65 - 0.68" -ForegroundColor Yellow
Write-Host "In range: $($accuracy -ge 0.65 -and $accuracy -le 0.68)" -ForegroundColor Yellow
