# Intelligent Mode Detection System v3.0
# Unified 5-factor confidence scoring with ML integration
# Optimized for <50ms latency, production-ready
#
# Architecture: Merges holistic-mode-detector.ps1 + confidence-calculator.ps1
# 5 Factors:
#   1. File Pattern Analysis (35%) - Path/extension/content heuristics
#   2. Conversation Topic Analysis (25%) - Keyword extraction + intent detection
#   3. Learned Pattern Matching (20%) - Bayesian inference + temporal decay
#   4. Historical Success Rate (15%) - Per-file/per-mode accuracy tracking
#   5. Time-Based Patterns (5%) - Work hours heuristics

param(
    [Parameter(Mandatory=$false)]
    [string]$FilePath = "",

    [Parameter(Mandatory=$false)]
    [string]$ConversationText = "",

    [Parameter(Mandatory=$false)]
    [string]$CurrentMode = "full",

    [Parameter(Mandatory=$false)]
    [switch]$ShowDetails
)

$ErrorActionPreference = "SilentlyContinue"
$scriptDir = $PSScriptRoot

# 5-factor weights (tuned for optimal accuracy, configurable)
$weights = @{
    file = 0.35           # File path/extension/content patterns
    conversation = 0.25   # Topic extraction from conversation
    learning = 0.20       # ML-learned patterns with Bayesian inference
    history = 0.15        # Historical success rate for this file/mode
    time = 0.05           # Time-of-day heuristics
}

# Load weights from config if available
$configFile = Join-Path $scriptDir ".." | Join-Path -ChildPath ".context-config.json"
if (Test-Path $configFile) {
    try {
        $config = Get-Content $configFile | ConvertFrom-Json
        if ($config.intelligentSwitching.PSObject.Properties.Name -contains 'factorWeights') {
            $configWeights = $config.intelligentSwitching.factorWeights
            foreach ($key in @('file', 'conversation', 'learning', 'history', 'time')) {
                if ($configWeights.PSObject.Properties.Name -contains $key) {
                    $weights[$key] = [double]$configWeights.$key
                }
            }
        }
    } catch {
        # Config load failed, use defaults
    }
}

# Keyword categories for topic detection (shared with confidence-calculator)
$keywordMap = @{
    animation = @('AnimNotify', 'montage', 'animation', 'blend', 'notify', 'phase', 'anim')
    'combat-logic' = @('combat', 'attack', 'combo', 'input', 'state', 'action', 'queue')
    'data-assets' = @('AttackData', 'asset', 'property', 'config', 'data', 'PDA', 'settings')
    'editor-ui' = @('editor', 'Slate', 'UI', 'panel', 'widget', 'customization', 'factory')
    testing = @('test', 'spec', 'assert', 'mock', 'fixture', 'automation', 'verify')
    documentation = @('docs', 'documentation', 'guide', 'README', 'tutorial', 'reference')
}

# Score accumulator
$scores = @{}

# ============================================================================
# FACTOR 1: File Pattern Analysis (35%)
# ============================================================================
if ($FilePath -and -not [string]::IsNullOrWhiteSpace($FilePath)) {
    try {
        $fileDetector = Join-Path $scriptDir "detect-mode.ps1"
        if (Test-Path $fileDetector) {
            $fileResultJson = & $fileDetector -FilePath $FilePath 2>$null

            if ($fileResultJson -and -not [string]::IsNullOrWhiteSpace($fileResultJson)) {
                try {
                    $fileResult = $fileResultJson | ConvertFrom-Json

                    if ($fileResult -and
                        $fileResult.PSObject.Properties['suggestedMode'] -and
                        $fileResult.PSObject.Properties['confidence'] -and
                        $fileResult.suggestedMode -ne "full") {

                        $scores['file'] = @{
                            mode = $fileResult.suggestedMode
                            confidence = [double]$fileResult.confidence
                            weight = $weights.file
                            weightedScore = [double]$fileResult.confidence * $weights.file
                            source = "file-pattern"
                        }
                    }
                } catch {
                    # JSON parse failed, skip factor
                }
            }
        }
    } catch {
        # File detection failed, continue
    }
}

