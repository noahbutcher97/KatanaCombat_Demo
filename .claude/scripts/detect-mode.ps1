# Intelligent Mode Detection with Confidence Scoring
# Analyzes file paths and returns suggested mode with confidence level

param(
    [Parameter(Mandatory=$true)]
    [string]$FilePath,

    [Parameter(Mandatory=$false)]
    [switch]$ShowDetails
)

# Input validation
if ([string]::IsNullOrWhiteSpace($FilePath)) {
    $errorResult = @{
        suggestedMode = "full"
        confidence = 0.0
        confidenceLevel = "none"
        reason = "Empty or null file path"
        allScores = @{}
    }
    if ($ShowDetails) {
        Write-Host "[ERROR] Empty or null file path provided" -ForegroundColor Red
    } else {
        $errorResult | ConvertTo-Json -Depth 10
    }
    exit 1
}

# Normalize path and make case-insensitive for Windows compatibility
$normalizedPath = $FilePath -replace '\\', '/'

# Mode detection patterns with weighted scoring
# Structure: @{ pattern = regex; weight = confidence boost (0.0-1.0) }
$modePatterns = @{
    animation = @(
        @{ pattern = 'AnimNotify'; weight = 0.95 }
        @{ pattern = 'AnimNotifyState'; weight = 0.95 }
        @{ pattern = 'Animation/'; weight = 0.90 }
        @{ pattern = 'MontageUtility'; weight = 0.85 }
        @{ pattern = 'AnimInstance'; weight = 0.80 }
        @{ pattern = 'Montage'; weight = 0.75 }
        @{ pattern = 'Phase.*Transition'; weight = 0.70 }
        @{ pattern = 'Blend'; weight = 0.40 }
    )

    'combat-logic' = @(
        @{ pattern = 'CombatComponent(?!.*Test)'; weight = 0.95 }
        @{ pattern = 'ActionQueue'; weight = 0.90 }
        @{ pattern = 'TargetingComponent'; weight = 0.85 }
        @{ pattern = 'WeaponComponent'; weight = 0.85 }
        @{ pattern = 'HitReactionComponent'; weight = 0.85 }
        @{ pattern = 'CombatTypes\.h'; weight = 0.80 }
        @{ pattern = 'Core/(?!.*Test)'; weight = 0.70 }
        @{ pattern = 'Input'; weight = 0.50 }
    )

    'data-assets' = @(
        @{ pattern = 'AttackData\.'; weight = 0.95 }
        @{ pattern = 'AttackConfiguration\.'; weight = 0.95 }
        @{ pattern = 'CombatSettings\.'; weight = 0.90 }
        @{ pattern = 'Data/'; weight = 0.80 }
        @{ pattern = 'Content/Data/'; weight = 0.85 }
        @{ pattern = '\.uasset$'; weight = 0.60 }
    )

    'editor-ui' = @(
        @{ pattern = 'Editor/'; weight = 0.95 }
        @{ pattern = 'AssetEditor'; weight = 0.90 }
        @{ pattern = 'Customization'; weight = 0.90 }
        @{ pattern = 'Factory\.'; weight = 0.85 }
        @{ pattern = 'Slate/'; weight = 0.80 }
        @{ pattern = 'KatanaCombatEditor/'; weight = 0.85 }
        @{ pattern = '\.Target\.cs$'; weight = 0.70 }
        @{ pattern = '\.uproject$'; weight = 0.50 }
    )

    testing = @(
        @{ pattern = 'Test\.cpp$'; weight = 0.95 }
        @{ pattern = 'Test\.h$'; weight = 0.95 }
        @{ pattern = 'Spec\.cpp$'; weight = 0.95 }
        @{ pattern = 'Tests/'; weight = 0.90 }
        @{ pattern = 'KatanaCombatTest/'; weight = 0.85 }
        @{ pattern = 'Mock'; weight = 0.60 }
    )

    documentation = @(
        @{ pattern = '\.md$'; weight = 0.95 }
        @{ pattern = 'docs/'; weight = 0.90 }
        @{ pattern = 'README'; weight = 0.85 }
        @{ pattern = 'CLAUDE\.md'; weight = 0.80 }
        @{ pattern = 'CHANGELOG'; weight = 0.75 }
    )
}

