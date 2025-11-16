# Self-Learning Context Pattern Tracker v2.1
# ML-Inspired: Bayesian + Temporal decay + Feedback + Correlation matrix
# Lightweight design: <30ms overhead (Phase 2a)

param(
    [Parameter(Mandatory=$true)]
    [ValidateSet('record', 'query', 'status', 'reset', 'feedback', 'correlate')]
    [string]$Action,

    [Parameter(Mandatory=$false)]
    [string]$Pattern = "",

    [Parameter(Mandatory=$false)]
    [string]$Mode = "",

    [Parameter(Mandatory=$false)]
    [double]$FileConfidence = 0.0,

    [Parameter(Mandatory=$false)]
    [double]$ConversationConfidence = 0.0,

    # Feedback parameters
    [Parameter(Mandatory=$false)]
    [bool]$Success = $true,

    [Parameter(Mandatory=$false)]
    [string]$ActualMode = "",

    # Correlation parameters
    [Parameter(Mandatory=$false)]
    [string[]]$Topics = @()
)

$learningFile = ".claude/.context-learning.json"
$lockFile = ".claude/.context-learning.lock"

# Helper function to safely load and normalize database structure
function Load-LearningDatabase {
    param([string]$FilePath)

    $learningJson = Get-Content $FilePath -Raw
    $data = $learningJson | ConvertFrom-Json

    # Normalize all nested structures to PSCustomObjects
    # This fixes PowerShell ConvertFrom-Json hashtable deserialization issues
    # Safely extract global stats with defaults for new fields
    $stats = $data.globalStats
    $implicitSuccess = if ($stats.PSObject.Properties.Name -contains 'implicitSuccess') { [int]$stats.implicitSuccess } else { 0 }
    $explicitSuccess = if ($stats.PSObject.Properties.Name -contains 'explicitSuccess') { [int]$stats.explicitSuccess } else { 0 }
    $explicitFailure = if ($stats.PSObject.Properties.Name -contains 'explicitFailure') { [int]$stats.explicitFailure } else { 0 }

    $normalized = [PSCustomObject]@{
        version = $data.version
        patterns = [PSCustomObject]@{}
        correlations = [PSCustomObject]@{}
        globalStats = [PSCustomObject]@{
            totalSwitches = $data.globalStats.totalSwitches
            autoSwitchAccuracy = $data.globalStats.autoSwitchAccuracy
            totalSuccess = $data.globalStats.totalSuccess
            totalFailure = $data.globalStats.totalFailure
            implicitSuccess = $implicitSuccess
            explicitSuccess = $explicitSuccess
            explicitFailure = $explicitFailure
            lastUpdated = $data.globalStats.lastUpdated
        }
    }

    # Normalize patterns
    if ($data.PSObject.Properties.Name -contains 'patterns') {
        foreach ($patternProp in $data.patterns.PSObject.Properties) {
            $p = $patternProp.Value

            # Safely access nested properties with defaults
            $bayesianData = if ($p.PSObject.Properties.Name -contains 'bayesian') { $p.bayesian } else { $null }
            $temporalData = if ($p.PSObject.Properties.Name -contains 'temporal') { $p.temporal } else { $null }
            $featuresData = if ($p.PSObject.Properties.Name -contains 'features') { $p.features } else { $null }

            # Safely extract counts from bayesian (handle both hashtable and PSCustomObject)
            $successCount = 0
            $failureCount = 0
            $lastUpdated = (Get-Date -Format "yyyy-MM-dd HH:mm:ss")
            if ($bayesianData) {
                try {
                    if ($bayesianData -is [hashtable]) {
                        $successCount = if ($bayesianData.ContainsKey('successCount')) { [int]$bayesianData['successCount'] } else { 0 }
                        $failureCount = if ($bayesianData.ContainsKey('failureCount')) { [int]$bayesianData['failureCount'] } else { 0 }
                        $lastUpdated = if ($bayesianData.ContainsKey('lastUpdated')) { $bayesianData['lastUpdated'] } else { $lastUpdated }
                    } else {
                        $successCount = if ($bayesianData.PSObject.Properties.Name -contains 'successCount') { [int]$bayesianData.successCount } else { 0 }
                        $failureCount = if ($bayesianData.PSObject.Properties.Name -contains 'failureCount') { [int]$bayesianData.failureCount } else { 0 }
                        $lastUpdated = if ($bayesianData.PSObject.Properties.Name -contains 'lastUpdated') { $bayesianData.lastUpdated } else { $lastUpdated }
                    }
                } catch {
                    # Fallback to defaults
                    $successCount = 0
                    $failureCount = 0
                }
            }

            # Safely extract temporal data
            $lastUsed = (Get-Date -Format "yyyy-MM-dd HH:mm:ss")
            $decayFactor = 1.0
            if ($temporalData) {
                try {
                    if ($temporalData -is [hashtable]) {
                        $lastUsed = if ($temporalData.ContainsKey('lastUsed')) { $temporalData['lastUsed'] } else { $lastUsed }
                        $decayFactor = if ($temporalData.ContainsKey('decayFactor')) { [double]$temporalData['decayFactor'] } else { 1.0 }
                    } else {
                        $lastUsed = if ($temporalData.PSObject.Properties.Name -contains 'lastUsed') { $temporalData.lastUsed } else { $lastUsed }
                        $decayFactor = if ($temporalData.PSObject.Properties.Name -contains 'decayFactor') { [double]$temporalData.decayFactor } else { 1.0 }
                    }
                } catch {
                    $lastUsed = (Get-Date -Format "yyyy-MM-dd HH:mm:ss")
                    $decayFactor = 1.0
                }
            }

            # Safely extract features data
            $avgFileConf = 0.0
            $avgConvConf = 0.0
            $weightsData = $null
            if ($featuresData) {
                try {
                    if ($featuresData -is [hashtable]) {
                        $avgFileConf = if ($featuresData.ContainsKey('avgFileConfidence')) { [double]$featuresData['avgFileConfidence'] } else { 0.0 }
                        $avgConvConf = if ($featuresData.ContainsKey('avgConversationConfidence')) { [double]$featuresData['avgConversationConfidence'] } else { 0.0 }
                        if ($featuresData.ContainsKey('weights')) {
                            $w = $featuresData['weights']
                            if ($w -is [hashtable]) {
                                $weightsData = [PSCustomObject]@{
                                    file = if ($w.ContainsKey('file')) { [double]$w['file'] } else { 0.6 }
                                    conversation = if ($w.ContainsKey('conversation')) { [double]$w['conversation'] } else { 0.4 }
                                }
                            }
                        }
                    } else {
                        $avgFileConf = if ($featuresData.PSObject.Properties.Name -contains 'avgFileConfidence') { [double]$featuresData.avgFileConfidence } else { 0.0 }
                        $avgConvConf = if ($featuresData.PSObject.Properties.Name -contains 'avgConversationConfidence') { [double]$featuresData.avgConversationConfidence } else { 0.0 }
                        if ($featuresData.PSObject.Properties.Name -contains 'weights') {
                            $weightsData = [PSCustomObject]@{
                                file = [double]$featuresData.weights.file
                                conversation = [double]$featuresData.weights.conversation
                            }
                        }
                    }
                } catch {
                    $avgFileConf = 0.0
                    $avgConvConf = 0.0
                }
            }

            $normalized.patterns | Add-Member -NotePropertyName $patternProp.Name -NotePropertyValue ([PSCustomObject]@{
                mode = if ($p.PSObject.Properties.Name -contains 'mode') { $p.mode } else { "unknown" }
                bayesian = [PSCustomObject]@{
                    successCount = $successCount
                    failureCount = $failureCount
                    lastUpdated = $lastUpdated
                }
                temporal = [PSCustomObject]@{
                    lastUsed = $lastUsed
                    decayFactor = $decayFactor
                }
                features = [PSCustomObject]@{
                    avgFileConfidence = $avgFileConf
                    avgConversationConfidence = $avgConvConf
                    weights = $weightsData
                }
            })
        }
    }

    # Normalize correlations
    if ($data.PSObject.Properties.Name -contains 'correlations') {
        foreach ($corrProp in $data.correlations.PSObject.Properties) {
            $c = $corrProp.Value
            $normalizedCorr = [PSCustomObject]@{
                topics = [PSCustomObject]@{}
                modes = [PSCustomObject]@{}
            }

            # Copy topics
            if ($c.PSObject.Properties.Name -contains 'topics') {
                foreach ($topicProp in $c.topics.PSObject.Properties) {
                    $normalizedCorr.topics | Add-Member -NotePropertyName $topicProp.Name -NotePropertyValue ([double]$topicProp.Value)
                }
            }

            # Copy modes
            if ($c.PSObject.Properties.Name -contains 'modes') {
                foreach ($modeProp in $c.modes.PSObject.Properties) {
                    $normalizedCorr.modes | Add-Member -NotePropertyName $modeProp.Name -NotePropertyValue ([double]$modeProp.Value)
                }
            }

            $normalized.correlations | Add-Member -NotePropertyName $corrProp.Name -NotePropertyValue $normalizedCorr
        }
    }

    return $normalized
}