# ============================================================================
# FACTOR 2: Conversation Topic Analysis (25%)
# ============================================================================
if ($ConversationText -and -not [string]::IsNullOrWhiteSpace($ConversationText)) {
    try {
        $convAnalyzer = Join-Path $scriptDir "conversation-analyzer.ps1"
        if (Test-Path $convAnalyzer) {
            $convResultJson = & $convAnalyzer -ConversationText $ConversationText 2>$null

            if ($convResultJson -and -not [string]::IsNullOrWhiteSpace($convResultJson)) {
                try {
                    $convResult = $convResultJson | ConvertFrom-Json

                    if ($convResult -and
                        $convResult.PSObject.Properties['suggestedMode'] -and
                        $convResult.PSObject.Properties['confidence'] -and
                        $convResult.suggestedMode -ne "full") {

                        $scores['conversation'] = @{
                            mode = $convResult.suggestedMode
                            confidence = [double]$convResult.confidence
                            weight = $weights.conversation
                            weightedScore = [double]$convResult.confidence * $weights.conversation
                            topicDistribution = $convResult.topicDistribution
                            taskComplexity = $convResult.taskComplexity
                            source = "conversation-topics"
                        }
                    }
                } catch {
                    # JSON parse failed, skip factor
                }
            }
        }
    } catch {
        # Conversation analysis failed, continue
    }
}

# ============================================================================
# FACTOR 3: Learned Pattern Matching (20%) - Bayesian + Correlation Boost
# ============================================================================
if ($FilePath -and -not [string]::IsNullOrWhiteSpace($FilePath)) {
    try {
        $learningTracker = Join-Path $scriptDir "learning-tracker.ps1"
        if (Test-Path $learningTracker) {
            $fileName = [System.IO.Path]::GetFileNameWithoutExtension($FilePath)

            if ($fileName -and -not [string]::IsNullOrWhiteSpace($fileName)) {
                $learnResultJson = & $learningTracker -Action query -Pattern $fileName 2>$null

                if ($learnResultJson -and -not [string]::IsNullOrWhiteSpace($learnResultJson)) {
                    try {
                        $learnResult = $learnResultJson | ConvertFrom-Json

                        if ($learnResult -and
                            $learnResult.PSObject.Properties['found'] -and
                            $learnResult.found -eq $true -and
                            $learnResult.PSObject.Properties['mode'] -and
                            $learnResult.PSObject.Properties['confidence']) {

                            $baseConfidence = [double]$learnResult.confidence
                            $correlationBoost = 0.0

                            # Apply correlation boost if conversation available
                            if ($ConversationText -and $scores.ContainsKey('conversation')) {
                                $convTopics = @()
                                if ($scores['conversation'].PSObject.Properties.Name -contains 'topicDistribution') {
                                    $convTopics = $scores['conversation'].topicDistribution.PSObject.Properties.Name | Select-Object -First 3
                                }

                                if ($convTopics.Count -gt 0) {
                                    try {
                                        $learningPath = Join-Path $scriptDir ".." | Join-Path -ChildPath ".context-learning.json"
                                        if (Test-Path $learningPath) {
                                            $learningData = Get-Content $learningPath | ConvertFrom-Json

                                            if ($learningData.PSObject.Properties.Name -contains 'correlations' -and
                                                $learningData.correlations.PSObject.Properties.Name -contains $fileName) {

                                                $patternCorr = $learningData.correlations.$fileName
                                                $totalCorr = 0.0
                                                $matchCount = 0

                                                foreach ($topic in $convTopics) {
                                                    if ($patternCorr.topics.PSObject.Properties.Name -contains $topic) {
                                                        $topicScore = [double]$patternCorr.topics.$topic
                                                        $totalCorr += $topicScore
                                                        $matchCount++
                                                    }
                                                }

                                                if ($matchCount -gt 0) {
                                                    $correlationBoost = $totalCorr / $matchCount
                                                }
                                            }
                                        }
                                    } catch {
                                        # Correlation query failed
                                    }
                                }
                            }

                            # Apply boost (20% max increase)
                            $boostedConfidence = $baseConfidence * (1.0 + $correlationBoost * 0.2)
                            $boostedConfidence = [Math]::Min(1.0, $boostedConfidence)

                            $scores['learning'] = @{
                                mode = $learnResult.mode
                                confidence = $boostedConfidence
                                baseConfidence = $baseConfidence
                                correlationBoost = $correlationBoost
                                weight = $weights.learning
                                weightedScore = $boostedConfidence * $weights.learning
                                source = "ml-learning"
                            }
                        }
                    } catch {
                        # JSON parse failed
                    }
                }
            }
        }
    } catch {
        # Learning query failed
    }
}

