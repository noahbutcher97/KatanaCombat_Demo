# Holistic Mode Detection
# Combines file-based, conversation-based, and learned patterns
# Optimized for <50ms total latency

param(
    [Parameter(Mandatory=$false)]
    [string]$FilePath = "",

    [Parameter(Mandatory=$false)]
    [string]$ConversationText = "",

    [Parameter(Mandatory=$false)]
    [switch]$ShowDetails
)

$scriptDir = $PSScriptRoot

# Multi-factor scoring weights
$weights = @{
    file = 0.50              # 50% weight on file patterns
    conversation = 0.35      # 35% weight on conversation topics
    learning = 0.15          # 15% weight on learned patterns
}

$scores = @{}

# Factor 1: File-Based Detection (inline for performance)
if ($FilePath -and -not [string]::IsNullOrWhiteSpace($FilePath)) {
    try {
        $fileDetector = Join-Path $scriptDir "detect-mode.ps1"
        if (Test-Path $fileDetector) {
            # Capture output from detect-mode script
            $fileResultJson = & $fileDetector -FilePath $FilePath 2>$null

            if ($fileResultJson -and -not [string]::IsNullOrWhiteSpace($fileResultJson)) {
                try {
                    $fileResult = $fileResultJson | ConvertFrom-Json

                    # Validate result has required properties
                    if ($fileResult -and
                        $fileResult.PSObject.Properties['suggestedMode'] -and
                        $fileResult.PSObject.Properties['confidence'] -and
                        $fileResult.suggestedMode -ne "full") {

                        $scores['file'] = @{
                            mode = $fileResult.suggestedMode
                            confidence = [double]$fileResult.confidence
                            weight = $weights.file
                            weightedScore = [double]$fileResult.confidence * $weights.file
                        }
                    }
                } catch {
                    # JSON parse failed, skip file factor
                }
            }
        }
    } catch {
        # File detection failed, continue gracefully
    }
}

# Factor 2: Conversation-Based Detection (inline for performance)
if ($ConversationText -and -not [string]::IsNullOrWhiteSpace($ConversationText)) {
    try {
        $convAnalyzer = Join-Path $scriptDir "conversation-analyzer.ps1"
        if (Test-Path $convAnalyzer) {
            $convResultJson = & $convAnalyzer -ConversationText $ConversationText 2>$null

            if ($convResultJson -and -not [string]::IsNullOrWhiteSpace($convResultJson)) {
                try {
                    $convResult = $convResultJson | ConvertFrom-Json

                    # Validate result has required properties
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
                        }
                    }
                } catch {
                    # JSON parse failed, skip conversation factor
                }
            }
        }
    } catch {
        # Conversation analysis failed, continue gracefully
    }
}

# Factor 3: Learned Patterns with Correlation Boost (inline for performance)
if ($FilePath -and -not [string]::IsNullOrWhiteSpace($FilePath)) {
    try {
        $learningTracker = Join-Path $scriptDir "learning-tracker.ps1"
        if (Test-Path $learningTracker) {
            # Extract pattern from file path (e.g., "AnimNotify", "CombatComponent")
            $fileName = [System.IO.Path]::GetFileNameWithoutExtension($FilePath)

            if ($fileName -and -not [string]::IsNullOrWhiteSpace($fileName)) {
                $learnResultJson = & $learningTracker -Action query -Pattern $fileName 2>$null

                if ($learnResultJson -and -not [string]::IsNullOrWhiteSpace($learnResultJson)) {
                    try {
                        $learnResult = $learnResultJson | ConvertFrom-Json

                        # Validate result has required properties
                        if ($learnResult -and
                            $learnResult.PSObject.Properties['found'] -and
                            $learnResult.found -eq $true -and
                            $learnResult.PSObject.Properties['mode'] -and
                            $learnResult.PSObject.Properties['confidence']) {

                            $baseConfidence = [double]$learnResult.confidence
                            $correlationBoost = 0.0

                            # Apply correlation boost if conversation topics available
                            if ($ConversationText -and $scores.ContainsKey('conversation')) {
                                # Extract topics from conversation result
                                $convTopics = @()
                                try {
                                    $convAnalyzer = Join-Path $scriptDir "conversation-analyzer.ps1"
                                    if (Test-Path $convAnalyzer) {
                                        $convResultJson = & $convAnalyzer -ConversationText $ConversationText 2>$null
                                        $convResult = $convResultJson | ConvertFrom-Json

                                        # Get dominant topics from conversation
                                        if ($convResult.PSObject.Properties['topicDistribution']) {
                                            $convTopics = $convResult.topicDistribution.PSObject.Properties.Name | Select-Object -First 3
                                        }
                                    }
                                } catch {
                                    # Topic extraction failed, continue without boost
                                }

                                # Query correlations for this pattern
                                if ($convTopics.Count -gt 0) {
                                    try {
                                        # Manual correlation calculation (avoid double-call overhead)
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
                                                        # Skip PSObject internal properties
                                                        if ($topic -notin @('Count', 'IsReadOnly', 'IsFixedSize', 'IsSynchronized', 'SyncRoot', 'Keys', 'Values')) {
                                                            $totalCorr += $topicScore
                                                            $matchCount++
                                                        }
                                                    }
                                                }

                                                if ($matchCount -gt 0) {
                                                    $correlationBoost = $totalCorr / $matchCount
                                                }
                                            }
                                        }
                                    } catch {
                                        # Correlation query failed, continue without boost
                                    }
                                }
                            }

                            # Apply boost (20% max increase from correlation)
                            $boostedConfidence = $baseConfidence * (1.0 + $correlationBoost * 0.2)
                            $boostedConfidence = [Math]::Min(1.0, $boostedConfidence)

                            $scores['learning'] = @{
                                mode = $learnResult.mode
                                confidence = $boostedConfidence
                                baseConfidence = $baseConfidence
                                correlationBoost = $correlationBoost
                                weight = $weights.learning
                                weightedScore = $boostedConfidence * $weights.learning
                            }
                        }
                    } catch {
                        # JSON parse failed, skip learning factor
                    }
                }
            }
        }
    } catch {
        # Learning query failed, continue gracefully
    }
}

