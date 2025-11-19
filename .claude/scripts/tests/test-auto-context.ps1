# Test auto-context hook with various file paths

param(
    [Parameter(Mandatory=$false)]
    [switch]$EnableAutoSwitch
)

$testFiles = @(
    "Source/KatanaCombat/Public/Animation/AnimNotify_AttackPhaseTransition.h"
    "Source/KatanaCombat/Private/Core/CombatComponent.cpp"
    "Source/KatanaCombat/Public/Data/AttackData.h"
    "Source/KatanaCombatTest/CombatComponentTest.cpp"
    "docs/SYSTEM_PROMPT.md"
    "Source/KatanaCombatEditor/AttackDataAssetEditor.cpp"
)

if ($EnableAutoSwitch) {
    $env:CLAUDE_AUTO_SWITCH_CONTEXT = "1"
    Write-Host "[INFO] Auto-switch ENABLED for testing" -ForegroundColor Green
    Write-Host ""
} else {
    $env:CLAUDE_AUTO_SWITCH_CONTEXT = "0"
    Write-Host "[INFO] Auto-switch DISABLED for testing" -ForegroundColor Yellow
    Write-Host ""
}

$hookScript = Join-Path $PSScriptRoot "..\hooks\auto-context.ps1"

foreach ($file in $testFiles) {
    Write-Host "==============================================" -ForegroundColor Cyan
    Write-Host "Testing: $file" -ForegroundColor Cyan
    Write-Host "==============================================" -ForegroundColor Cyan
    Write-Host ""

    $env:FILE_PATH = $file
    & $hookScript
    Write-Host ""
}

# Show final tracker state
Write-Host ""
Write-Host "============================================== " -ForegroundColor Magenta
Write-Host "Final Context Tracker State" -ForegroundColor Magenta
Write-Host "==============================================" -ForegroundColor Magenta
Write-Host ""

$trackerScript = Join-Path $PSScriptRoot "context-tracker.ps1"
& $trackerScript -Action status
