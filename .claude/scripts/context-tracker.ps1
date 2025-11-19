# Context State Tracker
# Tracks context switches and provides analytics

param(
    [Parameter(Mandatory=$false)]
    [string]$Action = "status",

    [Parameter(Mandatory=$false)]
    [string]$Mode = "",

    [Parameter(Mandatory=$false)]
    [string]$Reason = ""
)

$trackerFile = ".claude/.context-history.json"

# Initialize tracker file if doesn't exist
if (-not (Test-Path $trackerFile)) {
    $initialData = @{
        currentMode = "full"
        autoSwitchEnabled = $false
        history = @()
        statistics = @{
            totalSwitches = 0
            modeUsage = @{}
        }
    }
    $initialData | ConvertTo-Json -Depth 10 | Set-Content $trackerFile
}

# Load current state
$state = Get-Content $trackerFile | ConvertFrom-Json

function Update-State {
    $state | ConvertTo-Json -Depth 10 | Set-Content $trackerFile
}

function Add-SwitchHistory {
    param(
        [string]$FromMode,
        [string]$ToMode,
        [string]$Reason
    )

    $switch = @{
        timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
        from = $FromMode
        to = $ToMode
        reason = $Reason
    }

    $state.history += $switch
    $state.currentMode = $ToMode
    $state.statistics.totalSwitches++

    # Update mode usage stats
    if (-not ($state.statistics.modeUsage.PSObject.Properties.Name -contains $ToMode)) {
        $state.statistics.modeUsage | Add-Member -NotePropertyName $ToMode -NotePropertyValue 1
    } else {
        $state.statistics.modeUsage.$ToMode++
    }

    # Keep only last 50 switches
    if ($state.history.Count -gt 50) {
        $state.history = $state.history | Select-Object -Last 50
    }

    Update-State
}

function Show-Status {
    Write-Host ""
    Write-Host "Context System Status" -ForegroundColor Cyan
    Write-Host "=====================================" -ForegroundColor Cyan
    Write-Host ""

    # Current state
    Write-Host "Current Mode: " -NoNewline
    Write-Host $state.currentMode -ForegroundColor Green
    Write-Host ""

    Write-Host "Auto-Switch: " -NoNewline
    $autoEnabled = Test-Path ".claude/.auto-switch-enabled"
    if ($autoEnabled) {
        Write-Host "ENABLED" -ForegroundColor Green
    } else {
        Write-Host "DISABLED" -ForegroundColor Yellow
    }
    Write-Host ""

    # Recent switches
    if ($state.history.Count -gt 0) {
        Write-Host "Recent Context Switches (Last 5):" -ForegroundColor Yellow
        Write-Host ""

        $recent = $state.history | Select-Object -Last 5
        foreach ($switch in $recent) {
            Write-Host "  $($switch.timestamp): " -ForegroundColor Gray -NoNewline
            Write-Host "$($switch.from)" -ForegroundColor Red -NoNewline
            Write-Host " -> " -ForegroundColor Gray -NoNewline
            Write-Host "$($switch.to)" -ForegroundColor Green
            Write-Host "    Reason: $($switch.reason)" -ForegroundColor DarkGray
        }
        Write-Host ""
    }

    # Statistics
    if ($state.statistics.totalSwitches -gt 0) {
        Write-Host "Statistics:" -ForegroundColor Yellow
        Write-Host "  Total Switches: $($state.statistics.totalSwitches)" -ForegroundColor Gray
        Write-Host ""

        Write-Host "  Mode Usage:" -ForegroundColor Gray
        $sortedUsage = $state.statistics.modeUsage.PSObject.Properties | Sort-Object Value -Descending
        foreach ($mode in $sortedUsage) {
            $bar = "#" * [math]::Min($mode.Value, 20)
            Write-Host "    $($mode.Name): " -NoNewline -ForegroundColor Cyan
            Write-Host "$bar " -NoNewline -ForegroundColor Blue
            Write-Host "$($mode.Value)" -ForegroundColor Gray
        }
        Write-Host ""
    }

    # Commands
    Write-Host "Commands:" -ForegroundColor Yellow
    Write-Host "  /mode status        - Full context status" -ForegroundColor Gray
    Write-Host "  /mode auto enable   - Enable auto-switching" -ForegroundColor Gray
    Write-Host "  /mode suggest       - Get mode recommendation" -ForegroundColor Gray
    Write-Host "  /mode [name]        - Manually switch mode" -ForegroundColor Gray
    Write-Host ""
}

