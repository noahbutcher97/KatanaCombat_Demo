param(
    [string]$LogPath = "Saved/Logs/KatanaCombat.log"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $LogPath)) {
    Write-Error "Log not found: $LogPath"
    exit 1
}

$Completed = Select-String -Path $LogPath -Pattern "Test Completed.*Result=" -ErrorAction SilentlyContinue
$Failures = Select-String -Path $LogPath -Pattern "Test Completed\. Result=\{(Fail|Failed|Error|Cancelled|Timeout)\}|LogAutomation(CommandLine|Controller|Worker): Error:" -ErrorAction SilentlyContinue
$Warnings = Select-String -Path $LogPath -Pattern "Warning:.*Automation|Automation.*Warning" -ErrorAction SilentlyContinue

$Summary = [ordered]@{
    log_path = (Resolve-Path -LiteralPath $LogPath).Path
    completed_count = @($Completed).Count
    failure_or_error_count = @($Failures).Count
    automation_warning_count = @($Warnings).Count
    latest_completed = @($Completed | Select-Object -Last 10 | ForEach-Object { $_.Line })
    latest_failures_or_errors = @($Failures | Select-Object -Last 20 | ForEach-Object { $_.Line })
}

$Summary | ConvertTo-Json -Depth 4

if (@($Failures).Count -gt 0) {
    exit 2
}

exit 0
