# Self-Hosted Runner Setup Guide

This guide provides detailed instructions for setting up self-hosted GitHub Actions runners for the KatanaCombat UE5 project.

## Table of Contents
- [Prerequisites](#prerequisites)
- [Runner Installation](#runner-installation)
- [UE5 Configuration](#ue5-configuration)
- [Runner Configuration](#runner-configuration)
- [Testing](#testing)
- [Troubleshooting](#troubleshooting)

---

## Prerequisites

### Hardware Requirements
- **OS**: Windows 10/11 (64-bit) or Windows Server 2019/2022
- **CPU**: 8+ cores recommended (AMD Ryzen 7/Intel i7 or better)
- **RAM**: 32GB minimum, 64GB recommended
- **Storage**: 
  - 200GB+ free space for UE5 installation
  - 100GB+ free space for project builds
  - SSD strongly recommended for build performance
- **GPU**: Not required for headless builds, but useful for asset validation

### Software Requirements
- **Windows SDK**: Windows 11 SDK (10.0.22621.0 or later)
- **Visual Studio 2022**: 
  - Workload: Desktop development with C++
  - Workload: Game development with C++
  - Component: MSVC v143 - VS 2022 C++ x64/x86 build tools (v14.38 or later)
  - Component: C++ ATL for latest v143 build tools
  - Component: .NET Framework 4.6.2 targeting pack
- **Unreal Engine 5.6**: Full installation via Epic Games Launcher
- **Git**: Git for Windows with Git LFS enabled
- **PowerShell**: PowerShell 5.1 or later (included with Windows)

---

## Runner Installation

### Step 1: Install Visual Studio 2022

1. Download Visual Studio 2022 Community/Professional from [visualstudio.microsoft.com](https://visualstudio.microsoft.com/)

2. Run the installer and select the following workloads:
   - ✅ Desktop development with C++
   - ✅ Game development with C++

3. In the **Individual Components** tab, ensure these are selected:
   - ✅ MSVC v143 - VS 2022 C++ x64/x86 build tools (Latest)
   - ✅ C++ ATL for latest v143 build tools (x86 & x64)
   - ✅ Windows 11 SDK (10.0.22621.0)
   - ✅ .NET Framework 4.6.2 targeting pack

4. Complete the installation (this may take 30-60 minutes)

### Step 2: Install Unreal Engine 5.6

1. Install **Epic Games Launcher** from [epicgames.com](https://www.epicgames.com/store/en-US/download)

2. Sign in with your Epic Games account

3. Navigate to **Unreal Engine** → **Library**

4. Click the **+** icon next to **Engine Versions**

5. Select **5.6** from the dropdown and click **Install**

6. Choose installation location (default: `C:\Program Files\Epic Games\UE_5.6`)
   - Note: Ensure you have at least 200GB free space

7. Wait for installation to complete (this can take 1-2 hours depending on internet speed)

8. Verify installation:
   ```powershell
   Test-Path "C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat"
   ```
   Should return `True`

### Step 3: Install Git with LFS

1. Download Git for Windows from [git-scm.com](https://git-scm.com/download/win)

2. Run the installer with default options

3. Open PowerShell and configure Git LFS:
   ```powershell
   git lfs install
   ```

4. Verify installation:
   ```powershell
   git --version
   git lfs version
   ```

### Step 4: Configure GitHub Actions Runner

1. Navigate to your GitHub repository: `https://github.com/noahbutcher97/KatanaCombat_Demo`

2. Go to **Settings** → **Actions** → **Runners**

3. Click **New self-hosted runner**

4. Select **Windows** as the operating system

5. Follow the displayed commands in PowerShell (as Administrator):

   ```powershell
   # Create a folder
   mkdir actions-runner; cd actions-runner
   
   # Download the latest runner package
   Invoke-WebRequest -Uri https://github.com/actions/runner/releases/download/v2.311.0/actions-runner-win-x64-2.311.0.zip -OutFile actions-runner-win-x64-2.311.0.zip
   
   # Extract the installer
   Add-Type -AssemblyName System.IO.Compression.FileSystem
   [System.IO.Compression.ZipFile]::ExtractToDirectory("$PWD/actions-runner-win-x64-2.311.0.zip", "$PWD")
   ```

6. Configure the runner:
   ```powershell
   ./config.cmd --url https://github.com/noahbutcher97/KatanaCombat_Demo --token <YOUR_TOKEN>
   ```

7. When prompted for runner name, use a descriptive name like: `windows-ue5-builder-01`

8. When prompted for labels, add these labels (comma-separated):
   ```
   self-hosted,Windows,ue5
   ```

9. When prompted for work folder, use default or specify custom location

### Step 5: Install Runner as a Service

Running the runner as a Windows service ensures it starts automatically and runs in the background:

```powershell
# Install the service (requires Administrator)
./svc.sh install

# Start the service
./svc.sh start

# Check service status
./svc.sh status
```

**Alternative: Run Interactively**

If you prefer to run the runner interactively (useful for debugging):
```powershell
./run.cmd
```

---

## UE5 Configuration

### Verify UE5 Installation Paths

The CI workflow will automatically detect UE5 in these locations (in order):
1. `C:\Program Files\Epic Games\UE_5.6`
2. `C:\UE5\UE_5.6`
3. `D:\Program Files\Epic Games\UE_5.6`

If you installed UE5 in a custom location, you have two options:

**Option A: Create a symbolic link** (recommended)
```powershell
# Run as Administrator
New-Item -ItemType SymbolicLink -Path "C:\Program Files\Epic Games\UE_5.6" -Target "D:\YourCustomPath\UE_5.6"
```

**Option B: Set environment variable**
```powershell
# Set system environment variable
[System.Environment]::SetEnvironmentVariable("UE5_PATH", "D:\YourCustomPath\UE_5.6", "Machine")
```

### Configure Derived Data Cache (DDC)

To speed up builds, configure a shared DDC location:

1. Create a DDC directory:
   ```powershell
   mkdir C:\UE5\SharedDDC
   ```

2. Create or edit `%USERPROFILE%\AppData\Local\UnrealEngine\Engine.ini`:
   ```ini
   [InstalledDerivedDataBackendGraph]
   MinimumDaysToKeepFile=7
   Root=(Type=KeyLength, Length=120, Inner=AsyncPut)
   AsyncPut=(Type=AsyncPut, Inner=Hierarchy)
   Hierarchy=(Type=Hierarchical, Inner=Boot, Inner=Pak, Inner=EnginePak, Inner=Local, Inner=Shared)
   Boot=(Type=Boot, Filename="%GAMEDIR%DerivedDataCache/Boot.ddc", MaxCacheSize=512)
   Local=(Type=FileSystem, ReadOnly=false, Clean=false, Flush=false, PurgeTransient=true, DeleteUnused=true, UnusedFileAge=34, FoldersToClean=-1, Path=%USERPROFILE%/AppData/Local/UnrealEngine/Common/DerivedDataCache)
   Shared=(Type=FileSystem, ReadOnly=false, Clean=false, Flush=false, DeleteUnused=true, UnusedFileAge=10, FoldersToClean=10, MaxFileChecksPerSec=1, Path=C:/UE5/SharedDDC, EnvPathOverride=UE-SharedDataCachePath, EditorOverrideSetting=SharedDerivedDataCache)
   ```

---

## Runner Configuration

### Configure Runner Environment Variables

Set these environment variables for optimal build performance:

```powershell
# Set system environment variables (run as Administrator)
[System.Environment]::SetEnvironmentVariable("UE-SharedDataCachePath", "C:\UE5\SharedDDC", "Machine")
[System.Environment]::SetEnvironmentVariable("MSBUILDDISABLENODEREUSE", "1", "Machine")
```

### Configure Windows Defender Exclusions

Add build directories to Windows Defender exclusions to improve performance:

```powershell
# Run as Administrator
Add-MpPreference -ExclusionPath "C:\Program Files\Epic Games\UE_5.6"
Add-MpPreference -ExclusionPath "C:\actions-runner\_work"
Add-MpPreference -ExclusionPath "C:\UE5\SharedDDC"
```

### Increase Process and Memory Limits

For large projects, you may need to increase system limits:

1. Open **Registry Editor** (regedit.exe)

2. Navigate to: `HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\Session Manager\SubSystems`

3. Find the `Windows` value and locate the `SharedSection=` parameter

4. Change from `SharedSection=1024,20480,768` to `SharedSection=1024,20480,1024`

5. Restart the computer

---

## Testing

### Test Runner Configuration

1. Verify the runner is online:
   - Go to your repository: **Settings** → **Actions** → **Runners**
   - Your runner should show status: **Idle** (green)

2. Trigger a test workflow:
   - Go to **Actions** tab in your repository
   - Select **UE5 CI/CD Pipeline**
   - Click **Run workflow**
   - Select **self-hosted** as the runner type
   - Click **Run workflow**

3. Monitor the workflow execution:
   - Click on the running workflow
   - Watch each step complete
   - Check for any errors in the logs

### Test Build Locally

Before running in CI, test the build locally:

```powershell
cd C:\actions-runner\_work\KatanaCombat_Demo\KatanaCombat_Demo

# Generate project files
& "C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat" -projectfiles -project="$PWD\KatanaCombat.uproject" -game -rocket -progress

# Build the project
& "C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat" KatanaCombatEditor Win64 Development -Project="$PWD\KatanaCombat.uproject" -WaitMutex -FromMsBuild
```

If the local build succeeds, the CI build should work as well.

---

## Troubleshooting

### Runner is Offline

**Problem**: Runner shows as offline in GitHub

**Solutions**:
1. Check if the service is running:
   ```powershell
   ./svc.sh status
   ```

2. Restart the service:
   ```powershell
   ./svc.sh stop
   ./svc.sh start
   ```

3. Check Windows Event Viewer for errors:
   - Open Event Viewer → Windows Logs → Application
   - Look for errors from source "actions.runner"

### Build Fails: "UE5 Installation Not Found"

**Problem**: CI cannot locate UE5 installation

**Solutions**:
1. Verify UE5 is installed:
   ```powershell
   Test-Path "C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat"
   ```

2. Check the runner's PATH:
   ```powershell
   Get-ChildItem Env:\
   ```

3. Manually set UE5_PATH environment variable (see [UE5 Configuration](#ue5-configuration))

### Build Fails: "MSBuild Not Found"

**Problem**: Visual Studio build tools not accessible

**Solutions**:
1. Verify Visual Studio installation:
   ```powershell
   & "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\vswhere.exe" -latest
   ```

2. Repair Visual Studio installation:
   - Open Visual Studio Installer
   - Click **Modify** on VS 2022
   - Click **Repair**

3. Add MSBuild to PATH:
   ```powershell
   [System.Environment]::SetEnvironmentVariable("PATH", $env:PATH + ";C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin", "Machine")
   ```

### Build is Slow

**Problem**: Builds take too long

**Solutions**:
1. Enable shared DDC (see [Configure Derived Data Cache](#configure-derived-data-cache))

2. Increase parallel build jobs in `YourProject.Build.cs`:
   ```csharp
   bUseUnityBuild = true;
   ```

3. Use GitHub Actions cache effectively (already configured in workflow)

4. Exclude unnecessary directories from antivirus scanning (see [Configure Windows Defender Exclusions](#configure-windows-defender-exclusions))

### Insufficient Disk Space

**Problem**: Build fails due to disk space

**Solutions**:
1. Clean old build artifacts:
   ```powershell
   Remove-Item -Path "C:\actions-runner\_work\*\*\Intermediate" -Recurse -Force
   Remove-Item -Path "C:\actions-runner\_work\*\*\Binaries" -Recurse -Force
   ```

2. Configure automatic cleanup in runner config:
   - Edit `.runner` file in runner directory
   - Add: `"cleanupProcesses": true`

3. Move DDC to a larger drive (see [Configure Derived Data Cache](#configure-derived-data-cache))

### Tests Fail in Headless Mode

**Problem**: Automation tests crash with NullRHI

**Solutions**:
1. Ensure test modules are properly configured in `.uproject`

2. Run with additional flags:
   ```cmd
   UnrealEditor.exe "KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat" -unattended -nopause -NullRHI -log -NoSplash -ReportExportPath="Saved\Automation\Reports"
   ```

3. Check test logs in `Saved\Logs\` for specific errors

---

## Performance Optimization

### Recommended Runner Specifications

For optimal performance, configure your runner with:

- **Tier 1** (Small projects):
  - 8 cores, 32GB RAM, 500GB SSD
  - Expected build time: 15-30 minutes

- **Tier 2** (Medium projects) - **Recommended for KatanaCombat**:
  - 16 cores, 64GB RAM, 1TB NVMe SSD
  - Expected build time: 8-15 minutes

- **Tier 3** (Large projects):
  - 32+ cores, 128GB RAM, 2TB NVMe SSD
  - Expected build time: 5-10 minutes

### Build Time Benchmarks

Expected build times for KatanaCombat (Development Editor, Win64):

| Configuration | Cold Build | Incremental Build | Full Rebuild |
|--------------|------------|-------------------|--------------|
| Minimum Spec | 30-45 min  | 5-10 min         | 25-35 min    |
| Recommended  | 15-20 min  | 3-5 min          | 12-18 min    |
| High-End     | 8-12 min   | 2-3 min          | 6-10 min     |

---

## Security Considerations

### Runner Isolation

- Run the runner under a dedicated Windows account with minimal privileges
- Do not grant the runner account administrative rights unless absolutely necessary
- Use separate runners for different security zones (public repos vs private repos)

### Secrets Management

The workflow supports these optional secrets:
- `EPIC_API_KEY`: For automated UE5 installation (GitHub-hosted runners only)
- `STEAM_USERNAME` / `STEAM_PASSWORD`: For Steam deployment
- `DEPLOY_TOKEN`: For custom deployment targets

Configure secrets at: **Settings** → **Secrets and variables** → **Actions**

### Network Security

- Ensure the runner can reach:
  - `github.com` (GitHub API and Actions)
  - `objects.githubusercontent.com` (Git LFS)
  - `*.epicgames.com` (UE5 updates)
- Configure firewall rules to allow outbound HTTPS (port 443)

---

## Maintenance

### Regular Maintenance Tasks

**Weekly**:
- Check runner disk space
- Review build logs for warnings
- Update Windows and Visual Studio if needed

**Monthly**:
- Clean old build artifacts:
  ```powershell
  Remove-Item -Path "C:\actions-runner\_work\*\*\Intermediate" -Recurse -Force -Confirm
  ```
- Review runner performance metrics

**As Needed**:
- Update UE5 to latest patch version
- Update runner software when GitHub releases new versions
- Rotate runner labels if using multiple runners

### Updating the Runner

To update the runner software:

1. Stop the service:
   ```powershell
   ./svc.sh stop
   ```

2. Download the latest runner:
   ```powershell
   Invoke-WebRequest -Uri https://github.com/actions/runner/releases/latest -OutFile runner-latest.zip
   ```

3. Extract and replace files (backup the old version first)

4. Restart the service:
   ```powershell
   ./svc.sh start
   ```

---

## Support

For issues with:
- **GitHub Actions Runner**: [github.com/actions/runner](https://github.com/actions/runner/issues)
- **Unreal Engine**: [unrealengine.com/support](https://www.unrealengine.com/support)
- **KatanaCombat Project**: [github.com/noahbutcher97/KatanaCombat_Demo/issues](https://github.com/noahbutcher97/KatanaCombat_Demo/issues)

---

## Additional Resources

- [GitHub Actions Self-Hosted Runners Documentation](https://docs.github.com/en/actions/hosting-your-own-runners)
- [Unreal Engine Build System](https://docs.unrealengine.com/5.6/en-US/BuildGraph/)
- [Visual Studio 2022 Documentation](https://docs.microsoft.com/en-us/visualstudio/)
