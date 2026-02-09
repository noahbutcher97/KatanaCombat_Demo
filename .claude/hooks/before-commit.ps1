# Before-commit hook - Validation enforcer
# Runs comprehensive checks before allowing commits

param(
    [switch]$Force,
    [switch]$Quick
)

# Check if disabled
if (Test-Path ".claude/.skip-validation-enabled") {
    Write-Output "Validation skipped (.skip-validation-enabled flag present)"
    exit 0
}

Write-Output "<system-reminder>"
Write-Output "[VALIDATION] Pre-Commit Validation Enforcer"
Write-Output ""

# Get staged files
$stagedFiles = git diff --cached --name-only --diff-filter=ACM 2>$null

if (-not $stagedFiles) {
    Write-Output "[INFO] No staged files to validate"
    Write-Output "</system-reminder>"
    exit 0
}

Write-Output "[FILES] Staged Files: $($stagedFiles.Count)"
Write-Output ""

# Track validation results
$errors = @()
$warnings = @()
$suggestions = @()

# Filter by file type
$cppFiles = @($stagedFiles | Where-Object { $_ -match '\.(h|cpp)$' })
$docFiles = @($stagedFiles | Where-Object { $_ -match '\.md$' })
$configFiles = @($stagedFiles | Where-Object { $_ -match '\.(ini|json|uproject)$' })

# === VALIDATION CHECKS ===

## 1. Critical Files Check
$criticalFiles = @(
    'CombatTypes.h',
    'CLAUDE.md',
    'KatanaCombat.uproject'
)

foreach ($file in $stagedFiles) {
    $fileName = Split-Path $file -Leaf
    if ($criticalFiles -contains $fileName) {
        $warnings += "[WARNING] Modifying critical file: $fileName"
        $suggestions += "Review: $file (critical system file)"
    }
}

## 2. Component Files - Architecture Compliance
$componentFiles = $cppFiles | Where-Object { $_ -match 'Component.*\.(h|cpp)' }
if ($componentFiles.Count -gt 0) {
    $warnings += "[COMPONENTS] Component files modified ($($componentFiles.Count))"
    $suggestions += "Run: /validate-combat (check architecture compliance)"
}

## 3. AnimNotify Files - Phase System Compliance
$animNotifyFiles = $cppFiles | Where-Object { $_ -match 'AnimNotify' }
if ($animNotifyFiles.Count -gt 0) {
    $warnings += "[ANIM] AnimNotify files modified ($($animNotifyFiles.Count))"
    $suggestions += "Verify: Phase transitions follow Windup→Active→Recovery"
}

## 4. Test Files - Must Pass
$testFiles = $cppFiles | Where-Object { $_ -match 'Test\.cpp' }
if ($testFiles.Count -gt 0) {
    $warnings += "🧪 Test files modified ($($testFiles.Count))"
    $suggestions += "Run: Automation tests before commit"
}

## 5. Documentation Files - Date Check
if ($docFiles.Count -gt 0) {
    foreach ($docFile in $docFiles) {
        # Check if CLAUDE.md and it's being modified
        if ($docFile -match 'CLAUDE\.md') {
            # Read first 50 lines to check for recent date
            $today = Get-Date -Format "yyyy-MM-dd"
            $recentDates = @(
                $today,
                (Get-Date).AddDays(-1).ToString("yyyy-MM-dd"),
                (Get-Date).AddDays(-2).ToString("yyyy-MM-dd")
            )

            $hasRecentDate = $false
            if (Test-Path $docFile) {
                $content = Get-Content $docFile -Head 100 | Out-String
                foreach ($date in $recentDates) {
                    if ($content -match $date) {
                        $hasRecentDate = $true
                        break
                    }
                }
            }

            if (-not $hasRecentDate) {
                $warnings += "[DATE] CLAUDE.md may need date update (Recent Changes section)"
            }
        }
    }
}

## 6. Large Commits Check
if ($stagedFiles.Count -gt 20) {
    $warnings += "[LARGE] Large commit ($($stagedFiles.Count) files) - consider breaking up"
}

## 7. Mixed Concern Check (anti-pattern)
$hasComponent = $cppFiles | Where-Object { $_ -match 'Component' }
$hasData = $cppFiles | Where-Object { $_ -match 'Data/Attack' }
$hasAnimation = $cppFiles | Where-Object { $_ -match 'Animation' }

$concernCount = 0
if ($hasComponent) { $concernCount++ }
if ($hasData) { $concernCount++ }
if ($hasAnimation) { $concernCount++ }

if ($concernCount -gt 2) {
    $warnings += "[MIXED] Mixed concerns in commit (components + data + animation)"
    $suggestions += "Consider: Separate commits for different system changes"
}

# === OUTPUT RESULTS ===

if ($errors.Count -gt 0) {
    Write-Output "[ERROR] **VALIDATION FAILED** ($($errors.Count) errors)"
    Write-Output ""
    foreach ($error in $errors) {
        Write-Output "  $error"
    }
    Write-Output ""

    if (-not $Force) {
        Write-Output "[BLOCKED] **COMMIT BLOCKED**"
        Write-Output ""
        Write-Output "Fix errors or use --force to bypass"
        Write-Output "</system-reminder>"
        exit 1
    }
}

if ($warnings.Count -gt 0) {
    Write-Output "[WARNING] **Warnings** ($($warnings.Count))"
    Write-Output ""
    foreach ($warning in $warnings) {
        Write-Output "  $warning"
    }
    Write-Output ""
}

# === RECOMMENDED ACTIONS ===

Write-Output "[OK] **Recommended Pre-Commit Actions**"
Write-Output ""

# Context-based recommendations
if ($cppFiles.Count -gt 0) {
    Write-Output "  1. /check-warnings - Check IDE diagnostics"
}

if ($componentFiles.Count -gt 0) {
    Write-Output "  2. /validate-combat - Validate architecture compliance"
}

if ($docFiles.Count -gt 0) {
    Write-Output "  3. /sync-docs - Verify doc alignment"
}

if (-not $Quick) {
    Write-Output "  4. /pre-commit - Full validation suite"
}

Write-Output ""

# === SPECIFIC SUGGESTIONS ===

if ($suggestions.Count -gt 0) {
    Write-Output "[SUGGESTIONS] **Specific Suggestions**"
    Write-Output ""
    foreach ($suggestion in $suggestions) {
        Write-Output "  • $suggestion"
    }
    Write-Output ""
}

# === QUICK MODE INFO ===

if ($Quick) {
    Write-Output "[QUICK] Quick mode - skipped detailed checks"
    Write-Output ""
}

# === BYPASS OPTIONS ===

Write-Output "[BYPASS] **Bypass Options**"
Write-Output "  - Force commit: git commit --no-verify"
Write-Output "  - Skip validation: Use '/hooks skip validation'"
Write-Output "  - Quick mode: Use --quick flag"
Write-Output ""

Write-Output "</system-reminder>"

# Exit codes:
# 0 = Pass (warnings are advisory)
# 1 = Fail (errors found, blocked)
exit 0
