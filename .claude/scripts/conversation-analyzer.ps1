# Conversation-Based Mode Detection
# Analyzes conversation text to suggest context mode
# Designed to be called from /mode suggest command where conversation is available

param(
    [Parameter(Mandatory=$false)]
    [string]$ConversationText = "",

    [Parameter(Mandatory=$false)]
    [switch]$ShowDetails
)

# Topic keyword mappings (weighted by relevance)
$topicKeywords = @{
    animation = @{
        high = @('AnimNotify', 'AnimNotifyState', 'montage', 'AnimMontage', 'AnimInstance', 'animation blend')
        medium = @('animation', 'anim', 'phase transition', 'notify', 'blend', 'MontageUtility')
        low = @('playback', 'timing', 'frame', 'AnimBP')
    }

    'combat-logic' = @{
        high = @('CombatComponent', 'ActionQueue', 'combat state', 'attack execution', 'combo system')
        medium = @('combat', 'attack', 'combo', 'input buffer', 'state machine', 'transition')
        low = @('hit', 'damage', 'targeting', 'weapon', 'player input')
    }

    'data-assets' = @{
        high = @('AttackData', 'AttackConfiguration', 'CombatSettings', 'attack asset', 'data asset')
        medium = @('property', 'asset', 'configuration', 'settings', 'PDA', 'Primary Data Asset')
        low = @('value', 'parameter', 'tuning', 'balance')
    }

    'editor-ui' = @{
        high = @('AssetEditor', 'DetailCustomization', 'Slate', 'FAssetEditorToolkit', 'IDetailCustomization')
        medium = @('editor', 'customization', 'panel', 'UI', 'widget', 'toolbar')
        low = @('button', 'menu', 'window', 'UEditorUtilityWidget')
    }

    testing = @{
        high = @('IMPLEMENT_SIMPLE_AUTOMATION_TEST', 'DEFINE_SPEC', 'unit test', 'integration test')
        medium = @('test', 'spec', 'automation', 'assert', 'mock', 'fixture')
        low = @('verify', 'check', 'validate', 'debug test')
    }

    documentation = @{
        high = @('documentation', 'README', 'guide', 'tutorial', 'CHANGELOG')
        medium = @('docs', 'markdown', 'comment', 'explain', 'document')
        low = @('note', 'description', 'summary')
    }
}

# Task complexity keywords
$complexityKeywords = @{
    high = @('architecture', 'multi-system', 'pipeline', 'major refactor', 'framework', 'infrastructure')
    medium = @('implement', 'feature', 'system', 'refactor', 'redesign', 'integration')
    low = @('add property', 'fix typo', 'update value', 'change parameter', 'tweak')
}

# Input validation
if ([string]::IsNullOrWhiteSpace($ConversationText)) {
    $result = @{
        suggestedMode = "full"
        confidence = 0.0
        confidenceLevel = "none"
        reason = "No conversation text provided"
        topicDistribution = @{}
        taskComplexity = "unknown"
    }

    if ($ShowDetails) {
        Write-Host "[WARN] No conversation text provided for analysis" -ForegroundColor Yellow
    } else {
        $result | ConvertTo-Json -Depth 10
    }
    exit 0
}

# Normalize text for analysis
$normalizedText = $ConversationText.ToLower()

# Calculate topic scores
$topicScores = @{}

foreach ($mode in $topicKeywords.Keys) {
    $score = 0.0
    $matchDetails = @{
        high = 0
        medium = 0
        low = 0
    }

    # High relevance keywords (weight: 1.0)
    foreach ($keyword in $topicKeywords[$mode].high) {
        $matches = ([regex]::Matches($normalizedText, [regex]::Escape($keyword.ToLower()))).Count
        if ($matches -gt 0) {
            $score += $matches * 1.0
            $matchDetails.high += $matches
        }
    }

    # Medium relevance keywords (weight: 0.6)
    foreach ($keyword in $topicKeywords[$mode].medium) {
        $matches = ([regex]::Matches($normalizedText, [regex]::Escape($keyword.ToLower()))).Count
        if ($matches -gt 0) {
            $score += $matches * 0.6
            $matchDetails.medium += $matches
        }
    }

    # Low relevance keywords (weight: 0.3)
    foreach ($keyword in $topicKeywords[$mode].low) {
        $matches = ([regex]::Matches($normalizedText, [regex]::Escape($keyword.ToLower()))).Count
        if ($matches -gt 0) {
            $score += $matches * 0.3
            $matchDetails.low += $matches
        }
    }

    if ($score -gt 0) {
        $topicScores[$mode] = @{
            score = $score
            matches = $matchDetails
        }
    }
}

