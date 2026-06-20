$ErrorActionPreference = "Stop"

$RawInput = [Console]::In.ReadToEnd()
if ([string]::IsNullOrWhiteSpace($RawInput)) {
    exit 0
}

try {
    $Event = $RawInput | ConvertFrom-Json -ErrorAction Stop
} catch {
    exit 0
}

if ($env:KATANA_ALLOW_DESTRUCTIVE -eq "1") {
    exit 0
}

$ToolInput = $Event.tool_input
$Command = ""

if ($null -ne $ToolInput) {
    foreach ($Name in @("command", "cmd", "script")) {
        if ($ToolInput.PSObject.Properties.Name -contains $Name) {
            $Command = [string]$ToolInput.$Name
            break
        }
    }
}

if ([string]::IsNullOrWhiteSpace($Command)) {
    exit 0
}

$Normalized = $Command -replace "\\", "/"

$BlockedPatterns = @(
    @{
        Pattern = "(?i)\bgit\s+reset\s+--hard\b"
        Reason = "git reset --hard can destroy user WIP in this asset-heavy workspace."
    },
    @{
        Pattern = "(?i)\bgit\s+checkout\s+--\b"
        Reason = "git checkout -- can silently revert user changes."
    },
    @{
        Pattern = "(?i)\bgit\s+clean\b"
        Reason = "git clean can delete untracked Unreal assets and generated evidence."
    },
    @{
        Pattern = "(?i)\bRemove-Item\b.*\s-Recurse\b"
        Reason = "recursive Remove-Item requires explicit user direction and path review."
    },
    @{
        Pattern = "(?i)\brm\s+-rf\b"
        Reason = "recursive force deletion requires explicit user direction and path review."
    }
)

foreach ($Rule in $BlockedPatterns) {
    if ($Normalized -match $Rule.Pattern) {
        [Console]::Error.WriteLine("Blocked by KatanaCombat Codex safety hook: $($Rule.Reason) Set KATANA_ALLOW_DESTRUCTIVE=1 only for an explicitly approved maintenance command.")
        exit 2
    }
}

$DeletesAssetContent =
    ($Normalized -match "(?i)\b(Remove-Item|del|erase|rm)\b") -and
    ($Normalized -match "(?i)(^|[ /])Content/")

if ($DeletesAssetContent) {
    [Console]::Error.WriteLine("Blocked by KatanaCombat Codex safety hook: deleting under Content/ requires explicit user approval and path-by-path review.")
    exit 2
}

exit 0
