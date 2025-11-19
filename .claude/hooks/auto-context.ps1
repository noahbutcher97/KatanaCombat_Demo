# Auto-detect context mode based on file being edited/opened
# Uses intelligent mode detection with confidence scoring

$filePath = $env:FILE_PATH
if (-not $filePath) { exit 0 }

# Validate file path is not empty/whitespace
if ([string]::IsNullOrWhiteSpace($filePath)) {
    exit 0
}

# Use intelligent detector v3.0 for unified 5-factor ML detection
$intelligentDetectorPath = Join-Path $PSScriptRoot "..\scripts\intelligent-mode-detector.ps1"

if (-not (Test-Path $intelligentDetectorPath)) {
    # Fallback to holistic detector if intelligent detector missing
    $intelligentDetectorPath = Join-Path $PSScriptRoot "..\scripts\holistic-mode-detector.ps1"

    if (-not (Test-Path $intelligentDetectorPath)) {
        # Final fallback to basic file detection
        $intelligentDetectorPath = Join-Path $PSScriptRoot "..\scripts\detect-mode.ps1"
        if (-not (Test-Path $intelligentDetectorPath)) {
            exit 0
        }
    }
}

try {
    # Get current mode for context
    $trackerScript = Join-Path $PSScriptRoot "..\scripts\context-tracker.ps1"
    $currentMode = "full"
    if (Test-Path $trackerScript) {
        try {
            $trackerPath = Join-Path $PSScriptRoot ".." | Join-Path -ChildPath ".context-history.json"
            if (Test-Path $trackerPath) {
                $trackerData = Get-Content $trackerPath | ConvertFrom-Json
                if ($trackerData.PSObject.Properties.Name -contains 'currentMode') {
                    $currentMode = $trackerData.currentMode
                }
            }
        } catch {
            # Tracker read failed, use default
        }
    }

    # Call intelligent detector v3.0 with file path and current mode
    # Note: Conversation context not available at file-open time, but learning system
    # will use historical correlations to boost confidence based on past patterns
    $detectionResultRaw = & powershell.exe -ExecutionPolicy Bypass -File $intelligentDetectorPath -FilePath $filePath -CurrentMode $currentMode 2>&1

    if (-not $detectionResultRaw) {
        exit 0
    }

    # Extract JSON from output (filter non-JSON lines)
    $jsonLine = $detectionResultRaw | Where-Object { $_ -match '^\s*\{' } | Select-Object -Last 1

    if (-not $jsonLine) {
        exit 0
    }

    # Parse JSON result
    $detection = $jsonLine | ConvertFrom-Json

    if (-not $detection) {
        exit 0
    }

    $detectedMode = $detection.suggestedMode
    $confidence = [double]$detection.confidence
    $confidenceLevel = $detection.confidenceLevel

    # Skip if no useful detection (full mode with 0% confidence)
    if ($detectedMode -eq "full" -and $confidence -eq 0) {
        exit 0
    }

    # Record pattern usage for learning (non-blocking)
    $learningTracker = Join-Path $PSScriptRoot "..\scripts\learning-tracker.ps1"
    if (Test-Path $learningTracker) {
        try {
            $fileName = [System.IO.Path]::GetFileNameWithoutExtension($filePath)
            if ($fileName -and -not [string]::IsNullOrWhiteSpace($fileName)) {
                # Extract feature confidences if available
                $fileConf = 0.0
                $convConf = 0.0
                if ($detection.PSObject.Properties.Name -contains 'factors') {
                    if ($detection.factors.PSObject.Properties.Name -contains 'file') {
                        $fileConf = [double]$detection.factors.file.confidence
                    }
                    if ($detection.factors.PSObject.Properties.Name -contains 'conversation') {
                        $convConf = [double]$detection.factors.conversation.confidence
                    }
                }

                # Record usage (async, don't wait)
                Start-Job -ScriptBlock {
                    param($tracker, $pattern, $mode, $fileC, $convC)
                    & $tracker -Action record -Pattern $pattern -Mode $mode -FileConfidence $fileC -ConversationConfidence $convC 2>$null | Out-Null
                } -ArgumentList $learningTracker, $fileName, $detectedMode, $fileConf, $convConf | Out-Null
            }
        } catch {
            # Learning recording failed, continue (don't break hook)
        }
    }
}
catch {
    # Any error in detection, exit silently (hook should never break the workflow)
    exit 0
}