# File locking function
function Get-FileLock {
    $maxRetries = 50
    $retryDelayMs = 20
    $retries = 0

    while ($retries -lt $maxRetries) {
        try {
            # Try to create lock file (exclusive access)
            $null = New-Item -ItemType File -Path $lockFile -ErrorAction Stop
            return $true
        } catch {
            # Lock exists, wait and retry
            Start-Sleep -Milliseconds $retryDelayMs
            $retries++
        }
    }

    # Timeout - force remove stale lock
    if (Test-Path $lockFile) {
        Remove-Item $lockFile -Force -ErrorAction SilentlyContinue
    }
    return $false
}

function Release-FileLock {
    if (Test-Path $lockFile) {
        Remove-Item $lockFile -Force -ErrorAction SilentlyContinue
    }
}

# Acquire lock
if (-not (Get-FileLock)) {
    Write-Host "[ERROR] Failed to acquire file lock after retries" -ForegroundColor Red
    throw "Failed to acquire file lock"
}

try {
    # Initialize if doesn't exist
    if (-not (Test-Path $learningFile)) {
        $initialData = [PSCustomObject]@{
            version = "2.1"
            patterns = [PSCustomObject]@{}
            correlations = [PSCustomObject]@{}
            globalStats = [PSCustomObject]@{
                totalSwitches = 0
                autoSwitchAccuracy = 0.0
                totalSuccess = 0
                totalFailure = 0
                implicitSuccess = 0
                explicitSuccess = 0
                explicitFailure = 0
                lastUpdated = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
            }
        }
        $initialData | ConvertTo-Json -Depth 10 | Set-Content $learningFile
    }

    # Load learning data with normalization
    $learning = Load-LearningDatabase -FilePath $learningFile
} catch {
    Release-FileLock
    throw
}