# Calculate total score and distribution
$totalScore = ($topicScores.Values | ForEach-Object { $_.score } | Measure-Object -Sum).Sum

if ($totalScore -eq 0) {
    # No topic matches found
    $result = @{
        suggestedMode = "full"
        confidence = 0.0
        confidenceLevel = "none"
        reason = "No topic keywords detected in conversation"
        topicDistribution = @{}
        taskComplexity = "unknown"
    }
} else {
    # Calculate percentage distribution
    $distribution = @{}
    foreach ($mode in $topicScores.Keys) {
        $percentage = $topicScores[$mode].score / $totalScore
        $distribution[$mode] = [Math]::Round($percentage, 3)
    }

    # Find dominant topic
    $dominant = $distribution.GetEnumerator() | Sort-Object Value -Descending | Select-Object -First 1
    $dominantMode = $dominant.Key
    $dominantPercentage = $dominant.Value

    # Determine confidence level
    $confidence = $dominantPercentage
    $confidenceLevel = if ($dominantPercentage -ge 0.60) { "high" }
                      elseif ($dominantPercentage -ge 0.40) { "medium" }
                      else { "low" }

    # Detect task complexity
    $taskComplexity = "low"
    foreach ($level in @('high', 'medium')) {
        foreach ($keyword in $complexityKeywords[$level]) {
            if ($normalizedText -match [regex]::Escape($keyword.ToLower())) {
                $taskComplexity = $level
                break
            }
        }
        if ($taskComplexity -eq $level) { break }
    }

    # Build reason
    $totalMatches = ($topicScores[$dominantMode].matches.high +
                     $topicScores[$dominantMode].matches.medium +
                     $topicScores[$dominantMode].matches.low)
    $reason = "Conversation: $([Math]::Round($dominantPercentage * 100))% $dominantMode topic ($totalMatches keyword matches)"

    $result = @{
        suggestedMode = $dominantMode
        confidence = $confidence
        confidenceLevel = $confidenceLevel
        reason = $reason
        topicDistribution = $distribution
        taskComplexity = $taskComplexity
        matchDetails = $topicScores
    }
}

# Output
if ($ShowDetails) {
    Write-Host ""
    Write-Host "=== Conversation Analysis ===" -ForegroundColor Cyan
    Write-Host ""

    if ($result.suggestedMode -ne "full") {
        Write-Host "Suggested Mode: $($result.suggestedMode)" -ForegroundColor Green
        Write-Host "Confidence: $([Math]::Round($result.confidence * 100, 1))% ($($result.confidenceLevel))" -ForegroundColor Yellow
        Write-Host "Task Complexity: $($result.taskComplexity)" -ForegroundColor Gray
        Write-Host ""

        Write-Host "Topic Distribution:" -ForegroundColor Yellow
        $sortedTopics = $result.topicDistribution.GetEnumerator() | Sort-Object Value -Descending
        foreach ($topic in $sortedTopics) {
            $pct = [Math]::Round($topic.Value * 100, 1)
            $bar = "#" * [Math]::Min([Math]::Ceiling($pct / 5), 20)
            Write-Host "  $($topic.Key.PadRight(15)): $bar $pct%" -ForegroundColor Gray
        }
        Write-Host ""

        if ($result.matchDetails) {
            Write-Host "Match Details (Top Mode: $($result.suggestedMode)):" -ForegroundColor Yellow
            $details = $result.matchDetails[$result.suggestedMode].matches
            Write-Host "  High priority: $($details.high) matches" -ForegroundColor Green
            Write-Host "  Medium priority: $($details.medium) matches" -ForegroundColor Yellow
            Write-Host "  Low priority: $($details.low) matches" -ForegroundColor Gray
        }
    } else {
        Write-Host "No dominant topic detected" -ForegroundColor Yellow
        Write-Host "Suggestion: Stay in 'full' mode for mixed/exploratory tasks" -ForegroundColor Gray
    }
    Write-Host ""
} else {
    $result | ConvertTo-Json -Depth 10
}