# Mode-specific documentation and principles
$modeInfo = @{
    animation = @{
        docs = @(
            "docs/PHASE_SYSTEM_MIGRATION.md - Phase notify requirements"
            "docs/ARCHITECTURE.md:500-700 - Animation integration"
        )
        principles = @(
            "Phase notifies: Windup -> Active -> Recovery"
            "Windows overlap phases (ComboWindow, HoldWindow, ParryWindow)"
            "Use AnimNotify_AttackPhaseTransition, not deprecated AnimNotifyState_AttackPhase"
        )
    }

    'combat-logic' = @{
        docs = @(
            "docs/SYSTEM_PROMPT.md - Core design principles"
            "docs/ARCHITECTURE_QUICK.md - Default values"
        )
        principles = @(
            "Phases exclusive (Windup->Active->Recovery)"
            "Input ALWAYS buffered (combo window modifies WHEN, not WHETHER)"
            "Timer-based, not Tick-based"
        )
    }

    'data-assets' = @{
        docs = @(
            "docs/ATTACK_CREATION.md - Creating attack assets"
            "docs/ARCHITECTURE.md:200-400 - Data architecture"
        )
        principles = @(
            "Three-tier: Character -> CombatSettings -> AttackConfiguration"
            "Validation in AttackData::Validate()"
            "Default values in ARCHITECTURE_QUICK.md"
        )
    }

    'editor-ui' = @{
        docs = @(
            "UE5.6 Slate Framework docs"
            "UE5.6 AssetEditor API"
        )
        principles = @(
            "IDetailCustomization for property panels"
            "FAssetEditorToolkit for custom editors"
            "Editor module separate from runtime"
        )
    }

    testing = @{
        docs = @(
            "Source/KatanaCombatTest/README.md - Test infrastructure"
            "docs/ARCHITECTURE.md - Expected behaviors"
        )
        principles = @(
            "IMPLEMENT_SIMPLE_AUTOMATION_TEST for basic tests"
            "Spec-style: DEFINE_SPEC/DESCRIBE/IT for BDD"
            "7 existing test files with 45+ assertions"
        )
    }

    documentation = @{
        docs = @(
            "CLAUDE.md - Project guide"
            "docs/SYSTEM_PROMPT.md - Core philosophy"
        )
        principles = @(
            "Use file:line references"
            "ASCII diagrams for timing/states"
            "Date changes (YYYY-MM-DD)"
        )
    }

    full = @{
        docs = @(
            "CLAUDE.md - Project overview"
        )
        principles = @(
            "No filtering active"
        )
    }
}

# Auto-switch logic based on confidence
$autoSwitchEnabled = Test-Path ".claude/.auto-switch-enabled"

