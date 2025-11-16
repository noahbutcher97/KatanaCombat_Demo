# Test system robustness with edge cases

Write-Host "Testing System Robustness" -ForegroundColor Cyan
Write-Host "=========================" -ForegroundColor Cyan
Write-Host ""

$tests = @(
    @{ name = "Empty inputs"; file = ""; conv = "" }
    @{ name = "Only whitespace"; file = "   "; conv = "   " }
    @{ name = "Non-existent file"; file = "DoesNotExist.xyz"; conv = "test" }
    @{ name = "Nonsense conversation"; file = "real/path.h"; conv = "asdfqwerzxcv" }
    @{ name = "Minimal conversation"; file = "AnimNotify.h"; conv = "..." }
    @{ name = "Valid inputs"; file = "Source/KatanaCombat/Animation/AnimNotify_Phase.h"; conv = "Working on animation phase transitions" }
    @{ name = "File only (no conv)"; file = "CombatComponent.cpp"; conv = "" }
    @{ name = "Conv only (no file)"; file = ""; conv = "Need to configure AttackData properties" }
)

$passed = 0
$failed = 0

foreach ($test in $tests) {
    Write-Host "Test: $($test.name)" -ForegroundColor Yellow -NoNewline

    try {
        $result = & "$PSScriptRoot/holistic-mode-detector.ps1" -FilePath $test.file -ConversationText $test.conv 2>$null | ConvertFrom-Json

        if ($result -and $result.PSObject.Properties['suggestedMode']) {
            Write-Host " [PASS]" -ForegroundColor Green
            $passed++
        } else {
            Write-Host " [FAIL] No valid result" -ForegroundColor Red
            $failed++
        }
    } catch {
        Write-Host " [FAIL] Exception: $_" -ForegroundColor Red
        $failed++
    }
}

Write-Host ""
Write-Host "Results: $passed passed, $failed failed" -ForegroundColor $(if ($failed -eq 0) { "Green" } else { "Yellow" })

if ($failed -eq 0) {
    Write-Host ""
    Write-Host "[OK] All robustness tests passed!" -ForegroundColor Green
} else {
    Write-Host ""
    Write-Host "[WARN] Some tests failed - review error handling" -ForegroundColor Yellow
}