# Migrate v1.0 to v2.0 if needed
if ($learning.version -eq "1.0") {
    Write-Host "[MIGRATION] Upgrading learning data from v1.0 to v2.1..." -ForegroundColor Yellow

    $newPatterns = [PSCustomObject]@{}
    foreach ($patternProp in $learning.patterns.PSObject.Properties) {
        $old = $patternProp.Value
        $newPatterns | Add-Member -NotePropertyName $patternProp.Name -NotePropertyValue ([PSCustomObject]@{
            mode = $old.mode
            bayesian = [PSCustomObject]@{
                successCount = [int]$old.count
                failureCount = 0
                lastUpdated = $old.lastSeen
            }
            temporal = [PSCustomObject]@{
                lastUsed = $old.lastSeen
                decayFactor = 1.0
            }
            features = [PSCustomObject]@{
                avgFileConfidence = if ($old.PSObject.Properties.Name -contains 'avgFileConfidence') { $old.avgFileConfidence } else { 0.0 }
                avgConversationConfidence = if ($old.PSObject.Properties.Name -contains 'avgConversationConfidence') { $old.avgConversationConfidence } else { 0.0 }
            }
        })
    }

    $learning = [PSCustomObject]@{
        version = "2.1"
        patterns = $newPatterns
        correlations = [PSCustomObject]@{}
        globalStats = [PSCustomObject]@{
            totalSwitches = $learning.statistics.totalRecords
            autoSwitchAccuracy = 0.0
            totalSuccess = $learning.statistics.totalRecords
            totalFailure = 0
            lastUpdated = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
        }
    }

    $learning | ConvertTo-Json -Depth 10 | Set-Content $learningFile
    Write-Host "[OK] Migration complete (v2.1)" -ForegroundColor Green
}

# Migrate v2.0 to v2.1 if needed
if ($learning.version -eq "2.0") {
    Write-Host "[MIGRATION] Upgrading learning data from v2.0 to v2.1..." -ForegroundColor Yellow

    $learning | Add-Member -NotePropertyName "correlations" -NotePropertyValue ([PSCustomObject]@{}) -Force
    $learning.version = "2.1"

    $learning | ConvertTo-Json -Depth 10 | Set-Content $learningFile
    Write-Host "[OK] Migration complete (v2.1)" -ForegroundColor Green
}

