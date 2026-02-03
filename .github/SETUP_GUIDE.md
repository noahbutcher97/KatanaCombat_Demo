# GitHub Actions Setup Guide for KatanaCombat

This guide provides step-by-step instructions for setting up the CI/CD pipeline for the KatanaCombat project.

## Table of Contents
1. [Prerequisites](#prerequisites)
2. [Self-Hosted Runner Setup](#self-hosted-runner-setup)
3. [Secrets Configuration](#secrets-configuration)
4. [Workflow Configuration](#workflow-configuration)
5. [Testing the Pipeline](#testing-the-pipeline)
6. [Maintenance](#maintenance)

---

## Prerequisites

### Required Software

#### On the Build Machine (Self-Hosted Runner):

1. **Windows OS**
   - Windows Server 2019/2022 (recommended)
   - Or Windows 10/11 Pro

2. **Unreal Engine 5.6**
   - Download from: https://www.unrealengine.com/download
   - Install to: `C:\Program Files\Epic Games\UE_5.6`
   - Ensure all prerequisites are installed (Visual Studio, .NET, etc.)

3. **Visual Studio 2022**
   - Install the "Game Development with C++" workload
   - Include Windows 10/11 SDK
   - Include .NET Framework 4.8 SDK

4. **Git**
   - Download from: https://git-scm.com/download/win
   - Install with Git LFS support

5. **PowerShell 7** (optional but recommended)
   - Download from: https://github.com/PowerShell/PowerShell

### Minimum Hardware Requirements

- **CPU**: 8-core processor (16+ cores recommended)
- **RAM**: 16 GB (32 GB recommended)
- **Storage**: 
  - 150 GB for Unreal Engine installation
  - 50 GB for build artifacts and cache
  - SSD strongly recommended for build performance
- **Network**: Stable internet connection (for artifact uploads)

---

## Self-Hosted Runner Setup

### Step 1: Prepare the Build Machine

1. **Install Unreal Engine 5.6**
   ```powershell
   # Verify installation
   $uePath = "C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor.exe"
   if (Test-Path $uePath) {
       Write-Host "✅ Unreal Engine 5.6 is installed"
       & $uePath -version
   } else {
       Write-Host "❌ Unreal Engine 5.6 not found"
   }
   ```

2. **Verify Visual Studio Installation**
   ```powershell
   # Check for Visual Studio 2022
   $vsPath = "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\devenv.exe"
   if (Test-Path $vsPath) {
       Write-Host "✅ Visual Studio 2022 is installed"
   }
   ```

3. **Test Build Locally**
   ```powershell
   # Navigate to project directory
   cd "C:\YourPath\KatanaCombat_Demo"
   
   # Generate project files
   & "C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\GenerateProjectFiles.bat" `
       -projectfiles -project="$pwd\KatanaCombat.uproject" -game -rocket -progress
   
   # Test build
   & "C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat" `
       KatanaCombatEditor Win64 Development "$pwd\KatanaCombat.uproject" -WaitMutex
   ```

### Step 2: Register the Self-Hosted Runner

1. **Navigate to GitHub Repository Settings**
   - Go to: `https://github.com/noahbutcher97/KatanaCombat_Demo/settings/actions/runners`
   - Click "New self-hosted runner"
   - Select "Windows" as the operating system

2. **Follow GitHub's Installation Instructions**
   ```powershell
   # Example installation commands (use the actual commands from GitHub)
   
   # Create a folder
   mkdir actions-runner; cd actions-runner
   
   # Download the latest runner package
   Invoke-WebRequest -Uri https://github.com/actions/runner/releases/download/v2.311.0/actions-runner-win-x64-2.311.0.zip -OutFile actions-runner-win-x64-2.311.0.zip
   
   # Extract the installer
   Add-Type -AssemblyName System.IO.Compression.FileSystem ; 
   [System.IO.Compression.ZipFile]::ExtractToDirectory("$PWD/actions-runner-win-x64-2.311.0.zip", "$PWD")
   
   # Configure the runner WITH REQUIRED LABELS
   # IMPORTANT: The workflow requires the 'ue5' label to match jobs to this runner
   ./config.cmd --url https://github.com/noahbutcher97/KatanaCombat_Demo --token YOUR_TOKEN --labels self-hosted,Windows,ue5
   ```
   
   > **⚠️ CRITICAL:** You **MUST** include the `--labels self-hosted,Windows,ue5` parameter.
   > The workflow specifically looks for runners with the `ue5` label. Without it, your runner
   > will not pick up build jobs and they will timeout waiting for a matching runner.

3. **Configure as a Service (Recommended)**
   ```powershell
   # Install as a Windows service
   ./svc.cmd install
   
   # Start the service
   ./svc.cmd start
   
   # Verify service status
   ./svc.cmd status
   ```

### Step 3: Verify Runner Registration

1. Return to GitHub repository settings
2. Navigate to: Settings → Actions → Runners
3. Verify your runner shows as "Idle" (green)
4. **IMPORTANT:** Click on your runner name and verify it has these labels:
   - `self-hosted`
   - `Windows`
   - `ue5` ← **This is critical!**
   
   If the `ue5` label is missing, you need to reconfigure your runner:
   ```powershell
   cd C:\actions-runner
   ./config.cmd remove --token YOUR_REMOVAL_TOKEN
   ./config.cmd --url https://github.com/noahbutcher97/KatanaCombat_Demo --token YOUR_TOKEN --labels self-hosted,Windows,ue5
   ./svc.cmd install
   ./svc.cmd start
   ```

---

## Secrets Configuration

### Step 1: Navigate to Secrets Settings

1. Go to: `https://github.com/noahbutcher97/KatanaCombat_Demo/settings/secrets/actions`
2. Click "New repository secret"

### Step 2: Add Required Secrets (Optional for Basic Setup)

The current workflow doesn't require secrets, but you may want to add these for future enhancements:

#### Epic Games Credentials (if auto-installing UE)
```
Name: EPIC_GAMES_USERNAME
Value: your-epic-games-email@example.com

Name: EPIC_GAMES_PASSWORD
Value: your-epic-games-password
```

#### Code Signing Certificate (for release builds)
```
Name: CODE_SIGNING_CERTIFICATE
Value: <base64-encoded-certificate>

Name: CODE_SIGNING_PASSWORD
Value: your-certificate-password
```

To encode a certificate:
```powershell
# Convert PFX to Base64
$certPath = "C:\Path\To\Certificate.pfx"
$certBytes = [System.IO.File]::ReadAllBytes($certPath)
$certBase64 = [System.Convert]::ToBase64String($certBytes)
Write-Host $certBase64
```

#### Deployment Secrets (for Steam, Epic Games Store)
```
Name: STEAM_USERNAME
Value: your-steam-username

Name: STEAM_PASSWORD
Value: your-steam-password

Name: STEAM_APP_ID
Value: 1234567
```

### Step 3: Using Secrets in Workflows

Access secrets in the workflow file:

```yaml
- name: Example Step Using Secret
  env:
    EPIC_USERNAME: ${{ secrets.EPIC_GAMES_USERNAME }}
    EPIC_PASSWORD: ${{ secrets.EPIC_GAMES_PASSWORD }}
  run: |
    # Use the secrets here
```

---

## Workflow Configuration

### Step 1: Review Workflow File

The workflow is located at: `.github/workflows/ue5-ci.yml`

Key configuration variables at the top:
```yaml
env:
  UE_VERSION: '5.6'
  PROJECT_NAME: 'KatanaCombat'
  PLATFORM: 'Win64'
  CONFIGURATION: 'Development Editor'
```

### Step 2: Customize for Your Environment

#### If UE is installed in a different location:

Edit the workflow file to update the UE path:
```yaml
- name: Setup Unreal Engine 5.6
  shell: pwsh
  run: |
    # Update this path if your UE installation differs
    $ueInstallPath = "D:\UnrealEngine\UE_5.6"  # Example: different drive
```

#### To target different configurations:

```yaml
env:
  CONFIGURATION: 'Development'  # or 'Shipping', 'DebugGame'
  PLATFORM: 'Win64'             # or 'Linux', 'Mac'
```

### Step 3: Adjust Trigger Conditions

Current triggers:
```yaml
on:
  push:
    branches: [ main, develop ]
  pull_request:
    branches: [ main, develop ]
  workflow_dispatch:
```

To add more branches:
```yaml
on:
  push:
    branches: [ main, develop, feature/*, release/* ]
```

To run on a schedule:
```yaml
on:
  schedule:
    - cron: '0 0 * * *'  # Daily at midnight UTC
```

---

## Testing the Pipeline

### Step 1: Manual Workflow Run

1. Go to: `https://github.com/noahbutcher97/KatanaCombat_Demo/actions`
2. Select "UE5 CI - Build, Test, and Analyze"
3. Click "Run workflow"
4. Select branch (e.g., `main`)
5. Click "Run workflow"

### Step 2: Monitor the Workflow

1. Watch the workflow progress in real-time
2. Check each step for success/failure
3. Review logs if any step fails

### Step 3: Verify Artifacts

After successful run:
1. Scroll to the bottom of the workflow run page
2. Download artifacts:
   - `build-logs-<sha>`
   - `test-results-<sha>`
   - `static-analysis-<sha>`
   - `compiled-binaries-<sha>`

### Step 4: Test with a Pull Request

1. Create a new branch:
   ```bash
   git checkout -b test/ci-validation
   ```

2. Make a small change (e.g., add a comment):
   ```bash
   echo "// CI test" >> Source/KatanaCombat/Public/Core/CombatComponent.h
   git add .
   git commit -m "Test: Validate CI pipeline"
   git push origin test/ci-validation
   ```

3. Create a pull request on GitHub

4. Verify that the CI workflow runs automatically

---

## Maintenance

### Monitoring Runner Health

#### Check Runner Status
```powershell
# On the runner machine
cd C:\actions-runner
./svc.cmd status
```

#### View Runner Logs
```powershell
# Service logs
Get-Content "C:\actions-runner\_diag\*.log" -Tail 100
```

### Updating the Runner

```powershell
# Stop the service
./svc.cmd stop

# Download new version
Invoke-WebRequest -Uri https://github.com/actions/runner/releases/download/vX.X.X/actions-runner-win-x64-X.X.X.zip -OutFile actions-runner-new.zip

# Extract and replace files
# (Follow GitHub's update instructions)

# Start the service
./svc.cmd start
```

### Cleaning Build Artifacts

Periodically clean up build artifacts to free disk space:

```powershell
# Clean UE derived data cache
$projectPath = "C:\YourPath\KatanaCombat_Demo"
Remove-Item "$projectPath\DerivedDataCache" -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item "$projectPath\Intermediate" -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item "$projectPath\Saved\Logs" -Recurse -Force -ErrorAction SilentlyContinue

Write-Host "✅ Build artifacts cleaned"
```

### Monitoring Disk Space

```powershell
# Check available disk space
Get-PSDrive -PSProvider FileSystem | Where-Object {$_.Used -gt 0} | 
    Select-Object Name, @{Name="Used(GB)";Expression={[math]::Round($_.Used/1GB,2)}}, 
                       @{Name="Free(GB)";Expression={[math]::Round($_.Free/1GB,2)}}
```

### Troubleshooting Common Issues

#### Issue: Runner Not Picking Up Workflow Jobs

**Symptoms:**
- Runner shows as "Idle" (green) in GitHub
- Workflow jobs show "Waiting for a runner to pick up this job..."
- Jobs eventually timeout after 30 minutes
- Detect job runs but build job never starts

**Root Cause:** Missing `ue5` label on runner configuration

**Solution:**
```powershell
# 1. Check current labels (via GitHub web UI):
#    Go to: Repository Settings → Actions → Runners → Click your runner name
#    Verify labels: Should show 'self-hosted', 'Windows', 'ue5'

# 2. If 'ue5' label is missing, reconfigure the runner:
cd D:\actions-runner  # Or your runner installation path

# Stop and uninstall service
./svc.cmd stop
./svc.cmd uninstall

# Remove configuration
./config.cmd remove --token YOUR_REMOVAL_TOKEN

# Reconfigure with ALL required labels
# Get a new token from: https://github.com/noahbutcher97/KatanaCombat_Demo/settings/actions/runners/new
./config.cmd --url https://github.com/noahbutcher97/KatanaCombat_Demo --token YOUR_NEW_TOKEN --labels self-hosted,Windows,ue5

# Reinstall and restart service
./svc.cmd install
./svc.cmd start

# Verify
./svc.cmd status
```

**Verification:**
1. Go to: `Repository Settings → Actions → Runners`
2. Your runner should show:
   - Status: Idle (green circle)
   - Labels: `self-hosted`, `Windows`, `ue5`
3. Trigger a workflow run to test

#### Issue: Runner Goes Offline

**Possible Causes:**
- Service stopped
- Network connectivity issues
- Machine rebooted

**Solution:**
```powershell
# Restart the service
cd C:\actions-runner
./svc.cmd stop
./svc.cmd start
./svc.cmd status
```

#### Issue: Build Fails with "Out of Memory"

**Solution:**
- Increase runner machine RAM
- Close unnecessary applications
- Enable parallel builds in UE settings

#### Issue: Tests Timeout

**Solution:**
Increase timeout in workflow:
```yaml
- name: Run Automation Tests
  timeout-minutes: 30  # Increase from default 360
```

#### Issue: Artifacts Upload Fails

**Solution:**
- Check network connectivity
- Verify artifact size isn't too large (GitHub limit: 2GB per artifact)
- Compress artifacts before upload

---

## Advanced Configuration

### Using GitHub-Hosted Runners (Limitations Apply)

If you want to use GitHub-hosted runners instead of self-hosted:

1. Change `runs-on` in the workflow:
   ```yaml
   runs-on: windows-latest  # Instead of self-hosted
   ```

2. Add UE installation step (requires Epic Games credentials):
   ```yaml
   - name: Download and Install UE5.6
     run: |
       # Download from Epic Games (requires authentication)
       # This is complex and may exceed GitHub's time limits
   ```

**Note:** GitHub-hosted runners have limitations:
- 6-hour maximum job time
- Limited disk space
- No persistent cache between runs
- UE5 installation time (1-2 hours) counts against job time

### Multi-Platform Builds

To build for multiple platforms, use a matrix strategy:

```yaml
strategy:
  matrix:
    platform: [Win64, Linux, Mac]
    configuration: [Development, Shipping]
```

### Parallel Test Execution

Run tests in parallel to reduce overall time:

```yaml
strategy:
  matrix:
    test-group: [Combat, Targeting, Animation, Integration]
```

---

## Support and Resources

### Documentation
- [KatanaCombat Documentation](../docs/)
- [GitHub Actions Documentation](https://docs.github.com/en/actions)
- [Unreal Engine Build Automation](https://docs.unrealengine.com/en-US/ProductionPipelines/BuildTools/)

### Getting Help
1. Check workflow logs for error messages
2. Review the [Troubleshooting](#troubleshooting-common-issues) section
3. Open an issue in the repository with:
   - Workflow run URL
   - Error logs
   - Runner system information

---

## Checklist

Before going live with the CI/CD pipeline, verify:

- [ ] Self-hosted runner is installed and running
- [ ] Unreal Engine 5.6 is installed on the runner
- [ ] Visual Studio 2022 with C++ workload is installed
- [ ] Runner is registered and shows as "Idle" on GitHub
- [ ] Workflow file is committed to `.github/workflows/ue5-ci.yml`
- [ ] Test workflow runs successfully
- [ ] Artifacts are uploaded and downloadable
- [ ] Tests execute and report results
- [ ] Documentation is reviewed and understood
- [ ] Maintenance schedule is established

---

*Last Updated: 2026-01-31*
