# SessionStart Hook
# Runs when Claude Code session starts, resumes, or is cleared

param(
    [Parameter(Mandatory=$false)]
    [string]$Source = "startup"  # startup, resume, clear, compact
)

# Skip if disabled
if ($env:CLAUDE_SKIP_SESSION_START -eq "1") {
    exit 0
}

Write-Host ""
Write-Host "===========================================================================" -ForegroundColor Cyan
Write-Host " KatanaCombat - Session Initialized" -ForegroundColor Cyan
Write-Host "===========================================================================" -ForegroundColor Cyan
Write-Host ""

# Session type
switch ($Source) {
    "startup" { Write-Host "[SESSION] New session started" -ForegroundColor Green }
    "resume" { Write-Host "[SESSION] Session resumed" -ForegroundColor Yellow }
    "clear" { Write-Host "[SESSION] Session cleared" -ForegroundColor Yellow }
    "compact" { Write-Host "[SESSION] Session compacted" -ForegroundColor Yellow }
    default { Write-Host "[SESSION] Session initialized ($Source)" -ForegroundColor Gray }
}
Write-Host ""

# Check git status
try {
    $gitStatus = git status --short 2>&1
    if ($LASTEXITCODE -eq 0 -and $gitStatus) {
        $changedFiles = ($gitStatus | Measure-Object).Count
        Write-Host "[GIT] Uncommitted changes: $changedFiles files" -ForegroundColor Yellow

        # Show first few changed files
        $filesToShow = [Math]::Min(5, $changedFiles)
        $gitStatus | Select-Object -First $filesToShow | ForEach-Object {
            Write-Host "      $_" -ForegroundColor Gray
        }
        if ($changedFiles -gt $filesToShow) {
            Write-Host "      ... and $($changedFiles - $filesToShow) more" -ForegroundColor DarkGray
        }
        Write-Host ""
    } elseif ($LASTEXITCODE -eq 0) {
        Write-Host "[GIT] Working tree clean" -ForegroundColor Green
        Write-Host ""
    }
} catch {
    # Not a git repo or git not available
}

# Show active context mode
try {
    if (Test-Path .claude/.context-history.json) {
        $contextHistory = Get-Content .claude/.context-history.json -Raw | ConvertFrom-Json
        if ($contextHistory.switches -and $contextHistory.switches.Count -gt 0) {
            $lastSwitch = $contextHistory.switches[-1]
            $currentMode = $lastSwitch.toMode
            $switchTime = [DateTime]::Parse($lastSwitch.timestamp)
            $timeAgo = (Get-Date) - $switchTime

            $timeAgoStr = if ($timeAgo.TotalMinutes -lt 60) {
                "$([Math]::Round($timeAgo.TotalMinutes)) minutes ago"
            } elseif ($timeAgo.TotalHours -lt 24) {
                "$([Math]::Round($timeAgo.TotalHours)) hours ago"
            } else {
                "$([Math]::Round($timeAgo.TotalDays)) days ago"
            }

            Write-Host "[CONTEXT] Active Mode: $currentMode" -ForegroundColor Cyan
            Write-Host "          Last switch: $timeAgoStr" -ForegroundColor Gray
            Write-Host ""
        }
    }
} catch {
    # Context history not available
}

# Show recent commits
try {
    $recentCommits = git log -3 --oneline --no-decorate 2>&1
    if ($LASTEXITCODE -eq 0) {
        Write-Host "[GIT] Recent Commits:" -ForegroundColor Cyan
        $recentCommits | ForEach-Object {
            Write-Host "      $_" -ForegroundColor Gray
        }
        Write-Host ""
    }
} catch {
    # Git not available
}

# Show current branch
try {
    $currentBranch = git branch --show-current 2>&1
    if ($LASTEXITCODE -eq 0) {
        Write-Host "[GIT] Current Branch: $currentBranch" -ForegroundColor Cyan
        Write-Host ""
    }
} catch {
    # Git not available
}

# UE5.6 environment check
$ue5Path = $env:UE5_ROOT
if ($ue5Path -and (Test-Path $ue5Path)) {
    Write-Host "[UE5.6] Engine path: $ue5Path" -ForegroundColor Green
} elseif ($ue5Path) {
    Write-Host "[UE5.6] Engine path configured but not found: $ue5Path" -ForegroundColor Yellow
} else {
    Write-Host "[UE5.6] Engine path not configured (UE5_ROOT)" -ForegroundColor DarkGray
}
Write-Host ""

# Quick tips
Write-Host "===========================================================================" -ForegroundColor DarkGray
Write-Host "Intelligent Infrastructure:" -ForegroundColor Cyan
Write-Host ""
Write-Host "  CONTEXT MODES:" -ForegroundColor Yellow
Write-Host "    /mode [name]        - Switch context (animation, combat-logic, etc.)" -ForegroundColor Gray
Write-Host "    /mode status        - Show context system status" -ForegroundColor Gray
Write-Host "    /mode analyze       - Detailed confidence breakdown" -ForegroundColor Gray
Write-Host "    Auto-Switching: " -NoNewline -ForegroundColor Gray
if (Test-Path .claude/.auto-switch-enabled) {
    Write-Host "ENABLED" -ForegroundColor Green
} else {
    Write-Host "DISABLED (use '/mode auto enable')" -ForegroundColor Yellow
}
Write-Host ""
Write-Host "  AGENT COORDINATION:" -ForegroundColor Yellow
Write-Host "    /agent status       - Show available specialist agents" -ForegroundColor Gray
Write-Host "    Launch agents via Task tool for complex multi-file work" -ForegroundColor Gray
Write-Host ""
Write-Host "  OTHER COMMANDS:" -ForegroundColor Yellow
Write-Host "    /hooks status       - Check hook configuration" -ForegroundColor Gray
Write-Host "    /check-warnings     - Run diagnostics" -ForegroundColor Gray
Write-Host "    /validate-combat    - Validate combat system" -ForegroundColor Gray
Write-Host "===========================================================================" -ForegroundColor DarkGray
Write-Host ""