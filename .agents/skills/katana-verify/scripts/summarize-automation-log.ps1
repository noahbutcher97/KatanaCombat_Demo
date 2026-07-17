param(
    [string]$LogPath = "Saved/Logs/KatanaCombat.log"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $LogPath)) {
    Write-Error "Log not found: $LogPath"
    exit 1
}

$Completed = Select-String -Path $LogPath -Pattern "Test Completed.*Result=" -ErrorAction SilentlyContinue
$NonSuccessResults = @($Completed | Where-Object { $_.Line -notmatch "Result=\{Success\}" })
$AutomationErrors = @(Select-String -Path $LogPath -Pattern "LogAutomation(CommandLine|Controller|Worker): Error:" -ErrorAction SilentlyContinue)
$Failures = @($NonSuccessResults) + @($AutomationErrors)
$Warnings = Select-String -Path $LogPath -Pattern "Warning:.*Automation|Automation.*Warning" -ErrorAction SilentlyContinue
$Discovery = Select-String -Path $LogPath -Pattern "Found [0-9]+ automation tests based on" -ErrorAction SilentlyContinue
$SuccessMarkers = Select-String -Path $LogPath -Pattern "\*\*\*\* TEST COMPLETE\. EXIT CODE: 0 \*\*\*\*" -ErrorAction SilentlyContinue
$DiscoveredCount = 0
if (@($Discovery).Count -gt 0 -and $Discovery[-1].Line -match "Found ([0-9]+) automation tests based on") {
    $DiscoveredCount = [int]$Matches[1]
}
$CompletedCount = @($Completed).Count
$HasNonEmptyMatchingRun = $DiscoveredCount -gt 0 -and $CompletedCount -eq $DiscoveredCount
$HasSuccessMarker = @($SuccessMarkers).Count -gt 0

$Summary = [ordered]@{
    log_path = (Resolve-Path -LiteralPath $LogPath).Path
    discovered_count = $DiscoveredCount
    completed_count = $CompletedCount
    failure_or_error_count = @($Failures).Count
    automation_warning_count = @($Warnings).Count
    discovery_matches_completion = $HasNonEmptyMatchingRun
    has_success_exit_marker = $HasSuccessMarker
    verified_success = $HasNonEmptyMatchingRun -and $HasSuccessMarker -and @($Failures).Count -eq 0
    latest_completed = @($Completed | Select-Object -Last 10 | ForEach-Object { $_.Line })
    latest_failures_or_errors = @($Failures | Select-Object -Last 20 | ForEach-Object { $_.Line })
}

$Summary | ConvertTo-Json -Depth 4

if (@($Failures).Count -gt 0) {
    exit 2
}

if (-not $HasNonEmptyMatchingRun -or -not $HasSuccessMarker) {
    exit 3
}

exit 0
