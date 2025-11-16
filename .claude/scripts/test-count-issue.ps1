# Test count issue
& "$PSScriptRoot/test-e2e-real-world.ps1" 2>&1 | Out-Null

$data = Get-Content ".claude/.context-learning.json" | ConvertFrom-Json
$patternCount = 0
if ($data.PSObject.Properties.Name -contains 'patterns' -and $data.patterns.PSObject.Properties) {
    $patternCount = $data.patterns.PSObject.Properties.Count
}

Write-Host "Pattern count type: $($patternCount.GetType().FullName)"
Write-Host "Pattern count value: $patternCount"
Write-Host "Pattern count is array: $($patternCount -is [array])"

# Test the return statement
$result = "Only $patternCount patterns learned"
Write-Host "Result: $result"
Write-Host "Result type: $($result.GetType().FullName)"