# ============================================================================
# FACTOR 4: Historical Success Rate (15%)
# ============================================================================
if ($FilePath -and -not [string]::IsNullOrWhiteSpace($FilePath)) {
    try {
        $learningPath = Join-Path $scriptDir ".." | Join-Path -ChildPath ".context-learning.json"
        if (Test-Path $learningPath) {
            $learningData = Get-Content $learningPath | ConvertFrom-Json
            $fileName = [System.IO.Path]::GetFileNameWithoutExtension($FilePath)

            # Check if we have a suggested mode from other factors
            $targetMode = $CurrentMode
            if ($scores.Count -gt 0) {
                # Use the highest-weighted factor's mode
                $topFactor = $scores.GetEnumerator() | Sort-Object { $_.Value.weightedScore } -Descending | Select-Object -First 1
                if ($topFactor) {
                    $targetMode = $topFactor.Value.mode
                }
            }

            # Check historical success for this file pattern + mode combo
            $historyConfidence = 0.5  # Neutral default

            foreach ($patternProp in $learningData.patterns.PSObject.Properties) {
                $pattern = $patternProp.Value

                if ($pattern.mode -eq $targetMode -and $fileName -match $patternProp.Name) {
                    $successCount = if ($pattern.bayesian.PSObject.Properties.Name -contains 'successCount') {
                        [int]$pattern.bayesian.successCount
                    } else { 0 }
                    $failureCount = if ($pattern.bayesian.PSObject.Properties.Name -contains 'failureCount') {
                        [int]$pattern.bayesian.failureCount
                    } else { 0 }
                    $total = $successCount + $failureCount

                    if ($total -gt 0) {
                        $historyConfidence = $successCount / $total

                        # Apply temporal decay
                        if ($pattern.PSObject.Properties.Name -contains 'temporal' -and
                            $pattern.temporal.PSObject.Properties.Name -contains 'decayFactor') {
                            $historyConfidence *= [double]$pattern.temporal.decayFactor
                        }

                        break
                    }
                }
            }

            # If no specific pattern match, use global accuracy
            if ($historyConfidence -eq 0.5 -and
                $learningData.PSObject.Properties.Name -contains 'globalStats' -and
                $learningData.globalStats.PSObject.Properties.Name -contains 'autoSwitchAccuracy') {
                $historyConfidence = [double]$learningData.globalStats.autoSwitchAccuracy
            }

            $scores['history'] = @{
                mode = $targetMode
                confidence = $historyConfidence
                weight = $weights.history
                weightedScore = $historyConfidence * $weights.history
                source = "historical-accuracy"
            }
        }
    } catch {
        # History lookup failed
    }
}

# ============================================================================
# FACTOR 5: Time-Based Patterns (5%)
# ============================================================================
try {
    $currentHour = (Get-Date).Hour
    $currentDay = (Get-Date).DayOfWeek

    # Work hours heuristic (higher confidence during active work times)
    $timeConfidence = 0.5  # Neutral default

    if ($currentHour -ge 9 -and $currentHour -le 17 -and
        $currentDay -ne 'Saturday' -and $currentDay -ne 'Sunday') {
        $timeConfidence = 0.7  # Higher confidence during work hours
    } elseif ($currentHour -ge 18 -and $currentHour -le 23) {
        $timeConfidence = 0.6  # Medium confidence evening
    } else {
        $timeConfidence = 0.4  # Lower confidence late night/early morning
    }

    # Time factor supports the current best mode (doesn't suggest its own mode)
    $targetMode = "full"
    if ($scores.Count -gt 0) {
        $topFactor = $scores.GetEnumerator() | Sort-Object { $_.Value.weightedScore } -Descending | Select-Object -First 1
        if ($topFactor) {
            $targetMode = $topFactor.Value.mode
        }
    }

    $scores['time'] = @{
        mode = $targetMode
        confidence = $timeConfidence
        weight = $weights.time
        weightedScore = $timeConfidence * $weights.time
        source = "time-heuristic"
        timeOfDay = $currentHour
        dayOfWeek = $currentDay.ToString()
    }
} catch {
    # Time analysis failed
}

# ============================================================================
# AGGREGATE SCORES & DETERMINE BEST MODE
# ============================================================================

