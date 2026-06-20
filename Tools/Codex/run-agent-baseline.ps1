param(
    [string]$ProjectRoot = "",
    [string]$UnrealRoot = "C:\Program Files\Epic Games\UE_5.6"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Resolve-RepoRoot {
    if ($ProjectRoot) {
        return (Resolve-Path -LiteralPath $ProjectRoot).Path
    }

    return (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..\..")).Path
}

function Assert-PathExists {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Label
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        throw "$Label not found: $Path"
    }
}

function Invoke-LoggedProcess {
    param(
        [Parameter(Mandatory = $true)][string]$Label,
        [Parameter(Mandatory = $true)][string]$FilePath,
        [Parameter(Mandatory = $true)][string[]]$Arguments,
        [Parameter(Mandatory = $true)][string]$WorkingDirectory,
        [Parameter(Mandatory = $true)][string]$StdOutPath,
        [Parameter(Mandatory = $true)][string]$StdErrPath
    )

    Write-Host "==> $Label"
    Write-Host "    stdout: $StdOutPath"
    Write-Host "    stderr: $StdErrPath"

    Push-Location -LiteralPath $WorkingDirectory
    try {
        $global:LASTEXITCODE = $null
        & $FilePath @Arguments > $StdOutPath 2> $StdErrPath
        if ($null -ne $LASTEXITCODE) {
            $exitCode = [int]$LASTEXITCODE
        }
        elseif ($?) {
            $exitCode = 0
        }
        else {
            $exitCode = 1
        }
    }
    finally {
        Pop-Location
    }

    Write-Host "    exit: $exitCode"
    return $exitCode
}

$Root = Resolve-RepoRoot
$ProjectFile = Join-Path $Root "KatanaCombat.uproject"
$BuildBat = Join-Path $UnrealRoot "Engine\Build\BatchFiles\Build.bat"
$EditorCmd = Join-Path $UnrealRoot "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$SummaryScript = Join-Path $Root ".agents\skills\katana-verify\scripts\summarize-automation-log.ps1"
$LogDir = Join-Path $Root "Saved\Logs"
$Stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$Prefix = Join-Path $LogDir "Codex-Agent-Baseline-$Stamp"

Assert-PathExists -Path $ProjectFile -Label "Project file"
Assert-PathExists -Path $BuildBat -Label "Unreal build script"
Assert-PathExists -Path $EditorCmd -Label "UnrealEditor-Cmd"
Assert-PathExists -Path $SummaryScript -Label "Automation summary script"

New-Item -ItemType Directory -Force -Path $LogDir | Out-Null

$BuildOut = "$Prefix-build.out.log"
$BuildErr = "$Prefix-build.err.log"
$AutomationOut = "$Prefix-automation.out.log"
$AutomationErr = "$Prefix-automation.err.log"
$AutomationExit = "$Prefix-automation.exitcode.txt"
$SummaryJson = "$Prefix-automation-summary.json"

$BuildArgs = @(
    "KatanaCombatEditor",
    "Win64",
    "Development",
    "-Project=$ProjectFile",
    "-Progress",
    "-NoHotReload"
)

$BuildCode = Invoke-LoggedProcess `
    -Label "Build KatanaCombatEditor Win64 Development" `
    -FilePath $BuildBat `
    -Arguments $BuildArgs `
    -WorkingDirectory $Root `
    -StdOutPath $BuildOut `
    -StdErrPath $BuildErr

if ($BuildCode -ne 0) {
    Write-Error "Build failed with exit code $BuildCode. See $BuildOut and $BuildErr."
    exit $BuildCode
}

$AutomationArgs = @(
    $ProjectFile,
    "-ExecCmds=Automation RunTests KatanaCombat;Quit",
    "-NullRHI",
    "-NoSplash",
    "-Unattended",
    "-nopause",
    "-stdout"
)

$AutomationCode = Invoke-LoggedProcess `
    -Label "Run KatanaCombat automation" `
    -FilePath $EditorCmd `
    -Arguments $AutomationArgs `
    -WorkingDirectory $Root `
    -StdOutPath $AutomationOut `
    -StdErrPath $AutomationErr

Set-Content -LiteralPath $AutomationExit -Value $AutomationCode -Encoding ASCII

$summaryOutput = & powershell -NoProfile -ExecutionPolicy Bypass -File $SummaryScript -LogPath (Join-Path $LogDir "KatanaCombat.log")
$summaryCode = $LASTEXITCODE
$summaryOutput | Set-Content -LiteralPath $SummaryJson -Encoding UTF8
$summary = $summaryOutput | ConvertFrom-Json

Write-Host ""
Write-Host "==> Baseline summary"
Write-Host "    completed: $($summary.completed_count)"
Write-Host "    failures/errors: $($summary.failure_or_error_count)"
Write-Host "    automation warnings: $($summary.automation_warning_count)"
Write-Host "    summary: $SummaryJson"
Write-Host "    automation exit: $AutomationCode"

if ($AutomationCode -ne 0) {
    Write-Error "Automation command failed with exit code $AutomationCode. See $AutomationOut and $AutomationErr."
    exit $AutomationCode
}

if ($summaryCode -ne 0 -or $summary.failure_or_error_count -gt 0) {
    Write-Error "Automation summary reported failures/errors. See $SummaryJson."
    exit 2
}

Write-Host ""
Write-Host "BASELINE GREEN"
exit 0
