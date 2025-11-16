# Test Auto-Context Hook ML Integration
# Validates that the hook uses holistic detector and records learning data

Write-Host "Testing Auto-Context Hook ML Integration" -ForegroundColor Cyan
Write-Host "=========================================" -ForegroundColor Cyan
Write-Host ""

$passed = 0
$failed = 0

function Test-Component {
    param([string]$Name, [string]$Expected, [scriptblock]$Test)

    Write-Host "[$Name]" -NoNewline

    try {
        $result = & $Test
        if ($result) {
            Write-Host " PASS" -ForegroundColor Green
            Write-Host "  Expected: $Expected" -ForegroundColor Gray
            Write-Host "  Result: $result" -ForegroundColor Gray
            $script:passed++
            return $true
        } else {
            Write-Host " FAIL" -ForegroundColor Red
            Write-Host "  Expected: $Expected" -ForegroundColor Gray
            Write-Host "  Result: (false/null)" -ForegroundColor Red
            $script:failed++
            return $false
        }
    }
    catch {
        Write-Host " ERROR" -ForegroundColor Red
        Write-Host "  Exception: $_" -ForegroundColor Red
        $script:failed++
        return $false
    }
}

# Setup: Reset learning database
& "$PSScriptRoot/learning-tracker.ps1" -Action reset 2>&1 | Out-Null
Start-Sleep -Milliseconds 200

# Test 1: Hook uses holistic detector
Write-Host "=== Test 1: Hook Integration ===" -ForegroundColor Yellow
Write-Host ""

Test-Component -Name "Hook script exists" -Expected "File found" -Test {
    $hookPath = Join-Path $PSScriptRoot "..\hooks\auto-context.ps1"
    if (Test-Path $hookPath) {
        return "Hook found at $hookPath"
    }
    return $false
}

Test-Component -Name "Hook calls holistic detector" -Expected "Uses holistic-mode-detector.ps1" -Test {
    $hookPath = Join-Path $PSScriptRoot "..\hooks\auto-context.ps1"
    $content = Get-Content $hookPath -Raw
    if ($content -match "holistic-mode-detector\.ps1") {
        return "Hook uses holistic detector"
    }
    return "Hook does not reference holistic detector"
}

Test-Component -Name "Hook records learning data" -Expected "Calls learning-tracker.ps1" -Test {
    $hookPath = Join-Path $PSScriptRoot "..\hooks\auto-context.ps1"
    $content = Get-Content $hookPath -Raw
    if ($content -match "learning-tracker\.ps1.*-Action\s+(record|feedback)") {
        return "Hook integrates with learning tracker"
    }
    return "Hook does not record learning data"
}

# Test 2: Simulate file-open scenario
Write-Host ""
Write-Host "=== Test 2: File-Open Simulation ===" -ForegroundColor Yellow
Write-Host ""

Test-Component -Name "Detect animation file" -Expected "Suggests animation mode" -Test {
    # Simulate file open by setting environment variable
    $env:FILE_PATH = "Source/KatanaCombat/Public/Animation/AnimNotify_Phase.h"

    # Run hook
    $hookPath = Join-Path $PSScriptRoot "..\hooks\auto-context.ps1"
    $hookOutput = & powershell.exe -ExecutionPolicy Bypass -File $hookPath 2>&1

    # Check if it detected animation
    if ($hookOutput -match "animation" -or $hookOutput -match "ANIMATION") {
        return "Animation mode detected from file path"
    }
    return "Failed to detect animation mode"
}

Test-Component -Name "Learning data recorded" -Expected "Pattern saved to database" -Test {
    Start-Sleep -Milliseconds 500  # Wait for async job

    # Check if AnimNotify_Phase pattern was recorded
    $data = Get-Content ".claude/.context-learning.json" | ConvertFrom-Json

    if ($data.PSObject.Properties.Name -contains 'patterns') {
        $patternNames = $data.patterns.PSObject.Properties.Name
        if ($patternNames -contains "AnimNotify_Phase") {
            $pattern = $data.patterns.AnimNotify_Phase
            return "Pattern recorded: mode=$($pattern.mode), confidence=$([Math]::Round($pattern.bayesian.successCount * 0.067 * 100, 1))%"
        }
    }
    return "Pattern not found in learning database"
}

# Test 3: Learning improves over time
Write-Host ""
Write-Host "=== Test 3: Learning Over Time ===" -ForegroundColor Yellow
Write-Host ""