# Calculate confidence for each mode
$modeScores = @{}

foreach ($modeName in $modePatterns.Keys) {
    $totalScore = 0.0
    $matchCount = 0
    $matches = @()

    foreach ($patternObj in $modePatterns[$modeName]) {
        try {
            # Case-insensitive matching for Windows compatibility
            if ($normalizedPath -imatch $patternObj.pattern) {
                $totalScore += $patternObj.weight
                $matchCount++
                $matches += @{
                    pattern = $patternObj.pattern
                    weight = $patternObj.weight
                }
            }
        }
        catch {
            # Regex error - skip this pattern and continue
            if ($ShowDetails) {
                Write-Host "[WARN] Regex error for pattern '$($patternObj.pattern)': $_" -ForegroundColor Yellow
            }
            continue
        }
    }

    # Normalize score (cap at 1.0, boost for multiple matches)
    if ($matchCount -gt 0) {
        # Base confidence is highest single match
        $baseConfidence = ($matches | ForEach-Object { $_.weight } | Measure-Object -Maximum).Maximum

        # Bonus for multiple matches (diminishing returns)
        $multiMatchBonus = [Math]::Min(0.15, ($matchCount - 1) * 0.05)

        $finalConfidence = [Math]::Min(1.0, $baseConfidence + $multiMatchBonus)

        $modeScores[$modeName] = @{
            confidence = $finalConfidence
            matchCount = $matchCount
            matches = $matches
        }
    }
}

# Find best match
if ($modeScores.Count -eq 0) {
    # No matches - suggest full mode
    $result = @{
        suggestedMode = "full"
        confidence = 0.0
        confidenceLevel = "none"
        reason = "No specific patterns detected"
        matches = @()
    }
} else {
    # Get mode with highest confidence
    $bestMode = $modeScores.GetEnumerator() | Sort-Object { $_.Value.confidence } -Descending | Select-Object -First 1

    # Determine confidence level
    $confidenceValue = $bestMode.Value.confidence
    $confidenceLevel = if ($confidenceValue -ge 0.80) { "high" }
                      elseif ($confidenceValue -ge 0.50) { "medium" }
                      else { "low" }

    # Build reason
    $matchPatterns = $bestMode.Value.matches | ForEach-Object { $_.pattern }
    $reason = "Matched $($bestMode.Value.matchCount) pattern(s): $($matchPatterns -join ', ')"

    # Convert allScores to serializable format
    $scoresForJson = @{}
    foreach ($mode in $modeScores.Keys) {
        $scoresForJson[$mode] = @{
            confidence = $modeScores[$mode].confidence
            matchCount = $modeScores[$mode].matchCount
        }
    }

    $result = @{
        suggestedMode = $bestMode.Key
        confidence = $confidenceValue
        confidenceLevel = $confidenceLevel
        reason = $reason
        allScores = $scoresForJson
    }
}

# Output result as JSON for easy parsing
if ($ShowDetails) {
    Write-Host ""
    Write-Host "=== Mode Detection Analysis ===" -ForegroundColor Cyan
    Write-Host "File: $FilePath" -ForegroundColor Gray
    Write-Host ""
    Write-Host "Suggested Mode: $($result.suggestedMode)" -ForegroundColor Green
    Write-Host "Confidence: $([Math]::Round($result.confidence * 100, 1))% ($($result.confidenceLevel))" -ForegroundColor Yellow
    Write-Host "Reason: $($result.reason)" -ForegroundColor Gray
    Write-Host ""

    if ($result.allScores) {
        Write-Host "All Mode Scores:" -ForegroundColor Cyan
        foreach ($mode in ($result.allScores.GetEnumerator() | Sort-Object { $_.Value.confidence } -Descending)) {
            $pct = [Math]::Round($mode.Value.confidence * 100, 1)
            $bar = "#" * [Math]::Min([Math]::Ceiling($pct / 5), 20)
            Write-Host "  $($mode.Key.PadRight(15)): $bar $pct%" -ForegroundColor Gray
        }
        Write-Host ""
    }
} else {
    # Output JSON for programmatic use
    $result | ConvertTo-Json -Depth 10
}