# Aggregate scores
if ($scores.Count -eq 0) {
    # No factors available
    $result = @{
        suggestedMode = "full"
        confidence = 0.0
        confidenceLevel = "none"
        reason = "Insufficient data for mode suggestion"
        factors = @{}
    }
} else {
    # Aggregate by mode
    $modeAggregates = @{}

    foreach ($factorName in $scores.Keys) {
        $factor = $scores[$factorName]
        $mode = $factor.mode

        if (-not $modeAggregates.ContainsKey($mode)) {
            $modeAggregates[$mode] = @{
                totalWeightedScore = 0.0
                factors = @()
            }
        }

        $modeAggregates[$mode].totalWeightedScore += $factor.weightedScore
        $modeAggregates[$mode].factors += $factorName
    }

    # Find best mode
    $bestMode = $modeAggregates.GetEnumerator() | Sort-Object { $_.Value.totalWeightedScore } -Descending | Select-Object -First 1

    $finalConfidence = $bestMode.Value.totalWeightedScore
    $confidenceLevel = if ($finalConfidence -ge 0.70) { "high" }
                      elseif ($finalConfidence -ge 0.50) { "medium" }
                      else { "low" }

    # Build reason
    $contributingFactors = $bestMode.Value.factors -join ", "
    $reason = "Multi-factor analysis ($contributingFactors) suggests $($bestMode.Key)"

    $result = @{
        suggestedMode = $bestMode.Key
        confidence = [Math]::Round($finalConfidence, 3)
        confidenceLevel = $confidenceLevel
        reason = $reason
        factors = $scores
        aggregates = $modeAggregates
    }
}

# Output
if ($ShowDetails) {
    Write-Host ""
    Write-Host "=== Holistic Mode Detection ===" -ForegroundColor Cyan
    Write-Host ""

    if ($result.suggestedMode -ne "full") {
        Write-Host "Suggested Mode: $($result.suggestedMode)" -ForegroundColor Green
        Write-Host "Confidence: $([Math]::Round($result.confidence * 100, 1))% ($($result.confidenceLevel))" -ForegroundColor Yellow
        Write-Host "Reason: $($result.reason)" -ForegroundColor Gray
        Write-Host ""

        Write-Host "Contributing Factors:" -ForegroundColor Yellow
        foreach ($factorName in $result.factors.Keys) {
            $factor = $result.factors[$factorName]
            $pct = [Math]::Round($factor.confidence * 100, 1)
            $weighted = [Math]::Round($factor.weightedScore * 100, 1)

            $factorLine = "  [$factorName] $($factor.mode) - Confidence: $pct% (weighted: $weighted%)"

            # Show correlation boost for learning factor
            if ($factorName -eq "learning" -and $factor.PSObject.Properties.Name -contains 'correlationBoost') {
                $boost = $factor.correlationBoost
                if ($boost -gt 0) {
                    $base = [Math]::Round($factor.baseConfidence * 100, 1)
                    $boostPct = [Math]::Round($boost * 100, 1)
                    $factorLine += " [base: $base%, boost: +$boostPct%]"
                }
            }

            Write-Host $factorLine -ForegroundColor Gray
        }
        Write-Host ""

        if ($result.aggregates.Count -gt 1) {
            Write-Host "All Mode Scores:" -ForegroundColor Yellow
            $sortedModes = $result.aggregates.GetEnumerator() | Sort-Object { $_.Value.totalWeightedScore } -Descending
            foreach ($mode in $sortedModes) {
                $pct = [Math]::Round($mode.Value.totalWeightedScore * 100, 1)
                $bar = "#" * [Math]::Min([Math]::Ceiling($pct / 5), 20)
                $factors = $mode.Value.factors -join ", "
                Write-Host "  $($mode.Key.PadRight(15)): $bar $pct% ($factors)" -ForegroundColor Gray
            }
            Write-Host ""
        }
    } else {
        Write-Host "No mode suggestion available" -ForegroundColor Yellow
        Write-Host "Using 'full' mode as fallback" -ForegroundColor Gray
    }
    Write-Host ""
} else {
    $result | ConvertTo-Json -Depth 10
}
