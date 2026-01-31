# UE5 Setup Script for GitHub-Hosted Runners
# This script automates the installation of Unreal Engine 5.6 on clean Windows environments

param(
    [string]$InstallPath = "C:\UE5\UE_5.6",
    [string]$EpicAPIKey = $env:EPIC_API_KEY,
    [string]$SourceURL = "",
    [switch]$SkipValidation = $false
)

$ErrorActionPreference = "Stop"

Write-Host "======================================"
Write-Host "UE5.6 Installation Script"
Write-Host "======================================"
Write-Host ""

# Function to display progress
function Write-Progress-Message {
    param([string]$Message)
    Write-Host "[$(Get-Date -Format 'HH:mm:ss')] $Message" -ForegroundColor Cyan
}

# Function to check if UE5 is already installed
function Test-UE5Installation {
    param([string]$Path)
    
    $buildBat = Join-Path $Path "Engine\Build\BatchFiles\Build.bat"
    $editor = Join-Path $Path "Engine\Binaries\Win64\UnrealEditor.exe"
    
    return (Test-Path $buildBat) -and (Test-Path $editor)
}

# Check if already installed
if (Test-UE5Installation -Path $InstallPath) {
    Write-Host "✅ UE5 already installed at: $InstallPath" -ForegroundColor Green
    exit 0
}

Write-Progress-Message "Creating installation directory..."
New-Item -Path $InstallPath -ItemType Directory -Force | Out-Null

# Check available installation methods
$installationMethod = $null

if ($SourceURL) {
    Write-Progress-Message "Using custom source URL: $SourceURL"
    $installationMethod = "custom-url"
}
elseif ($EpicAPIKey) {
    Write-Progress-Message "Using Epic Games API for installation"
    $installationMethod = "epic-api"
}
else {
    Write-Host "❌ No installation method available" -ForegroundColor Red
    Write-Host ""
    Write-Host "To install UE5 on GitHub-hosted runners, you need one of the following:" -ForegroundColor Yellow
    Write-Host "  1. Set EPIC_API_KEY secret in repository settings"
    Write-Host "  2. Provide a custom source URL with -SourceURL parameter"
    Write-Host "  3. Pre-cache UE5 in GitHub Actions cache"
    Write-Host "  4. Use self-hosted runners with UE5 pre-installed"
    Write-Host ""
    Write-Host "For self-hosted runner setup, see: docs/SETUP_GUIDE.md"
    exit 1
}

# Installation based on method
switch ($installationMethod) {
    "custom-url" {
        Write-Progress-Message "Downloading UE5 from custom URL..."
        $archivePath = Join-Path $env:TEMP "ue5.zip"
        
        try {
            Invoke-WebRequest -Uri $SourceURL -OutFile $archivePath -UseBasicParsing
            Write-Progress-Message "Download complete"
        }
        catch {
            Write-Host "❌ Failed to download UE5 from URL: $_" -ForegroundColor Red
            exit 1
        }
        
        Write-Progress-Message "Extracting UE5 archive..."
        try {
            Expand-Archive -Path $archivePath -DestinationPath $InstallPath -Force
            Write-Progress-Message "Extraction complete"
        }
        catch {
            Write-Host "❌ Failed to extract UE5 archive: $_" -ForegroundColor Red
            exit 1
        }
        
        Remove-Item $archivePath -Force
    }
    
    "epic-api" {
        Write-Progress-Message "Installing via Epic Games API..."
        Write-Host "Note: Epic Games API installation is not yet fully implemented" -ForegroundColor Yellow
        Write-Host "This would typically use Epic's launcher CLI or API endpoints" -ForegroundColor Yellow
        Write-Host ""
        Write-Host "Recommended approach:" -ForegroundColor Yellow
        Write-Host "  1. Build UE5 from source in a separate job"
        Write-Host "  2. Cache the built UE5 installation"
        Write-Host "  3. Restore from cache in subsequent builds"
        Write-Host ""
        Write-Host "For now, please use self-hosted runners or provide a custom source URL"
        exit 1
    }
}

# Validate installation
if (-not $SkipValidation) {
    Write-Progress-Message "Validating UE5 installation..."
    
    if (Test-UE5Installation -Path $InstallPath) {
        Write-Host "✅ UE5 installation validated successfully" -ForegroundColor Green
    }
    else {
        Write-Host "❌ UE5 installation validation failed" -ForegroundColor Red
        Write-Host "Required files not found at: $InstallPath" -ForegroundColor Red
        exit 1
    }
}

# Set environment variables
Write-Progress-Message "Setting environment variables..."
[System.Environment]::SetEnvironmentVariable("UE5_PATH", $InstallPath, "Process")
[System.Environment]::SetEnvironmentVariable("UE5_BUILD_BAT", (Join-Path $InstallPath "Engine\Build\BatchFiles\Build.bat"), "Process")
[System.Environment]::SetEnvironmentVariable("UE5_RUNUAT", (Join-Path $InstallPath "Engine\Build\BatchFiles\RunUAT.bat"), "Process")
[System.Environment]::SetEnvironmentVariable("UE5_EDITOR", (Join-Path $InstallPath "Engine\Binaries\Win64\UnrealEditor.exe"), "Process")

Write-Host ""
Write-Host "======================================"
Write-Host "✅ UE5 Setup Complete!" -ForegroundColor Green
Write-Host "======================================"
Write-Host "Installation Path: $InstallPath"
Write-Host "Build Tool: $env:UE5_BUILD_BAT"
Write-Host "Editor: $env:UE5_EDITOR"
Write-Host ""
