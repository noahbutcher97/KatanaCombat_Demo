# Debug correlation recording

Write-Host "Testing correlation recording..." -ForegroundColor Cyan

# Reset
& "$PSScriptRoot/learning-tracker.ps1" -Action reset | Out-Null

# Record correlation
$topics = @("montage", "blending", "phase")
Write-Host "Topics array: $($topics -join ', ')" -ForegroundColor Yellow
Write-Host "Topics count: $($topics.Count)" -ForegroundColor Yellow

$result = & "$PSScriptRoot/learning-tracker.ps1" -Action correlate -Pattern "AnimNotify" -Mode "animation" -Topics $topics
Write-Host "Correlate result: $result" -ForegroundColor Green

# Check what was saved
$data = Get-Content ".claude/.context-learning.json" | ConvertFrom-Json
Write-Host "" -ForegroundColor Yellow
Write-Host "Saved data:" -ForegroundColor Cyan
$data | ConvertTo-Json -Depth 10