function Record-Pattern {
    param([string]$Pattern, [string]$Mode, [double]$FileConf, [double]$ConvConf)

    $now = Get-Date -Format "yyyy-MM-dd HH:mm:ss"

    if (-not ($learning.patterns.PSObject.Properties.Name -contains $Pattern)) {
        # New pattern - initialize with success assumption
        $learning.patterns | Add-Member -NotePropertyName $Pattern -NotePropertyValue ([PSCustomObject]@{
            mode = $Mode
            bayesian = [PSCustomObject]@{
                successCount = 1
                failureCount = 0
                lastUpdated = $now
            }
            temporal = [PSCustomObject]@{
                lastUsed = $now
                decayFactor = 1.0
            }
            features = [PSCustomObject]@{
                avgFileConfidence = $FileConf
                avgConversationConfidence = $ConvConf
                weights = [PSCustomObject]@{
                    file = 0.6
                    conversation = 0.4
                }
            }
        })
    } else {
        # Update existing pattern
        $existing = $learning.patterns.$Pattern

        # Ensure bayesian structure exists
        if (-not ($existing.PSObject.Properties.Name -contains 'bayesian')) {
            $existing | Add-Member -NotePropertyName 'bayesian' -NotePropertyValue ([PSCustomObject]@{
                successCount = 0
                failureCount = 0
                lastUpdated = $now
            })
        }
        if (-not ($existing.bayesian.PSObject.Properties.Name -contains 'successCount')) {
            $existing.bayesian | Add-Member -NotePropertyName 'successCount' -NotePropertyValue 0 -Force
        }
        if (-not ($existing.bayesian.PSObject.Properties.Name -contains 'failureCount')) {
            $existing.bayesian | Add-Member -NotePropertyName 'failureCount' -NotePropertyValue 0 -Force
        }

        # Bayesian update (assume success if mode matches)
        $existingMode = if ($existing.PSObject.Properties.Name -contains 'mode') { $existing.mode } else { "" }
        if ($existingMode -eq $Mode) {
            $existing.bayesian.successCount++
        } else {
            # Mode changed - could indicate failure or new preference
            # Treat as new evidence for the new mode
            if ($existing.PSObject.Properties.Name -contains 'mode') {
                $existing.mode = $Mode
            } else {
                $existing | Add-Member -NotePropertyName 'mode' -NotePropertyValue $Mode -Force
            }
            $existing.bayesian.successCount++
        }

        # Ensure temporal structure exists
        if (-not ($existing.PSObject.Properties.Name -contains 'temporal')) {
            $existing | Add-Member -NotePropertyName 'temporal' -NotePropertyValue ([PSCustomObject]@{
                lastUsed = $now
                decayFactor = 1.0
            })
        }
        if (-not ($existing.temporal.PSObject.Properties.Name -contains 'lastUsed')) {
            $existing.temporal | Add-Member -NotePropertyName 'lastUsed' -NotePropertyValue $now -Force
        } else {
            $existing.temporal.lastUsed = $now
        }
        if (-not ($existing.temporal.PSObject.Properties.Name -contains 'decayFactor')) {
            $existing.temporal | Add-Member -NotePropertyName 'decayFactor' -NotePropertyValue 1.0 -Force
        } else {
            $existing.temporal.decayFactor = 1.0
        }

        # Update bayesian lastUpdated
        if (-not ($existing.bayesian.PSObject.Properties.Name -contains 'lastUpdated')) {
            $existing.bayesian | Add-Member -NotePropertyName 'lastUpdated' -NotePropertyValue $now -Force
        } else {
            $existing.bayesian.lastUpdated = $now
        }

        # Ensure features structure exists
        if (-not ($existing.PSObject.Properties.Name -contains 'features')) {
            $existing | Add-Member -NotePropertyName 'features' -NotePropertyValue ([PSCustomObject]@{
                avgFileConfidence = $FileConf
                avgConversationConfidence = $ConvConf
            })
        }
        if (-not ($existing.features.PSObject.Properties.Name -contains 'avgFileConfidence')) {
            $existing.features | Add-Member -NotePropertyName 'avgFileConfidence' -NotePropertyValue 0.0 -Force
        }
        if (-not ($existing.features.PSObject.Properties.Name -contains 'avgConversationConfidence')) {
            $existing.features | Add-Member -NotePropertyName 'avgConversationConfidence' -NotePropertyValue 0.0 -Force
        }

        # Feature update (exponential moving average)
        $alpha = 0.3  # Learning rate
        $existing.features.avgFileConfidence =
            $existing.features.avgFileConfidence * (1.0 - $alpha) + $FileConf * $alpha
        $existing.features.avgConversationConfidence =
            $existing.features.avgConversationConfidence * (1.0 - $alpha) + $ConvConf * $alpha

        # Ensure weights exist (for migration from older versions)
        if (-not ($existing.features.PSObject.Properties.Name -contains 'weights')) {
            $existing.features | Add-Member -NotePropertyName "weights" -NotePropertyValue ([PSCustomObject]@{
                file = 0.6
                conversation = 0.4
            })
        }
    }

    # Global stats - track implicit success (pattern recording = implicit acceptance)
    $learning.globalStats.totalSwitches++
    $learning.globalStats.implicitSuccess++
    $learning.globalStats.totalSuccess++

    # Calculate accuracy: totalSuccess / totalSwitches
    if ($learning.globalStats.totalSwitches -gt 0) {
        $learning.globalStats.autoSwitchAccuracy = [Math]::Round(
            $learning.globalStats.totalSuccess / $learning.globalStats.totalSwitches,
            3
        )
    }

    $learning.globalStats.lastUpdated = $now

    # Save (atomic write)
    $learning | ConvertTo-Json -Depth 10 | Set-Content $learningFile

    @{ success = $true } | ConvertTo-Json
}

function Calculate-BayesianConfidence {
    param([object]$Pattern)

    # Safely extract counts with defaults
    $successCount = 0
    $failureCount = 0
    if ($Pattern.PSObject.Properties.Name -contains 'bayesian') {
        if ($Pattern.bayesian.PSObject.Properties.Name -contains 'successCount') {
            $successCount = [int]$Pattern.bayesian.successCount
        }
        if ($Pattern.bayesian.PSObject.Properties.Name -contains 'failureCount') {
            $failureCount = [int]$Pattern.bayesian.failureCount
        }
    }

    # Beta distribution: α = successCount + 1, β = failureCount + 1
    $alpha = $successCount + 1
    $beta = $failureCount + 1

    # Mean of Beta distribution
    $bayesianMean = $alpha / ($alpha + $beta)

    # Uncertainty penalty (fewer samples = lower confidence)
    $sampleSize = $successCount + $failureCount
    $uncertaintyPenalty = [Math]::Min(1.0, $sampleSize / 10.0)

    # Adjusted confidence
    $baseConfidence = $bayesianMean * $uncertaintyPenalty

    # Temporal decay (exponential)
    try {
        $lastUsed = [DateTime]::ParseExact($Pattern.temporal.lastUsed, "yyyy-MM-dd HH:mm:ss", $null)
        $daysSinceUse = ((Get-Date) - $lastUsed).TotalDays
        $decayRate = 0.95  # 5% decay per day
        $temporalDecay = [Math]::Pow($decayRate, $daysSinceUse)
    } catch {
        # Fallback if date parsing fails
        $temporalDecay = 1.0
    }

    # Final confidence with temporal decay
    $finalConfidence = $baseConfidence * $temporalDecay

    return @{
        confidence = [Math]::Round($finalConfidence, 3)
        bayesianMean = [Math]::Round($bayesianMean, 3)
        uncertaintyPenalty = [Math]::Round($uncertaintyPenalty, 3)
        temporalDecay = [Math]::Round($temporalDecay, 3)
        sampleSize = $sampleSize
    }
}

