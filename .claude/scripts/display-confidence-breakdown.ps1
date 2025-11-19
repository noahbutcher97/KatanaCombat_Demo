# Display Confidence Breakdown
# Rich visual display of multi-factor mode detection results
# Shows factor contributions, confidence levels, and reasoning

param(
    [Parameter(Mandatory=$true)]
    [string]$DetectionResultJson,

    [Parameter(Mandatory=$false)]
    [switch]$Compact
)

try {
    $detection = $DetectionResultJson | ConvertFrom-Json
} catch {
    Write-Host "[ERROR] Invalid JSON input" -ForegroundColor Red
    exit 1
}

if (-not $detection) {
    exit 0
}

$mode = $detection.suggestedMode
$confidence = [double]$detection.confidence
$confidenceLevel = $detection.confidenceLevel

# Skip display if no useful detection
if ($mode -eq "full" -and $confidence -eq 0) {
    exit 0
}

# ============================================================================
# Color Coding
# ============================================================================
$colorHigh = 'Green'
$colorMedium = 'Yellow'
$colorLow = 'Red'
$colorNeutral = 'Gray'

$confidenceColor = switch ($confidenceLevel) {
    'high' { $colorHigh }
    'medium' { $colorMedium }
    'low' { $colorLow }
    default { $colorNeutral }
}

# ============================================================================
# Compact Display (for system reminders)
# ============================================================================
if ($Compact) {
    $output = "MODE: $($mode.ToUpper()) | Confidence: $([Math]::Round($confidence * 100))% ($confidenceLevel)"

    # Add top contributing factor
    if ($detection.PSObject.Properties.Name -contains 'factors') {
        $topFactor = $null
        $topScore = 0.0

        foreach ($factorName in @('file', 'conversation', 'learning', 'history', 'time')) {
            if ($detection.factors.PSObject.Properties.Name -contains $factorName) {
                $factor = $detection.factors.$factorName
                $score = [double]$factor.weightedScore
                if ($score -gt $topScore) {
                    $topScore = $score
                    $topFactor = $factorName
                }
            }
        }

        if ($topFactor) {
            $output += " | Primary: $topFactor"
        }
    }

    Write-Output $output
    exit 0
}

# ============================================================================
# Full Display
# ============================================================================

Write-Host ""
Write-Host "================================================================" -ForegroundColor Cyan
Write-Host "      INTELLIGENT MODE DETECTION - CONFIDENCE REPORT" -ForegroundColor Cyan
Write-Host "================================================================" -ForegroundColor Cyan
Write-Host ""

# ============================================================================
# Overall Result
# ============================================================================
Write-Host "SUGGESTED MODE: " -NoNewline -ForegroundColor White
Write-Host $mode.ToUpper() -ForegroundColor $confidenceColor

Write-Host "CONFIDENCE:     " -NoNewline -ForegroundColor White
$confidencePct = [Math]::Round($confidence * 100, 1)
Write-Host "$confidencePct% " -NoNewline -ForegroundColor $confidenceColor
Write-Host "($confidenceLevel)" -ForegroundColor $colorNeutral

Write-Host ""

# ============================================================================
# Visual Confidence Bar
# ============================================================================
$barLength = 50
$filledBars = [Math]::Floor($confidence * $barLength)
$emptyBars = $barLength - $filledBars

$bar = ""
for ($i = 0; $i -lt $filledBars; $i++) { $bar += "#" }
for ($i = 0; $i -lt $emptyBars; $i++) { $bar += "-" }

Write-Host "CONFIDENCE BAR: " -NoNewline -ForegroundColor White
Write-Host $bar -ForegroundColor $confidenceColor
Write-Host ""

