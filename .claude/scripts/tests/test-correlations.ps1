# Test Correlation Matrix

Write-Host "Testing Correlation Matrix System" -ForegroundColor Cyan
Write-Host "==================================" -ForegroundColor Cyan
Write-Host ""

# Test 1: Record correlations for AnimNotify pattern
Write-Host "[Test 1] Recording AnimNotify pattern with animation topics" -ForegroundColor Yellow
& "$PSScriptRoot/learning-tracker.ps1" -Action correlate -Pattern "AnimNotify" -Mode "animation" -Topics @("montage", "blending", "phase", "timing") | Out-Null
Write-Host "  Recorded: AnimNotify + [montage, blending, phase, timing] -> animation" -ForegroundColor Gray

# Record again with overlapping topics (should boost existing)
& "$PSScriptRoot/learning-tracker.ps1" -Action correlate -Pattern "AnimNotify" -Mode "animation" -Topics @("montage", "notify", "animation") | Out-Null
Write-Host "  Recorded: AnimNotify + [montage, notify, animation] -> animation" -ForegroundColor Gray

# Record with different topics
& "$PSScriptRoot/learning-tracker.ps1" -Action correlate -Pattern "AnimNotify" -Mode "animation" -Topics @("montage", "phase") | Out-Null
Write-Host "  Recorded: AnimNotify + [montage, phase] -> animation (montage should be high now)" -ForegroundColor Gray
Write-Host ""

# Test 2: Record correlations for Combat pattern
Write-Host "[Test 2] Recording CombatComponent pattern with combat topics" -ForegroundColor Yellow
& "$PSScriptRoot/learning-tracker.ps1" -Action correlate -Pattern "CombatComponent" -Mode "combat-logic" -Topics @("attack", "combo", "input", "state") | Out-Null
Write-Host "  Recorded: CombatComponent + [attack, combo, input, state] -> combat-logic" -ForegroundColor Gray

& "$PSScriptRoot/learning-tracker.ps1" -Action correlate -Pattern "CombatComponent" -Mode "combat-logic" -Topics @("attack", "damage", "hit") | Out-Null
Write-Host "  Recorded: CombatComponent + [attack, damage, hit] -> combat-logic" -ForegroundColor Gray
Write-Host ""

# Test 3: Query correlations (without -Topics triggers query mode)
Write-Host "[Test 3] Querying learned correlations" -ForegroundColor Yellow

# Query AnimNotify correlations
Write-Host "  Querying AnimNotify pattern..." -ForegroundColor Gray
$animQuery = & "$PSScriptRoot/learning-tracker.ps1" -Action correlate -Pattern "AnimNotify" | ConvertFrom-Json
Write-Host "  AnimNotify correlations:" -ForegroundColor White
Write-Host "    Found: $($animQuery.found)" -ForegroundColor Green
Write-Host "    Top Topics: $($animQuery.topTopics)" -ForegroundColor Gray

# Now test boost with specific topics
$topics = @("montage", "blending")
$boost = 0.0
$matched = 0

# Manually calculate boost from learning data
$learningData = Get-Content ".claude/.context-learning.json" | ConvertFrom-Json
if ($learningData.correlations.PSObject.Properties.Name -contains "AnimNotify") {
    $pattern = $learningData.correlations.AnimNotify
    foreach ($topic in $topics) {
        if ($pattern.topics.PSObject.Properties.Name -contains $topic) {
            $boost += [double]$pattern.topics.$topic
            $matched++
        }
    }
    if ($matched -gt 0) { $boost = $boost / $matched }
}

Write-Host "  AnimNotify + [montage, blending]:" -ForegroundColor White
Write-Host "    Boost: $([Math]::Round($boost * 100, 1))%" -ForegroundColor Green
Write-Host "    Matched: $matched/$($topics.Count)" -ForegroundColor Gray

# Query CombatComponent
Write-Host "  Querying CombatComponent pattern..." -ForegroundColor Gray
$combatQuery = & "$PSScriptRoot/learning-tracker.ps1" -Action correlate -Pattern "CombatComponent" | ConvertFrom-Json
Write-Host "  CombatComponent correlations:" -ForegroundColor White
Write-Host "    Found: $($combatQuery.found)" -ForegroundColor Green
Write-Host "    Top Topics: $($combatQuery.topTopics)" -ForegroundColor Gray
Write-Host ""

# Test 4: Decay test (unrelated topics)
Write-Host "[Test 4] Testing topic decay (unrelated topics)" -ForegroundColor Yellow
& "$PSScriptRoot/learning-tracker.ps1" -Action correlate -Pattern "AnimNotify" -Mode "animation" -Topics @("testing", "debug") | Out-Null
Write-Host "  Recorded AnimNotify with unrelated topics [testing, debug]" -ForegroundColor Gray

$decayResult = & "$PSScriptRoot/learning-tracker.ps1" -Action correlate -Pattern "AnimNotify" -Topics @("montage") | ConvertFrom-Json
Write-Host "  After decay, montage correlation:" -ForegroundColor White
Write-Host "    Top Correlations: $($decayResult.topTopics)" -ForegroundColor Gray
Write-Host "    Note: Old topics (blending, phase) should have decayed ~5% per switch" -ForegroundColor Gray
Write-Host ""

# Final status
Write-Host "[Final Status]" -ForegroundColor Cyan
& "$PSScriptRoot/learning-tracker.ps1" -Action status