function Query-Pattern {
    param([string]$Pattern)

    # Exact match
    if ($learning.patterns.PSObject.Properties.Name -contains $Pattern) {
        $match = $learning.patterns.$Pattern
        $bayesianResult = Calculate-BayesianConfidence -Pattern $match

        # Safely extract counts
        $successCount = 0
        $failureCount = 0
        if ($match.PSObject.Properties.Name -contains 'bayesian') {
            if ($match.bayesian.PSObject.Properties.Name -contains 'successCount') {
                $successCount = [int]$match.bayesian.successCount
            }
            if ($match.bayesian.PSObject.Properties.Name -contains 'failureCount') {
                $failureCount = [int]$match.bayesian.failureCount
            }
        }

        @{
            found = $true
            mode = $match.mode
            confidence = $bayesianResult.confidence
            bayesian = @{
                successCount = $successCount
                failureCount = $failureCount
                mean = $bayesianResult.bayesianMean
                uncertainty = $bayesianResult.uncertaintyPenalty
                sampleSize = $bayesianResult.sampleSize
            }
            temporal = @{
                decay = $bayesianResult.temporalDecay
                lastUsed = $match.temporal.lastUsed
            }
            features = $match.features
        } | ConvertTo-Json -Depth 10
        return
    }

    # Partial match (for fuzzy queries)
    $partialMatches = $learning.patterns.PSObject.Properties | Where-Object {
        $Pattern -match [regex]::Escape($_.Name) -or $_.Name -match [regex]::Escape($Pattern)
    }

    if ($partialMatches) {
        # Find best match based on Bayesian confidence
        $bestMatch = $null
        $bestConfidence = 0.0

        foreach ($matchProp in $partialMatches) {
            $bayesianResult = Calculate-BayesianConfidence -Pattern $matchProp.Value
            if ($bayesianResult.confidence -gt $bestConfidence) {
                $bestConfidence = $bayesianResult.confidence
                $bestMatch = $matchProp
            }
        }

        if ($bestMatch) {
            $bayesianResult = Calculate-BayesianConfidence -Pattern $bestMatch.Value

            # Safely extract counts
            $successCount = 0
            $failureCount = 0
            if ($bestMatch.Value.PSObject.Properties.Name -contains 'bayesian') {
                if ($bestMatch.Value.bayesian.PSObject.Properties.Name -contains 'successCount') {
                    $successCount = [int]$bestMatch.Value.bayesian.successCount
                }
                if ($bestMatch.Value.bayesian.PSObject.Properties.Name -contains 'failureCount') {
                    $failureCount = [int]$bestMatch.Value.bayesian.failureCount
                }
            }

            # Partial match penalty (70% of exact match confidence)
            $adjustedConfidence = $bayesianResult.confidence * 0.7

            @{
                found = $true
                mode = $bestMatch.Value.mode
                confidence = [Math]::Round($adjustedConfidence, 3)
                partial = $true
                matchedPattern = $bestMatch.Name
                bayesian = @{
                    successCount = $successCount
                    failureCount = $failureCount
                    mean = $bayesianResult.bayesianMean
                    uncertainty = $bayesianResult.uncertaintyPenalty
                    sampleSize = $bayesianResult.sampleSize
                }
                temporal = @{
                    decay = $bayesianResult.temporalDecay
                    lastUsed = $bestMatch.Value.temporal.lastUsed
                }
            } | ConvertTo-Json -Depth 10
            return
        }
    }

    # No match
    @{
        found = $false
        confidence = 0.0
    } | ConvertTo-Json
}

