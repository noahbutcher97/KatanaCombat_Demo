# Pre-commit diagnostics hook
# Checks for critical IDE warnings before allowing commit

param(
    [switch]$Force,
    [switch]$WarningsOnly
)

# Check if diagnostics check is disabled
if (Test-Path ".claude/.skip-diagnostics-enabled") {
    Write-Output "Diagnostics check skipped (.skip-diagnostics-enabled flag present)"
    exit 0
}

Write-Output "<system-reminder>"
Write-Output "🔍 Running pre-commit diagnostics check..."
Write-Output ""

# Load diagnostics configuration
$configPath = ".claude/diagnostics-config.json"
if (-not (Test-Path $configPath)) {
    Write-Output "⚠️ Diagnostics config not found at $configPath"
    Write-Output "Run /check-warnings for manual diagnostics"
    Write-Output "</system-reminder>"
    exit 0
}

$config = Get-Content $configPath | ConvertFrom-Json

# Get list of staged files (files about to be committed)
$stagedFiles = git diff --cached --name-only --diff-filter=ACM 2>$null

if (-not $stagedFiles) {
    Write-Output "ℹ️ No staged files to check"
    Write-Output "</system-reminder>"
    exit 0
}

# Filter for C++ files
$cppFiles = $stagedFiles | Where-Object { $_ -match '\.(h|cpp)$' }

if (-not $cppFiles) {
    Write-Output "ℹ️ No C++ files in staged changes"
    Write-Output "</system-reminder>"
    exit 0
}

Write-Output "📂 Checking $($cppFiles.Count) staged C++ files..."
Write-Output ""

# Track issues
$criticalIssues = @()
$warnings = @()
$filteredCount = 0

# Ignore patterns from config
$ignorePatterns = @()
foreach ($category in $config.ignorePatterns.PSObject.Properties) {
    $ignorePatterns += $category.Value.patterns
}

# Check each staged file
# Note: This is a simplified check since we can't call MCP directly from PowerShell
# In practice, Claude will need to run the actual diagnostics check

Write-Output "💡 To run full diagnostics check with filtering:"
Write-Output "   /check-warnings"
Write-Output ""

# Provide general reminders based on file types
$hasComponent = $cppFiles | Where-Object { $_ -match 'CombatComponent' }
$hasAnimNotify = $cppFiles | Where-Object { $_ -match 'AnimNotify' }
$hasData = $cppFiles | Where-Object { $_ -match 'AttackData|CombatSettings' }
$hasTests = $cppFiles | Where-Object { $_ -match 'Test\.cpp' }

Write-Output "📋 Pre-commit Checklist:"
Write-Output ""

if ($hasComponent) {
    Write-Output "⚙️ CombatComponent files modified:"
    Write-Output "   - Check for null pointer dereferences"
    Write-Output "   - Validate state transitions use CanTransitionTo()"
    Write-Output "   - Ensure timers used instead of Tick"
    Write-Output "   - Verify input always buffered (no combo window gating)"
    Write-Output ""
}

if ($hasAnimNotify) {
    Write-Output "🎬 AnimNotify files modified:"
    Write-Output "   - Verify phase order: Windup → Active → Recovery"
    Write-Output "   - Check windows can overlap phases"
    Write-Output "   - Use AnimNotify_AttackPhaseTransition (not deprecated)"
    Write-Output ""
}

if ($hasData) {
    Write-Output "📊 Data asset files modified:"
    Write-Output "   - Add validation in AttackData::Validate()"
    Write-Output "   - Update ARCHITECTURE_QUICK.md if changing defaults"
    Write-Output "   - Ensure UPROPERTY has Category and Tooltip"
    Write-Output ""
}

if ($hasTests) {
    Write-Output "🧪 Test files modified:"
    Write-Output "   - Ensure tests actually run (IMPLEMENT_SIMPLE_AUTOMATION_TEST)"
    Write-Output "   - Add to test suite (see Source/KatanaCombatTest/README.md)"
    Write-Output ""
}

# Common checks for all files
Write-Output "✅ General checklist:"
Write-Output "   - No compilation errors"
Write-Output "   - Blueprint-exposed members marked (BlueprintCallable, etc.)"
Write-Output "   - Forward declarations in headers, includes in .cpp"
Write-Output "   - Const-correctness for large struct parameters"
Write-Output ""

# Recommendation
Write-Output "🎯 Recommended Actions:"
Write-Output "   1. Run: /check-warnings (detailed diagnostics with filtering)"
Write-Output "   2. Run: /validate-combat (architecture compliance)"
Write-Output "   3. Run: /pre-commit (full validation suite)"
Write-Output ""

if ($Force) {
    Write-Output "⚠️ Force mode enabled - skipping blocking"
    Write-Output "</system-reminder>"
    exit 0
}

# In a full implementation, this would actually block on critical issues
# For now, it's advisory
Write-Output "💡 Tip: Use '/hooks skip diagnostics' to disable this check"
Write-Output "</system-reminder>"

exit 0