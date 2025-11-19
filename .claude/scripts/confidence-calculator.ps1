# Confidence Calculator for Intelligent Context Switching
# Multi-factor confidence scoring using Bayesian inference + topic analysis

param(
    [Parameter(Mandatory=$true)]
    [string]$FilePath,

    [Parameter(Mandatory=$false)]
    [string[]]$ConversationKeywords = @(),

    [Parameter(Mandatory=$false)]
    [string]$CurrentMode = "full"
)

# Load learning data
$learningFile = ".claude/.context-learning.json"
if (-not (Test-Path $learningFile)) {
    # Return default confidence if no learning data
    return [PSCustomObject]@{
        Overall = 0.5
        FilePattern = 0.5
        Topic = 0.0
        History = 0.5
        Time = 0.5
        SuggestedMode = "full"
        Reason = "No learning data available"
    }
}

$learningData = Get-Content $learningFile -Raw | ConvertFrom-Json

# Keyword categories for topic detection
$keywordMap = @{
    animation = @('AnimNotify', 'montage', 'animation', 'blend', 'notify', 'phase', 'anim')
    'combat-logic' = @('combat', 'attack', 'combo', 'input', 'state', 'action', 'queue')
    'data-assets' = @('AttackData', 'asset', 'property', 'config', 'data', 'PDA', 'settings')
    'editor-ui' = @('editor', 'Slate', 'UI', 'panel', 'widget', 'customization', 'factory')
    testing = @('test', 'spec', 'assert', 'mock', 'fixture', 'automation', 'verify')
    documentation = @('docs', 'documentation', 'guide', 'README', 'tutorial', 'reference')
}

# ============================================================================
# Factor 1: File Pattern Confidence (40% weight)
# ============================================================================
function Get-FilePatternConfidence {
    param([string]$File, [object]$Learning)

    $fileName = Split-Path $File -Leaf
    $filePath = $File -replace '\\', '/'

    # Check for exact pattern match first
    $exactMatch = $null
    $bestPartialMatch = $null
    $bestPartialConf = 0.0

    foreach ($patternProp in $Learning.patterns.PSObject.Properties) {
        $patternName = $patternProp.Name
        $pattern = $patternProp.Value

        # Exact match
        if ($fileName -eq $patternName -or $filePath -match [regex]::Escape($patternName)) {
            $exactMatch = $pattern
            break
        }

        # Partial match (pattern contains file keywords)
        if ($fileName -match $patternName -or $patternName -match $fileName.Split('.')[0]) {
            # Calculate Bayesian confidence for this pattern
            $successCount = if ($pattern.bayesian.PSObject.Properties.Name -contains 'successCount') {
                [int]$pattern.bayesian.successCount
            } else { 0 }
            $failureCount = if ($pattern.bayesian.PSObject.Properties.Name -contains 'failureCount') {
                [int]$pattern.bayesian.failureCount
            } else { 0 }

            if ($successCount + $failureCount -gt 0) {
                $successRate = $successCount / ($successCount + $failureCount)
                $temporal = if ($pattern.PSObject.Properties.Name -contains 'temporal') {
                    if ($pattern.temporal.PSObject.Properties.Name -contains 'decay') { $pattern.temporal.decay } else { 1.0 }
                } else { 1.0 }

                $conf = $successRate * $temporal
                if ($conf > $bestPartialConf) {
                    $bestPartialConf = $conf
                    $bestPartialMatch = $pattern
                }
            }
        }
    }

    $matchedPattern = if ($exactMatch) { $exactMatch } else { $bestPartialMatch }

    if ($matchedPattern) {
        $successCount = [int]$matchedPattern.bayesian.successCount
        $failureCount = [int]$matchedPattern.bayesian.failureCount
        $totalAttempts = $successCount + $failureCount

        if ($totalAttempts -eq 0) { return @{ confidence = 0.5; mode = $matchedPattern.mode } }

        $successRate = $successCount / $totalAttempts

        # Apply Bayesian prior (neutral = 0.5)
        $prior = 0.5
        $weight = [Math]::Min(1.0, $totalAttempts / 10.0)  # Full weight at 10+ samples

        # Apply temporal decay if available
        $temporal = if ($matchedPattern.PSObject.Properties.Name -contains 'temporal') {
            if ($matchedPattern.temporal.PSObject.Properties.Name -contains 'decayFactor') {
                $matchedPattern.temporal.decayFactor
            } else { 1.0 }
        } else { 1.0 }

        $confidence = (($successRate * $weight) + ($prior * (1 - $weight))) * $temporal

        return @{
            confidence = $confidence
            mode = $matchedPattern.mode
            matchType = if ($exactMatch) { "exact" } else { "partial" }
            samples = $totalAttempts
        }
    }

    # No match - use file extension/path heuristics
    $mode = Get-ModeFromFilePath -File $File
    return @{ confidence = 0.3; mode = $mode; matchType = "heuristic"; samples = 0 }
}