function Record-Feedback {
    param(
        [string]$Pattern,
        [string]$SuggestedMode,
        [bool]$Success,
        [string]$ActualMode
    )

    $now = Get-Date -Format "yyyy-MM-dd HH:mm:ss"

    # Pattern must exist to record feedback
    if (-not ($learning.patterns.PSObject.Properties.Name -contains $Pattern)) {
        # Create new pattern if user manually corrected
        if (-not $Success -and $ActualMode) {
            $learning.patterns | Add-Member -NotePropertyName $Pattern -NotePropertyValue ([PSCustomObject]@{
                mode = $ActualMode
                bayesian = [PSCustomObject]@{
                    successCount = 1
                    failureCount = 0
                    lastUpdated = $now
                }
                temporal = [PSCustomObject]@{
                    lastUsed = $now
                    decayFactor = 1.0
                }
                features = [PSCustomObject]@{
                    avgFileConfidence = 0.0
                    avgConversationConfidence = 0.0
                }
            })
        } else {
            return @{ success = $false; reason = "Pattern not found" } | ConvertTo-Json
        }
    } else {
        $existing = $learning.patterns.$Pattern

        # Ensure bayesian structure exists
        if (-not ($existing.PSObject.Properties.Name -contains 'bayesian')) {
            $existing | Add-Member -NotePropertyName 'bayesian' -NotePropertyValue ([PSCustomObject]@{
                successCount = 0
                failureCount = 0
                lastUpdated = $now
            })
        }
        if (-not ($existing.bayesian.PSObject.Properties.Name -contains 'successCount')) {
            $existing.bayesian | Add-Member -NotePropertyName 'successCount' -NotePropertyValue 0 -Force
        }
        if (-not ($existing.bayesian.PSObject.Properties.Name -contains 'failureCount')) {
            $existing.bayesian | Add-Member -NotePropertyName 'failureCount' -NotePropertyValue 0 -Force
        }

        if ($Success) {
            # Explicit success - user confirmed the suggestion was correct
            $existing.bayesian.successCount++
            $learning.globalStats.explicitSuccess++
            # Note: totalSuccess already incremented by Record-Pattern (implicit)
            # No additional increment needed
        } else {
            # Explicit failure - user rejected the suggestion
            $existing.bayesian.failureCount++
            $learning.globalStats.explicitFailure++
            $learning.globalStats.totalFailure++

            # Subtract the implicit success we counted during Record-Pattern
            # since this turned out to be wrong
            if ($learning.globalStats.implicitSuccess -gt 0) {
                $learning.globalStats.implicitSuccess--
            }
            if ($learning.globalStats.totalSuccess -gt 0) {
                $learning.globalStats.totalSuccess--
            }

            # If actual mode provided and different, learn from correction
            if ($ActualMode -and $ActualMode -ne $SuggestedMode) {
                # Update mode to actual preference
                if ($existing.PSObject.Properties.Name -contains 'mode') {
                    $existing.mode = $ActualMode
                } else {
                    $existing | Add-Member -NotePropertyName 'mode' -NotePropertyValue $ActualMode -Force
                }
            }
        }

        # Update bayesian lastUpdated
        if (-not ($existing.bayesian.PSObject.Properties.Name -contains 'lastUpdated')) {
            $existing.bayesian | Add-Member -NotePropertyName 'lastUpdated' -NotePropertyValue $now -Force
        } else {
            $existing.bayesian.lastUpdated = $now
        }

        # Ensure temporal structure exists
        if (-not ($existing.PSObject.Properties.Name -contains 'temporal')) {
            $existing | Add-Member -NotePropertyName 'temporal' -NotePropertyValue ([PSCustomObject]@{
                lastUsed = $now
                decayFactor = 1.0
            })
        }
        if (-not ($existing.temporal.PSObject.Properties.Name -contains 'lastUsed')) {
            $existing.temporal | Add-Member -NotePropertyName 'lastUsed' -NotePropertyValue $now -Force
        } else {
            $existing.temporal.lastUsed = $now
        }
        if (-not ($existing.temporal.PSObject.Properties.Name -contains 'decayFactor')) {
            $existing.temporal | Add-Member -NotePropertyName 'decayFactor' -NotePropertyValue 1.0 -Force
        } else {
            $existing.temporal.decayFactor = 1.0
        }

        # Ensure features structure exists
        if (-not ($existing.PSObject.Properties.Name -contains 'features')) {
            $existing | Add-Member -NotePropertyName 'features' -NotePropertyValue ([PSCustomObject]@{
                avgFileConfidence = 0.0
                avgConversationConfidence = 0.0
            })
        }

        # Gradient descent weight learning (if feature data available)
        if ($existing.PSObject.Properties.Name -contains 'features' -and
            $existing.features.PSObject.Properties.Name -contains 'avgFileConfidence' -and
            $existing.features.PSObject.Properties.Name -contains 'avgConversationConfidence' -and
            $existing.features.avgFileConfidence -gt 0.0) {

            # Ensure weights exist
            if (-not ($existing.features.PSObject.Properties.Name -contains 'weights')) {
                $existing.features | Add-Member -NotePropertyName "weights" -NotePropertyValue ([PSCustomObject]@{
                    file = 0.6
                    conversation = 0.4
                })
            }

            $weights = $existing.features.weights
            $fileConf = $existing.features.avgFileConfidence
            $convConf = $existing.features.avgConversationConfidence

            # Current prediction (weighted combination)
            $w1 = [double]$weights.file
            $w2 = [double]$weights.conversation
            $prediction = $fileConf * $w1 + $convConf * $w2

            # Actual outcome (1 = success, 0 = failure)
            $actual = if ($Success) { 1.0 } else { 0.0 }

            # Error
            $error = $actual - $prediction

            # Gradient descent update (learning rate = 0.1)
            $learningRate = 0.1
            $w1 += $learningRate * $error * $fileConf
            $w2 += $learningRate * $error * $convConf

            # Normalize weights to sum to 1
            $total = $w1 + $w2
            if ($total -gt 0) {
                $w1 = $w1 / $total
                $w2 = $w2 / $total

                # Clamp to reasonable range (20% - 80%)
                $w1 = [Math]::Max(0.2, [Math]::Min(0.8, $w1))
                $w2 = 1.0 - $w1

                $weights.file = [Math]::Round($w1, 3)
                $weights.conversation = [Math]::Round($w2, 3)
            }
        }
    }

    # Recalculate global accuracy: totalSuccess / totalSwitches
    if ($learning.globalStats.totalSwitches -gt 0) {
        $learning.globalStats.autoSwitchAccuracy = [Math]::Round(
            $learning.globalStats.totalSuccess / $learning.globalStats.totalSwitches,
            3
        )
    }

    $learning.globalStats.lastUpdated = $now

    # Save
    $learning | ConvertTo-Json -Depth 10 | Set-Content $learningFile

    @{
        success = $true
        accuracy = $learning.globalStats.autoSwitchAccuracy
    } | ConvertTo-Json
}

