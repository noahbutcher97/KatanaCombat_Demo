# Debug reset test

Write-Host "Testing reset validation..." -ForegroundColor Cyan

& "$PSScriptRoot/learning-tracker.ps1" -Action reset 2>&1 | Out-Null
Start-Sleep -Milliseconds 200

$data = Get-Content ".claude/.context-learning.json" | ConvertFrom-Json

Write-Host "Version property exists: $($data.PSObject.Properties.Name -contains 'version')" -ForegroundColor Yellow
Write-Host "Version value: '$($data.version)'" -ForegroundColor Yellow
Write-Host "Patterns property exists: $($data.PSObject.Properties.Name -contains 'patterns')" -ForegroundColor Yellow
Write-Host "Patterns type: $($data.patterns.GetType().FullName)" -ForegroundColor Yellow
Write-Host "Patterns has PSObject: $($null -ne $data.patterns.PSObject)" -ForegroundColor Yellow
Write-Host "Patterns properties: $($data.patterns.PSObject.Properties)" -ForegroundColor Yellow
$patternCount = if ($data.patterns.PSObject.Properties) { $data.patterns.PSObject.Properties.Count } else { 0 }
Write-Host "Patterns count: $patternCount" -ForegroundColor Yellow

if ($data.PSObject.Properties.Name -contains 'version' -and $data.version -eq "2.1" -and $patternCount -eq 0) {
    Write-Host "PASS: All checks passed" -ForegroundColor Green
} else {
    Write-Host "FAIL: One or more checks failed" -ForegroundColor Red
}