function Show-Analytics {
    Write-Host ""
    Write-Host "Context Analytics" -ForegroundColor Cyan
    Write-Host "=====================================" -ForegroundColor Cyan
    Write-Host ""

    if ($state.history.Count -eq 0) {
        Write-Host "No context switch history available." -ForegroundColor Yellow
        Write-Host ""
        return
    }

    # Mode distribution
    Write-Host "Mode Usage Distribution:" -ForegroundColor Yellow
    $total = ($state.statistics.modeUsage.PSObject.Properties | Measure-Object -Property Value -Sum).Sum
    $sortedUsage = $state.statistics.modeUsage.PSObject.Properties | Sort-Object Value -Descending

    foreach ($mode in $sortedUsage) {
        $percentage = [math]::Round(($mode.Value / $total) * 100, 1)
        $bar = "#" * [math]::Ceiling($percentage / 2)
        Write-Host "  $($mode.Name.PadRight(15)): " -NoNewline -ForegroundColor Cyan
        Write-Host "$bar " -NoNewline -ForegroundColor Blue
        Write-Host "$percentage% ($($mode.Value) switches)" -ForegroundColor Gray
    }
    Write-Host ""

    # Switch reasons
    $reasons = @{}
    foreach ($switch in $state.history) {
        $r = $switch.reason
        if ($reasons.ContainsKey($r)) {
            $reasons[$r]++
        } else {
            $reasons[$r] = 1
        }
    }

    Write-Host "Switch Triggers:" -ForegroundColor Yellow
    $sortedReasons = $reasons.GetEnumerator() | Sort-Object Value -Descending
    foreach ($reason in $sortedReasons) {
        Write-Host "  $($reason.Key): $($reason.Value)" -ForegroundColor Gray
    }
    Write-Host ""

    # Time-based analysis
    $recentSwitches = $state.history | Select-Object -Last 10
    if ($recentSwitches.Count -gt 1) {
        $timestamps = $recentSwitches | ForEach-Object { [DateTime]::Parse($_.timestamp) }
        $intervals = @()
        for ($i = 1; $i -lt $timestamps.Count; $i++) {
            $interval = ($timestamps[$i] - $timestamps[$i-1]).TotalMinutes
            $intervals += $interval
        }

        $avgInterval = [math]::Round(($intervals | Measure-Object -Average).Average, 1)
        Write-Host "Average Time Between Switches: $avgInterval minutes" -ForegroundColor Yellow
        Write-Host ""
    }
}

function Record-Switch {
    param(
        [string]$ToMode,
        [string]$Reason
    )

    $fromMode = $state.currentMode
    Add-SwitchHistory -FromMode $fromMode -ToMode $ToMode -Reason $Reason

    Write-Host ""
    Write-Host "[OK] Context switched: $fromMode -> $ToMode" -ForegroundColor Green
    Write-Host "   Reason: $Reason" -ForegroundColor Gray
    Write-Host ""
}

function Clear-History {
    Write-Host ""
    Write-Host "[WARNING] Clear Context History?" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "This will:" -ForegroundColor Gray
    Write-Host "  - Delete all switch history" -ForegroundColor Gray
    Write-Host "  - Reset statistics" -ForegroundColor Gray
    Write-Host "  - Keep current mode: $($state.currentMode)" -ForegroundColor Gray
    Write-Host ""

    $confirm = Read-Host "Type 'yes' to confirm"

    if ($confirm -eq "yes") {
        $state.history = @()
        $state.statistics.totalSwitches = 0
        $state.statistics.modeUsage = @{}
        Update-State

        Write-Host ""
        Write-Host "[OK] History cleared" -ForegroundColor Green
        Write-Host ""
    } else {
        Write-Host ""
        Write-Host "[CANCELLED]" -ForegroundColor Red
        Write-Host ""
    }
}

# Main execution
switch ($Action.ToLower()) {
    "status" {
        Show-Status
    }
    "analytics" {
        Show-Analytics
    }
    "switch" {
        if (-not $Mode) {
            Write-Host "[ERROR] Mode required for switch action" -ForegroundColor Red
            Write-Host "Usage: -Action switch -Mode [mode-name] -Reason [reason]" -ForegroundColor Yellow
        } else {
            $switchReason = if ($Reason) { $Reason } else { "Manual switch" }
            Record-Switch -ToMode $Mode -Reason $switchReason
        }
    }
    "clear" {
        Clear-History
    }
    "export" {
        $exportFile = ".claude/context-history-export-$(Get-Date -Format 'yyyy-MM-dd-HHmmss').json"
        $state | ConvertTo-Json -Depth 10 | Set-Content $exportFile
        Write-Host ""
        Write-Host "[OK] Exported to: $exportFile" -ForegroundColor Green
        Write-Host ""
    }
    default {
        Write-Host "[ERROR] Unknown action: $Action" -ForegroundColor Red
        Write-Host "Available actions: status, analytics, switch, clear, export" -ForegroundColor Yellow
    }
}