if ($autoSwitchEnabled) {
    # Load confidence thresholds from config (with fallback to defaults)
    $highThreshold = 0.80
    $mediumThreshold = 0.50

    $configFile = ".claude/.context-config.json"
    if (Test-Path $configFile) {
        try {
            $config = Get-Content $configFile | ConvertFrom-Json
            if ($config.intelligentSwitching.PSObject.Properties.Name -contains 'confidenceThresholds') {
                $thresholds = $config.intelligentSwitching.confidenceThresholds
                if ($thresholds.PSObject.Properties.Name -contains 'high') {
                    $highThreshold = [double]$thresholds.high
                }
                if ($thresholds.PSObject.Properties.Name -contains 'medium') {
                    $mediumThreshold = [double]$thresholds.medium
                }
            }
        } catch {
            # Config load failed, use defaults
        }
    }

    $shouldSwitch = $false
    $switchReason = "Auto-detected from file: $filePath"

    if ($confidence -ge $highThreshold) {
        # High confidence: auto-switch immediately
        $shouldSwitch = $true
        $switchReason = "Auto-switch (high confidence: $([Math]::Round($confidence * 100))%): $filePath"
    }
    elseif ($confidence -ge $mediumThreshold) {
        # Medium confidence: auto-switch with notification
        $shouldSwitch = $true
        $switchReason = "Auto-switch (medium confidence: $([Math]::Round($confidence * 100))%): $filePath"
    }
    # Low confidence (<50%): Show hint only, don't auto-switch

    if ($shouldSwitch) {
        # Record the switch
        $trackerScript = Join-Path $PSScriptRoot "..\scripts\context-tracker.ps1"

        if (Test-Path $trackerScript) {
            try {
                & powershell.exe -ExecutionPolicy Bypass -File $trackerScript -Action switch -Mode $detectedMode -Reason $switchReason 2>$null
            }
            catch {
                # Tracker error, continue anyway (don't break auto-switch)
            }
        }

        # Record as successful prediction for learning (user accepted auto-switch)
        $learningTracker = Join-Path $PSScriptRoot "..\scripts\learning-tracker.ps1"
        if (Test-Path $learningTracker) {
            try {
                $fileName = [System.IO.Path]::GetFileNameWithoutExtension($filePath)
                if ($fileName) {
                    # Record success feedback (async)
                    Start-Job -ScriptBlock {
                        param($tracker, $pattern, $mode)
                        & $tracker -Action feedback -Pattern $pattern -Mode $mode -Success $true 2>$null | Out-Null
                    } -ArgumentList $learningTracker, $fileName, $detectedMode | Out-Null
                }
            } catch {
                # Feedback recording failed, continue
            }
        }
    }
}

# Build contextual reminder
$info = $modeInfo[$detectedMode]
$reminder = "<system-reminder>`n"

# Header with confidence indicator
$confidencePct = [Math]::Round($confidence * 100)
if ($autoSwitchEnabled -and $confidence -ge 0.50) {
    $reminder += "AUTO-SWITCHED TO: $($detectedMode.ToUpper()) (Confidence: $confidencePct%)`n`n"
} else {
    $reminder += "AUTO-DETECTED CONTEXT: $($detectedMode.ToUpper()) (Confidence: $confidencePct%)`n`n"
}

# Documentation
if ($info -and $info.docs) {
    $reminder += "Relevant Documentation:`n"
    foreach ($doc in $info.docs) {
        $reminder += "  - $doc`n"
    }
}

# Principles
if ($info -and $info.principles) {
    $reminder += "`nKey Principles:`n"
    foreach ($principle in $info.principles) {
        $reminder += "  - $principle`n"
    }
}

# Confidence breakdown (if available from intelligent detector v3.0)
if ($detection.PSObject.Properties.Name -contains 'factors') {
    $reminder += "`nConfidence Breakdown:`n"
    foreach ($factorName in @('file', 'conversation', 'learning', 'history', 'time')) {
        if ($detection.factors.PSObject.Properties.Name -contains $factorName) {
            $factor = $detection.factors.$factorName
            $factorConf = [Math]::Round([double]$factor.confidence * 100, 1)
            $weight = [Math]::Round([double]$factor.weight * 100)
            $weighted = [Math]::Round([double]$factor.weightedScore, 3)
            $reminder += "  $factorName ($weight%): $factorConf% conf -> $weighted weighted`n"
        }
    }
}

# Action hint based on confidence
if ($autoSwitchEnabled) {
    if ($confidence -ge 0.80) {
        $reminder += "`n[AUTO-SWITCH] Automatically switched to $detectedMode mode (high confidence)`n"
    }
    elseif ($confidence -ge 0.50) {
        $reminder += "`n[AUTO-SWITCH] Automatically switched to $detectedMode mode (medium confidence)`n"
    }
    else {
        $reminder += "`nHINT: Consider '/mode $detectedMode' (low confidence: $confidencePct%)`n"
    }

    # Negative feedback detection: If user manually overrides within 30 seconds, record failure
    $reminder += "`nNote: Manual mode switch within 30s will record negative feedback for ML learning`n"
} else {
    $reminder += "`nTip: Use '/mode $detectedMode' for full context switch, or use '/mode auto enable' for auto-switching`n"
}

$reminder += "</system-reminder>"

Write-Output $reminder