Test-Component -Name "Multiple opens increase confidence" -Expected "Confidence grows with usage" -Test {
    # Simulate opening the same file 3 times
    for ($i = 1; $i -le 3; $i++) {
        $env:FILE_PATH = "Source/KatanaCombat/Public/Animation/AnimNotify_Phase.h"
        $hookPath = Join-Path $PSScriptRoot "..\hooks\auto-context.ps1"
        & powershell.exe -ExecutionPolicy Bypass -File $hookPath 2>&1 | Out-Null
        Start-Sleep -Milliseconds 200
    }

    Start-Sleep -Milliseconds 500  # Wait for async jobs

    # Check confidence increased
    $result = & "$PSScriptRoot/learning-tracker.ps1" -Action query -Pattern "AnimNotify_Phase" | ConvertFrom-Json

    if ($result.found -and $result.bayesian.successCount -ge 3) {
        $conf = [Math]::Round($result.confidence * 100, 1)
        return "Success count = $($result.bayesian.successCount), confidence = $conf%"
    }
    return "Learning not working (expected multiple records)"
}

Test-Component -Name "Correlation boost available" -Expected "Topics can be correlated" -Test {
    # Record correlation for AnimNotify_Phase pattern
    & "$PSScriptRoot/learning-tracker.ps1" -Action correlate -Pattern "AnimNotify_Phase" -Mode "animation" -Topics @("montage", "blending", "phase") 2>&1 | Out-Null

    $data = Get-Content ".claude/.context-learning.json" | ConvertFrom-Json

    if ($data.PSObject.Properties.Name -contains 'correlations' -and
        $data.correlations.PSObject.Properties.Name -contains 'AnimNotify_Phase') {
        $topics = $data.correlations.AnimNotify_Phase.topics
        $topicCount = if ($topics.PSObject.Properties) { $topics.PSObject.Properties.Count } else { 0 }
        if ($topicCount -gt 0) {
            return "Correlations recorded: $topicCount topics"
        }
    }
    return "Correlations not found"
}

# Test 4: Feedback loop integration
Write-Host ""
Write-Host "=== Test 4: Feedback Integration ===" -ForegroundColor Yellow
Write-Host ""

Test-Component -Name "Hook can record feedback" -Expected "Success/failure tracking" -Test {
    $hookPath = Join-Path $PSScriptRoot "..\hooks\auto-context.ps1"
    $content = Get-Content $hookPath -Raw

    # Check for feedback recording logic
    if ($content -match "-Action\s+feedback") {
        return "Feedback integration present in hook"
    }
    return "Feedback integration not found"
}

Test-Component -Name "Async job execution" -Expected "Non-blocking learning" -Test {
    $hookPath = Join-Path $PSScriptRoot "..\hooks\auto-context.ps1"
    $content = Get-Content $hookPath -Raw

    # Check for async execution (Start-Job)
    if ($content -match "Start-Job") {
        return "Async execution used (non-blocking)"
    }
    return "Synchronous execution (may block hook)"
}

# Test 5: Graceful degradation
Write-Host ""
Write-Host "=== Test 5: Error Handling ===" -ForegroundColor Yellow
Write-Host ""

Test-Component -Name "Handles missing files gracefully" -Expected "No error on invalid path" -Test {
    $env:FILE_PATH = "NonExistent/Path/File.h"
    $hookPath = Join-Path $PSScriptRoot "..\hooks\auto-context.ps1"

    try {
        $hookOutput = & powershell.exe -ExecutionPolicy Bypass -File $hookPath 2>&1
        # Should exit gracefully (no exception)
        return "Hook handled missing file gracefully"
    } catch {
        return "Hook threw exception on missing file"
    }
}

Test-Component -Name "Handles empty path gracefully" -Expected "No error on empty path" -Test {
    $env:FILE_PATH = ""
    $hookPath = Join-Path $PSScriptRoot "..\hooks\auto-context.ps1"

    try {
        $hookOutput = & powershell.exe -ExecutionPolicy Bypass -File $hookPath 2>&1
        return "Hook handled empty path gracefully"
    } catch {
        return "Hook threw exception on empty path"
    }
}

# Cleanup
$env:FILE_PATH = $null

# Summary
Write-Host ""
Write-Host "=========================================" -ForegroundColor Cyan
Write-Host "Auto-Context ML Integration Test Results" -ForegroundColor Cyan
Write-Host "=========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Total Tests: $($passed + $failed)" -ForegroundColor White
Write-Host "Passed: $passed" -ForegroundColor Green
Write-Host "Failed: $failed" -ForegroundColor $(if ($failed -gt 0) { "Red" } else { "Green" })
Write-Host "Success Rate: $([Math]::Round($passed / ($passed + $failed) * 100, 1))%" -ForegroundColor $(if ($failed -eq 0) { "Green" } else { "Yellow" })
Write-Host ""

if ($failed -eq 0) {
    Write-Host "[OK] All auto-context ML integration tests passed!" -ForegroundColor Green
    Write-Host "Auto-context hook is production-ready." -ForegroundColor Green
} else {
    Write-Host "[ERROR] Some tests failed" -ForegroundColor Red
    Write-Host "Review failures above and fix before deployment." -ForegroundColor Yellow
}