# ============================================================================
# Factor 2: Topic/Keyword Confidence (30% weight)
# ============================================================================
function Get-TopicConfidence {
    param([string[]]$Keywords, [string]$SuggestedMode)

    if ($Keywords.Count -eq 0) { return @{ confidence = 0.0; mode = $SuggestedMode } }

    # Count keyword matches per mode
    $modeScores = @{}
    foreach ($mode in $keywordMap.Keys) {
        $modeKeywords = $keywordMap[$mode]
        $matchCount = 0

        foreach ($keyword in $Keywords) {
            foreach ($modeKeyword in $modeKeywords) {
                if ($keyword -match $modeKeyword) {
                    $matchCount++
                    break
                }
            }
        }

        $modeScores[$mode] = $matchCount / [Math]::Max(1, $Keywords.Count)
    }

    # Find best matching mode
    $bestMode = $SuggestedMode
    $bestScore = 0.0

    foreach ($mode in $modeScores.Keys) {
        if ($modeScores[$mode] -gt $bestScore) {
            $bestScore = $modeScores[$mode]
            $bestMode = $mode
        }
    }

    # If suggested mode matches conversation, boost confidence
    if ($bestMode -eq $SuggestedMode -and $bestScore -gt 0) {
        $confidence = [Math]::Min(1.0, $bestScore * 1.5)  # Boost for alignment
    } else {
        $confidence = $bestScore
    }

    return @{ confidence = $confidence; mode = $bestMode; matches = $modeScores }
}

# ============================================================================
# Factor 3: Historical Accuracy (20% weight)
# ============================================================================
function Get-HistoricalConfidence {
    param([string]$File, [string]$Mode, [object]$Learning)

    # Check if this specific file has been used before
    $fileName = Split-Path $File -Leaf

    foreach ($patternProp in $Learning.patterns.PSObject.Properties) {
        $pattern = $patternProp.Value

        if ($pattern.mode -eq $Mode -and $fileName -match $patternProp.Name) {
            $successCount = [int]$pattern.bayesian.successCount
            $failureCount = [int]$pattern.bayesian.failureCount
            $total = $successCount + $failureCount

            if ($total -eq 0) { return 0.5 }

            return $successCount / $total
        }
    }

    # Check mode-wide success rate
    if ($Learning.PSObject.Properties.Name -contains 'globalStats') {
        $accuracy = $Learning.globalStats.autoSwitchAccuracy
        return $accuracy
    }

    return 0.5  # Neutral if no history
}

# ============================================================================
# Factor 4: Time-Based Patterns (10% weight)
# ============================================================================
function Get-TimeBasedConfidence {
    param([object]$Learning)

    $currentHour = (Get-Date).Hour
    $currentDay = (Get-Date).DayOfWeek

    # Simple heuristic: working hours boost
    if ($currentHour -ge 9 -and $currentHour -le 17 -and $currentDay -ne 'Saturday' -and $currentDay -ne 'Sunday') {
        return 0.7  # Higher confidence during work hours
    } elseif ($currentHour -ge 18 -and $currentHour -le 23) {
        return 0.6  # Medium confidence evening
    } else {
        return 0.4  # Lower confidence late night/early morning
    }
}

# ============================================================================
# Helper: Mode Detection from File Path
# ============================================================================
function Get-ModeFromFilePath {
    param([string]$File)

    $fileLower = $File.ToLower()

    if ($fileLower -match 'animnotify|animation|montage') { return 'animation' }
    if ($fileLower -match 'combatcomponent|actionqueue|input') { return 'combat-logic' }
    if ($fileLower -match 'attackdata|settings|config.*pda') { return 'data-assets' }
    if ($fileLower -match 'editor|customization|factory|slate') { return 'editor-ui' }
    if ($fileLower -match 'test\.cpp|spec\.cpp') { return 'testing' }
    if ($fileLower -match '\.md$|readme|docs/') { return 'documentation' }

    return 'full'
}

# ============================================================================
# Main: Calculate Overall Confidence
# ============================================================================

# Factor 1: File Pattern (40%)
$fileResult = Get-FilePatternConfidence -File $FilePath -Learning $learningData
$fileConfidence = $fileResult.confidence
$suggestedMode = $fileResult.mode

# Factor 2: Topic/Keywords (30%)
$topicResult = Get-TopicConfidence -Keywords $ConversationKeywords -SuggestedMode $suggestedMode
$topicConfidence = $topicResult.confidence

# Factor 3: Historical (20%)
$historyConfidence = Get-HistoricalConfidence -File $FilePath -Mode $suggestedMode -Learning $learningData

# Factor 4: Time-based (10%)
$timeConfidence = Get-TimeBasedConfidence -Learning $learningData

# Weighted combination
$overallConfidence = (
    ($fileConfidence * 0.4) +
    ($topicConfidence * 0.3) +
    ($historyConfidence * 0.2) +
    ($timeConfidence * 0.1)
)

# Build reason string
$reasons = @()
if ($fileResult.matchType -eq "exact") {
    $reasons += "Exact pattern match ($($fileResult.samples) samples)"
} elseif ($fileResult.matchType -eq "partial") {
    $reasons += "Partial pattern match ($($fileResult.samples) samples)"
} else {
    $reasons += "File path heuristic"
}

if ($topicConfidence -gt 0.3) {
    $reasons += "Topic keywords match"
}

if ($fileResult.samples -ge 5) {
    $reasons += "High sample size ($($fileResult.samples))"
}

# Return comprehensive confidence object
[PSCustomObject]@{
    Overall = [Math]::Round($overallConfidence, 3)
    FilePattern = [Math]::Round($fileConfidence, 3)
    Topic = [Math]::Round($topicConfidence, 3)
    History = [Math]::Round($historyConfidence, 3)
    Time = [Math]::Round($timeConfidence, 3)
    SuggestedMode = $suggestedMode
    CurrentMode = $CurrentMode
    ShouldSwitch = ($suggestedMode -ne $CurrentMode -and $overallConfidence -ge 0.5)
    Reason = ($reasons -join ", ")
    MatchType = $fileResult.matchType
    Samples = $fileResult.samples
}
