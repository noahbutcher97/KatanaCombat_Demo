# Get list of files relevant to current context mode
# Used by diagnostics and other tools to focus on relevant files

param(
    [Parameter(Mandatory=$false)]
    [string]$ContextMode = "full",

    [Parameter(Mandatory=$false)]
    [int]$Limit = 50
)

$contextConfigPath = ".claude/context-modes/$ContextMode.json"

if (-not (Test-Path $contextConfigPath)) {
    Write-Error "Context mode '$ContextMode' not found at $contextConfigPath"
    exit 1
}

$contextConfig = Get-Content $contextConfigPath | ConvertFrom-Json

# Get include patterns
$includePatterns = $contextConfig.includePatterns
$excludePatterns = $contextConfig.excludePatterns

# Convert glob patterns to PowerShell-compatible wildcards
function Convert-GlobToWildcard {
    param([string]$Pattern)

    $wildcard = $Pattern `
        -replace '\*\*/','**\' `
        -replace '/','\'

    return $wildcard
}

# Find files matching include patterns
$matchedFiles = @()

foreach ($pattern in $includePatterns) {
    $wildcard = Convert-GlobToWildcard -Pattern $pattern

    # Use Get-ChildItem with -Recurse for ** patterns
    if ($wildcard -match '\*\*') {
        $basePath = $wildcard -replace '\*\*.*',''
        $filePattern = $wildcard -replace '.*\*\*\\',''

        if (Test-Path $basePath) {
            $files = Get-ChildItem -Path $basePath -Filter $filePattern -Recurse -File -ErrorAction SilentlyContinue
            $matchedFiles += $files.FullName
        }
    }
    else {
        if (Test-Path $wildcard) {
            $matchedFiles += (Get-Item $wildcard).FullName
        }
    }
}

# Apply exclude patterns
$filteredFiles = $matchedFiles | Where-Object {
    $file = $_
    $shouldExclude = $false

    foreach ($excludePattern in $excludePatterns) {
        $excludeWildcard = Convert-GlobToWildcard -Pattern $excludePattern
        if ($file -like "*$excludeWildcard*") {
            $shouldExclude = $true
            break
        }
    }

    -not $shouldExclude
}

# Remove duplicates and sort
$uniqueFiles = $filteredFiles | Select-Object -Unique | Sort-Object

# Limit results
if ($Limit -gt 0) {
    $uniqueFiles = $uniqueFiles | Select-Object -First $Limit
}

# Output as JSON for easy parsing
$output = @{
    contextMode = $ContextMode
    totalFiles = $uniqueFiles.Count
    files = $uniqueFiles
    includePatterns = $includePatterns
    excludePatterns = $excludePatterns
}

$output | ConvertTo-Json -Depth 10