if ($scores.Count -eq 0) {
    # No factors available - return default
    $result = @{
        suggestedMode = "full"
        confidence = 0.0
        confidenceLevel = "none"
        reason = "Insufficient data for mode suggestion"
        factors = @{}
        algorithm = "intelligent-v3"
    }
} else {
    # Aggregate by mode (weighted voting)
    $modeAggregates = @{}

    foreach ($factorName in $scores.Keys) {
        $factor = $scores[$factorName]
        $mode = $factor.mode

        if (-not $modeAggregates.ContainsKey($mode)) {
            $modeAggregates[$mode] = @{
                totalWeightedScore = 0.0
                factors = @()
                factorDetails = @{}
            }
        }

        $modeAggregates[$mode].totalWeightedScore += $factor.weightedScore
        $modeAggregates[$mode].factors += $factorName
        $modeAggregates[$mode].factorDetails[$factorName] = @{
            confidence = $factor.confidence
            weight = $factor.weight
            weightedScore = $factor.weightedScore
        }
    }

    # Find best mode
    $bestMode = $modeAggregates.GetEnumerator() | Sort-Object { $_.Value.totalWeightedScore } -Descending | Select-Object -First 1

    $finalConfidence = $bestMode.Value.totalWeightedScore
    $confidenceLevel = if ($finalConfidence -ge 0.70) { "high" }
                      elseif ($finalConfidence -ge 0.50) { "medium" }
                      else { "low" }

    # Build detailed reason
    $contributingFactors = $bestMode.Value.factors -join ", "
    $reason = "Multi-factor analysis ($contributingFactors) suggests $($bestMode.Key)"

    # Determine if should switch from current mode
    $shouldSwitch = ($bestMode.Key -ne $CurrentMode -and $finalConfidence -ge 0.50)

    $result = @{
        suggestedMode = $bestMode.Key
        confidence = [Math]::Round($finalConfidence, 3)
        confidenceLevel = $confidenceLevel
        reason = $reason
        shouldSwitch = $shouldSwitch
        currentMode = $CurrentMode
        factors = $scores
        modeAggregates = $modeAggregates
        algorithm = "intelligent-v3"
        weights = $weights
    }
}

# ============================================================================
# OUTPUT (JSON format)
# ============================================================================

if ($ShowDetails) {
    Write-Host ""
    Write-Host "Intelligent Mode Detection v3.0" -ForegroundColor Cyan
    Write-Host "================================" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "File: $FilePath" -ForegroundColor Gray
    Write-Host "Current Mode: $CurrentMode" -ForegroundColor Gray
    Write-Host ""
    Write-Host "RESULT:" -ForegroundColor Yellow
    Write-Host "  Suggested Mode: $($result.suggestedMode)" -ForegroundColor Green
    Write-Host "  Confidence: $($result.confidence) ($($result.confidenceLevel))" -ForegroundColor $(if ($result.confidenceLevel -eq 'high') { 'Green' } elseif ($result.confidenceLevel -eq 'medium') { 'Yellow' } else { 'Red' })
    Write-Host "  Should Switch: $($result.shouldSwitch)" -ForegroundColor $(if ($result.shouldSwitch) { 'Green' } else { 'Gray' })
    Write-Host ""

    if ($scores.Count -gt 0) {
        Write-Host "FACTOR BREAKDOWN:" -ForegroundColor Yellow
        foreach ($factorName in @('file', 'conversation', 'learning', 'history', 'time')) {
            if ($scores.ContainsKey($factorName)) {
                $factor = $scores[$factorName]
                $percentage = [Math]::Round($factor.confidence * 100, 1)
                $weighted = [Math]::Round($factor.weightedScore, 3)
                Write-Host "  [$factorName]".PadRight(18) -NoNewline -ForegroundColor Cyan
                Write-Host "$($factor.mode)".PadRight(15) -NoNewline -ForegroundColor White
                Write-Host "conf: $percentage% ".PadRight(15) -NoNewline -ForegroundColor Gray
                Write-Host "weighted: $weighted" -ForegroundColor DarkGray
            }
        }
        Write-Host ""
        Write-Host "  Total Weighted Score: $($result.confidence)" -ForegroundColor Green
        Write-Host ""
    }

    Write-Host "Reason: $($result.reason)" -ForegroundColor Gray
    Write-Host ""
}

# Output JSON for programmatic consumption
$result | ConvertTo-Json -Depth 10 -Compress