function Show-Status {
    Write-Host ""
    Write-Host "Learning System Status v2.0" -ForegroundColor Cyan
    Write-Host "=====================================" -ForegroundColor Cyan
    Write-Host ""

    # Global stats
    $accuracy = if ($learning.globalStats.autoSwitchAccuracy -gt 0) {
        "$([Math]::Round($learning.globalStats.autoSwitchAccuracy * 100, 1))%"
    } else {
        "N/A"
    }

    Write-Host "Total Switches: $($learning.globalStats.totalSwitches)" -ForegroundColor Green
    Write-Host "Auto-Switch Accuracy: $accuracy" -ForegroundColor $(if ($learning.globalStats.autoSwitchAccuracy -gt 0.8) { "Green" } elseif ($learning.globalStats.autoSwitchAccuracy -gt 0.6) { "Yellow" } else { "Red" })
    Write-Host "Success/Failure: $($learning.globalStats.totalSuccess)/$($learning.globalStats.totalFailure)" -ForegroundColor Gray
    Write-Host "Last Updated: $($learning.globalStats.lastUpdated)" -ForegroundColor Gray
    Write-Host ""

    if ($learning.patterns.PSObject.Properties.Count -gt 0) {
        Write-Host "Top Learned Patterns (by confidence):" -ForegroundColor Yellow

        # Calculate confidence for each pattern and sort
        $patternConfidences = @()
        foreach ($patternProp in $learning.patterns.PSObject.Properties) {
            $bayesianResult = Calculate-BayesianConfidence -Pattern $patternProp.Value

            # Safely extract counts
            $successCount = 0
            $failureCount = 0
            if ($patternProp.Value.PSObject.Properties.Name -contains 'bayesian') {
                if ($patternProp.Value.bayesian.PSObject.Properties.Name -contains 'successCount') {
                    $successCount = [int]$patternProp.Value.bayesian.successCount
                }
                if ($patternProp.Value.bayesian.PSObject.Properties.Name -contains 'failureCount') {
                    $failureCount = [int]$patternProp.Value.bayesian.failureCount
                }
            }

            $patternConfidences += @{
                name = $patternProp.Name
                mode = $patternProp.Value.mode
                confidence = $bayesianResult.confidence
                successCount = $successCount
                failureCount = $failureCount
                decay = $bayesianResult.temporalDecay
            }
        }

        $topPatterns = $patternConfidences | Sort-Object { $_.confidence } -Descending | Select-Object -First 10

        foreach ($pattern in $topPatterns) {
            $conf = [Math]::Round($pattern.confidence * 100, 1)
            $decayPct = [Math]::Round($pattern.decay * 100, 0)
            Write-Host "  $($pattern.name) -> $($pattern.mode)" -ForegroundColor White -NoNewline
            Write-Host " (conf: $conf%, s/f: $($pattern.successCount)/$($pattern.failureCount), decay: $decayPct%)" -ForegroundColor Gray
        }
        Write-Host ""
    }
}

function Update-Correlations {
    param([string]$Pattern, [string]$Mode, [string[]]$Topics)

    # Initialize correlation structure if needed
    if (-not ($learning.PSObject.Properties.Name -contains 'correlations')) {
        $learning | Add-Member -NotePropertyName "correlations" -NotePropertyValue ([PSCustomObject]@{})
    }

    # Update pattern correlations
    if (-not ($learning.correlations.PSObject.Properties.Name -contains $Pattern)) {
        $learning.correlations | Add-Member -NotePropertyName $Pattern -NotePropertyValue ([PSCustomObject]@{
            topics = [PSCustomObject]@{}
            modes = [PSCustomObject]@{}
        })
    }

    $patternCorr = $learning.correlations.$Pattern

    # Update topic correlations (exponential moving average)
    $alpha = 0.2  # Learning rate for correlations
    foreach ($topic in $Topics) {
        # Skip if topic is empty or whitespace
        if ([string]::IsNullOrWhiteSpace($topic)) { continue }

        if ($patternCorr.topics.PSObject.Properties.Name -contains $topic) {
            # Existing topic: boost correlation
            $oldScore = [double]$patternCorr.topics.$topic
            $newScore = $oldScore * (1.0 - $alpha) + 1.0 * $alpha
            $patternCorr.topics.$topic = [Math]::Round($newScore, 3)
        } else {
            # New topic: initialize with moderate confidence
            $patternCorr.topics | Add-Member -NotePropertyName $topic -NotePropertyValue 0.5 -Force
        }
    }

    # Decay unseen topics (not mentioned in this switch)
    # Filter out PSObject internal properties
    $internalProps = @('Count', 'IsReadOnly', 'IsFixedSize', 'IsSynchronized', 'SyncRoot', 'Keys', 'Values')
    foreach ($topicProp in $patternCorr.topics.PSObject.Properties) {
        # Skip PSObject internal properties
        if ($topicProp.Name -in $internalProps) { continue }

        if ($Topics -notcontains $topicProp.Name) {
            $oldScore = [double]$topicProp.Value
            if ($oldScore -is [double] -or $oldScore -is [int]) {
                $decayed = $oldScore * 0.95  # 5% decay per switch
                $patternCorr.topics.($topicProp.Name) = [Math]::Round($decayed, 3)
            }
        }
    }

    # Update mode correlation (only if mode provided)
    if ($Mode -and -not [string]::IsNullOrWhiteSpace($Mode)) {
        if ($patternCorr.modes.PSObject.Properties.Name -contains $Mode) {
            $oldScore = [double]$patternCorr.modes.$Mode
            $newScore = $oldScore * (1.0 - $alpha) + 1.0 * $alpha
            $patternCorr.modes.$Mode = [Math]::Round($newScore, 3)
        } else {
            $patternCorr.modes | Add-Member -NotePropertyName $Mode -NotePropertyValue 0.5
        }
    }
}