# ============================================================================
# Factor Breakdown
# ============================================================================
if ($detection.PSObject.Properties.Name -contains 'factors' -and $detection.factors.PSObject.Properties.Count -gt 0) {
    Write-Host "+-------------------------------------------------------------+" -ForegroundColor Cyan
    Write-Host "|  FACTOR CONTRIBUTIONS                                       |" -ForegroundColor Cyan
    Write-Host "+-------------------------------------------------------------+" -ForegroundColor Cyan

    foreach ($factorName in @('file', 'conversation', 'learning', 'history', 'time')) {
        if ($detection.factors.PSObject.Properties.Name -contains $factorName) {
            $factor = $detection.factors.$factorName
            $factorConf = [Math]::Round([double]$factor.confidence * 100, 1)
            $weight = [Math]::Round([double]$factor.weight * 100)
            $weighted = [Math]::Round([double]$factor.weightedScore, 3)

            # Color code based on contribution
            $factorColor = if ($weighted -ge 0.20) { $colorHigh }
                          elseif ($weighted -ge 0.10) { $colorMedium }
                          else { $colorNeutral }

            # Visual bar for weighted score
            $factorBarLength = 20
            $factorFilled = [Math]::Floor($weighted * $factorBarLength / 0.50)  # Scale to 0.50 max
            $factorBar = ""
            for ($i = 0; $i -lt $factorFilled; $i++) { $factorBar += "#" }
            for ($i = $factorFilled; $i -lt $factorBarLength; $i++) { $factorBar += "-" }

            Write-Host "|  " -NoNewline -ForegroundColor Cyan
            Write-Host "$factorName".PadRight(12) -NoNewline -ForegroundColor White
            Write-Host " $factorBar " -NoNewline -ForegroundColor $factorColor
            Write-Host "$weighted".PadLeft(5) -NoNewline -ForegroundColor $factorColor
            Write-Host "  |" -ForegroundColor Cyan

            # Details line
            Write-Host "|  " -NoNewline -ForegroundColor Cyan
            Write-Host "  -> " -NoNewline -ForegroundColor DarkGray
            Write-Host "conf: $factorConf% " -NoNewline -ForegroundColor Gray
            Write-Host "x weight: $weight% " -NoNewline -ForegroundColor Gray
            Write-Host "-> $($factor.mode)".PadRight(31) -NoNewline -ForegroundColor DarkGray
            Write-Host "|" -ForegroundColor Cyan
        }
    }

    Write-Host "+-------------------------------------------------------------+" -ForegroundColor Cyan
    Write-Host ""
}

# ============================================================================
# Algorithm Details
# ============================================================================
if ($detection.PSObject.Properties.Name -contains 'algorithm') {
    Write-Host "Algorithm: " -NoNewline -ForegroundColor Gray
    Write-Host $detection.algorithm -ForegroundColor White
}

if ($detection.PSObject.Properties.Name -contains 'reason') {
    Write-Host "Reason:    " -NoNewline -ForegroundColor Gray
    Write-Host $detection.reason -ForegroundColor White
}

Write-Host ""

# ============================================================================
# Recommendation
# ============================================================================
$currentMode = if ($detection.PSObject.Properties.Name -contains 'currentMode') {
    $detection.currentMode
} else {
    "unknown"
}

$shouldSwitch = if ($detection.PSObject.Properties.Name -contains 'shouldSwitch') {
    $detection.shouldSwitch
} else {
    $false
}

if ($shouldSwitch) {
    Write-Host "================================================================" -ForegroundColor Green
    Write-Host "  RECOMMENDATION: SWITCH TO $($mode.ToUpper())" -ForegroundColor Green
    Write-Host "================================================================" -ForegroundColor Green
    Write-Host ""
    Write-Host "  Current: $currentMode → Suggested: $mode" -ForegroundColor Yellow
    Write-Host ""
} else {
    if ($confidence -ge 0.30) {
        Write-Host "================================================================" -ForegroundColor Yellow
        Write-Host "  SUGGESTION: Consider switching to $($mode.ToUpper())" -ForegroundColor Yellow
        Write-Host "================================================================" -ForegroundColor Yellow
        Write-Host ""
        Write-Host "  Low/medium confidence - manual switch recommended" -ForegroundColor Gray
        Write-Host "  Command: /mode $mode" -ForegroundColor Gray
        Write-Host ""
    }
}

# ============================================================================
# Footer
# ============================================================================
Write-Host "---------------------------------------------------------------" -ForegroundColor DarkGray
Write-Host "  Use '/mode config' to adjust thresholds and weights" -ForegroundColor DarkGray
Write-Host "  Use '/mode learn' to view ML learning status" -ForegroundColor DarkGray
Write-Host "---------------------------------------------------------------" -ForegroundColor DarkGray
Write-Host ""