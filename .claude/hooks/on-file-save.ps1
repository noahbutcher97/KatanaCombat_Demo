# On-file-save hook - Quick style and formatting checks
# Runs lightweight checks on save for immediate feedback

param(
    [string]$FilePath = $env:FILE_PATH
)

# Check if disabled
if (Test-Path ".claude/.skip-on-save-enabled") { exit 0 }

if (-not $FilePath -or -not (Test-Path $FilePath)) { exit 0 }

# Only check C++ files
if ($FilePath -notmatch '\.(h|cpp)$') { exit 0 }

# Read file content
$content = Get-Content $FilePath -Raw

if (-not $content) { exit 0 }

# Track issues found
$styleIssues = @()
$suggestions = @()
$goodPractices = @()

# === QUICK STYLE CHECKS ===

## 1. Include Guard Check (headers only)
if ($FilePath -match '\.h$') {
    if ($content -notmatch '#pragma once') {
        $styleIssues += "Missing #pragma once include guard"
    }
}

## 2. UPROPERTY without Category
if ($content -match 'UPROPERTY\([^)]*\)' -and $content -notmatch 'UPROPERTY\([^)]*Category\s*=') {
    $matches = [regex]::Matches($content, 'UPROPERTY\((?![^)]*Category)[^)]*\)')
    if ($matches.Count -gt 0) {
        $styleIssues += "Found UPROPERTY without Category ($($matches.Count) instances)"
        $suggestions += "Add Category = ""ComponentName|GroupName"" to UPROPERTY"
    }
}

## 3. BlueprintCallable without DisplayName/Tooltip
if ($content -match 'UFUNCTION\([^)]*BlueprintCallable[^)]*\)') {
    # Check if missing DisplayName or Tooltip
    $matches = [regex]::Matches($content, 'UFUNCTION\((?![^)]*DisplayName)(?![^)]*ToolTip)[^)]*BlueprintCallable[^)]*\)')
    if ($matches.Count -gt 0) {
        $suggestions += "Consider adding DisplayName and ToolTip to BlueprintCallable functions"
    }
}

## 4. Null Pointer Access Patterns (basic check)
$nullAccessPatterns = @(
    '\w+->\w+\s*\(' # Direct call without null check
)

# This is a simplified check - full analysis needs actual parsing
if ($content -match '(?<!if\s*\([^)]*)\w+->\w+' -and $content -notmatch 'ensure\(') {
    $suggestions += "Consider null checks before pointer dereference (use ensure() or if checks)"
}

## 5. TODO/FIXME/HACK Comments
$todoCount = ([regex]::Matches($content, '//\s*TODO:')).Count
$fixmeCount = ([regex]::Matches($content, '//\s*FIXME:')).Count
$hackCount = ([regex]::Matches($content, '//\s*HACK:')).Count

if ($todoCount -gt 0 -or $fixmeCount -gt 0 -or $hackCount -gt 0) {
    $suggestions += "Technical debt: $todoCount TODOs, $fixmeCount FIXMEs, $hackCount HACKs"
}

## 6. Large Functions (rough estimate)
$functionMatches = [regex]::Matches($content, '\n\s*\w+::\w+\([^)]*\)(?:[^{]*\{)')
foreach ($match in $functionMatches) {
    $functionStart = $match.Index
    $bracketCount = 1
    $charIndex = $functionStart + $match.Length

    # Find matching closing brace
    while ($bracketCount -gt 0 -and $charIndex -lt $content.Length) {
        if ($content[$charIndex] -eq '{') { $bracketCount++ }
        if ($content[$charIndex] -eq '}') { $bracketCount-- }
        $charIndex++
    }

    $functionLength = $charIndex - $functionStart
    $lineCount = ($content.Substring($functionStart, $functionLength) -split "`n").Count

    if ($lineCount -gt 100) {
        $functionName = if ($match.Value -match '(\w+)::\w+') { $matches[1] } else { "Unknown" }
        $styleIssues += "Large function detected (~$lineCount lines) - consider refactoring"
    }
}

## 7. Tick Usage Check (performance anti-pattern)
if ($content -match 'TickComponent|ActorTick' -and $content -notmatch 'PrimaryComponentTick\.bCanEverTick\s*=\s*false') {
    $styleIssues += "⚠️ Tick usage detected - KatanaCombat uses timer-based approach"
    $suggestions += "Replace Tick with FTimerManager or event-driven approach"
}

## 8. Delegate Declaration Check
if ($FilePath -notmatch 'CombatTypes\.h' -and $content -match 'DECLARE_DYNAMIC_MULTICAST_DELEGATE') {
    $styleIssues += "❌ Delegate declared outside CombatTypes.h (architecture violation)"
    $suggestions += "Move delegate declarations to CombatTypes.h"
}

## 9. Good Practices Detection
if ($content -match 'ensure\(') {
    $goodPractices += "✅ Using ensure() for validation"
}

if ($content -match 'UPROPERTY\([^)]*EditAnywhere[^)]*meta\s*=\s*\([^)]*ClampMin') {
    $goodPractices += "✅ Using ClampMin/ClampMax for numeric properties"
}

if ($content -match '\/\/\s*NOLINT') {
    $goodPractices += "✅ Marking intentionally unused code with NOLINT"
}

# === OUTPUT RESULTS ===

if ($styleIssues.Count -gt 0 -or $suggestions.Count -gt 0 -or $goodPractices.Count -gt 0) {
    $output = "<system-reminder>`n"
    $output += "🎨 Quick Style Check (On Save)`n`n"

    $fileName = Split-Path $FilePath -Leaf
    $output += "**File**: $fileName`n`n"

    if ($styleIssues.Count -gt 0) {
        $output += "⚠️ **Style Issues** ($($styleIssues.Count)):`n"
        foreach ($issue in $styleIssues) {
            $output += "  - $issue`n"
        }
        $output += "`n"
    }

    if ($suggestions.Count -gt 0) {
        $output += "💡 **Suggestions** ($($suggestions.Count)):`n"
        foreach ($suggestion in $suggestions) {
            $output += "  - $suggestion`n"
        }
        $output += "`n"
    }

    if ($goodPractices.Count -gt 0 -and $goodPractices.Count -le 3) {
        $output += "✅ **Good Practices Detected**:`n"
        foreach ($practice in $goodPractices) {
            $output += "  - $practice`n"
        }
        $output += "`n"
    }

    $output += "🔧 **Actions**:`n"
    $output += "  - Detailed check: /check-warnings`n"
    $output += "  - Disable: Use '/hooks skip on-save'`n`n"

    $output += "</system-reminder>"

    Write-Output $output
}