function Query-Correlations {
    param([string]$Pattern, [string[]]$ActiveTopics)

    # Validate pattern exists in correlations
    $corrExists = $false
    try {
        $corrExists = $learning.correlations.PSObject.Properties.Name -contains $Pattern
    } catch {
        $corrExists = $false
    }

    if (-not $corrExists) {
        return @{
            found = $false
            boost = 0.0
            matchedTopics = 0
            totalTopics = $ActiveTopics.Count
        }
    }

    $patternCorr = $learning.correlations.$Pattern

    # Calculate correlation boost based on active topics
    $totalBoost = 0.0
    $matchCount = 0

    foreach ($topic in $ActiveTopics) {
        if ($patternCorr.topics.PSObject.Properties.Name -contains $topic) {
            $topicScore = [double]$patternCorr.topics.$topic
            $totalBoost += $topicScore
            $matchCount++
        }
    }

    # Average boost (0.0 - 1.0)
    $avgBoost = if ($matchCount -gt 0) { $totalBoost / $matchCount } else { 0.0 }

    # Get top 3 topics (excluding PSObject internal properties)
    $validTopics = $patternCorr.topics.PSObject.Properties |
                   Where-Object {
                       $_.Name -notin @('Count', 'IsReadOnly', 'IsFixedSize', 'IsSynchronized', 'SyncRoot', 'Keys', 'Values')
                   } |
                   Sort-Object { [double]$_.Value } -Descending |
                   Select-Object -First 3

    $topTopicsStr = if ($validTopics) {
        ($validTopics | ForEach-Object {
            $val = [Math]::Round([double]$_.Value * 100, 0)
            "$($_.Name):$val%"
        }) -join ", "
    } else {
        "none"
    }

    return @{
        found = $true
        boost = [Math]::Round($avgBoost, 3)
        matchedTopics = $matchCount
        totalTopics = $ActiveTopics.Count
        topTopics = $topTopicsStr
    }
}

function Reset-Learning {
    $initialData = [PSCustomObject]@{
        version = "2.1"
        patterns = [PSCustomObject]@{}
        correlations = [PSCustomObject]@{}
        globalStats = [PSCustomObject]@{
            totalSwitches = 0
            autoSwitchAccuracy = 0.0
            totalSuccess = 0
            totalFailure = 0
            lastUpdated = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
        }
    }
    $initialData | ConvertTo-Json -Depth 10 | Set-Content $learningFile
    Write-Host "[OK] Learning data reset (v2.1)" -ForegroundColor Green
}

# Execute action with automatic lock release
try {
    switch ($Action) {
        'record' {
            if (-not $Pattern -or -not $Mode) {
                Write-Host "[ERROR] Pattern and Mode required for record action" -ForegroundColor Red
                throw "Pattern and Mode required for record action"
            }
            Record-Pattern -Pattern $Pattern -Mode $Mode -FileConf $FileConfidence -ConvConf $ConversationConfidence

            # Also update correlations if topics provided
            if ($Topics.Count -gt 0) {
                Update-Correlations -Pattern $Pattern -Mode $Mode -Topics $Topics
                $learning | ConvertTo-Json -Depth 10 | Set-Content $learningFile
            }
        }
        'query' {
            if (-not $Pattern) {
                Write-Host "[ERROR] Pattern required for query action" -ForegroundColor Red
                throw "Pattern required for query action"
            }
            # Reload database to get latest changes with normalization
            $learning = Load-LearningDatabase -FilePath $learningFile
            Query-Pattern -Pattern $Pattern
        }
        'feedback' {
            if (-not $Pattern) {
                Write-Host "[ERROR] Pattern required for feedback action" -ForegroundColor Red
                throw "Pattern required for feedback action"
            }
            # Reload database to get latest changes with normalization
            $learning = Load-LearningDatabase -FilePath $learningFile
            Record-Feedback -Pattern $Pattern -SuggestedMode $Mode -Success $Success -ActualMode $ActualMode
        }
        'correlate' {
            if (-not $Pattern) {
                Write-Host "[ERROR] Pattern required for correlate action" -ForegroundColor Red
                throw "Pattern required for correlate action"
            }

            # Reload database to get latest changes with normalization
            $learning = Load-LearningDatabase -FilePath $learningFile

            if ($Topics.Count -eq 0) {
                # Query mode: return correlation data
                $result = Query-Correlations -Pattern $Pattern -ActiveTopics @()
                $result | ConvertTo-Json
            } else {
                # Update mode: record correlation
                Update-Correlations -Pattern $Pattern -Mode $Mode -Topics $Topics
                $learning | ConvertTo-Json -Depth 10 | Set-Content $learningFile

                @{ success = $true; topics = $Topics.Count } | ConvertTo-Json
            }
        }
        'status' {
            Show-Status
        }
        'reset' {
            Reset-Learning
        }
    }
} finally {
    # Always release lock
    Release-FileLock
}
