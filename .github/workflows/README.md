# KatanaCombat CI/CD Pipeline Documentation

This directory contains the GitHub Actions workflows for the KatanaCombat UE5.6 project.

## 📋 Table of Contents
- [Overview](#overview)
- [Workflow: ue5-ci.yml](#workflow-ue5-ciyml)
- [Setup Instructions](#setup-instructions)
- [Usage](#usage)
- [Mobile Development](#mobile-development)
- [Troubleshooting](#troubleshooting)

---

## Overview

The KatanaCombat CI pipeline provides automated building, testing, and validation for the Unreal Engine 5.6 project. It supports both self-hosted and GitHub-hosted runners with intelligent auto-detection.

### Key Features
- ✅ **Dual Runner Support**: Works with self-hosted or GitHub-hosted Windows runners
- ✅ **Automated Tooling**: Auto-installs VS 2022 Build Tools on GitHub runners
- ✅ **Smart Caching**: Caches Intermediate/, DDC, and build tools
- ✅ **Comprehensive Testing**: Runs 126+ automation tests with NullRHI
- ✅ **Asset Validation**: Validates assets using ResavePackages commandlet
- ✅ **Detailed Reporting**: Build stats, test results, and PR comments
- ✅ **Mobile-Friendly**: workflow_dispatch for manual triggers from mobile devices
- ✅ **Artifact Management**: Smart retention policies (7-30 days)

---

## Workflow: ue5-ci.yml

### Triggers
- **Push**: Branches: `main`, `develop`, `feature/**`, `bugfix/**`
- **Pull Request**: Targets: `main`, `develop`
- **Manual**: workflow_dispatch with configurable options

### Jobs

#### 1. detect-environment
Detects runner type and sets cache prefixes.

**Outputs:**
- `runner-type`: `self-hosted` or `windows-latest`
- `use-self-hosted`: Boolean flag
- `cache-key-prefix`: Cache namespace (`self` or `github`)

#### 2. build-and-test
Main build and test job (runs on detected runner type).

**Steps:**
1. **Setup**: Checkout, Git LFS, environment display
2. **Tooling**: VS 2022 Build Tools (GitHub runners only), MSBuild detection
3. **UE5.6**: Detection, validation, environment setup
4. **Caching**: Intermediate files, DDC, build tools
5. **Build**: Clean, compile, statistics, error parsing
6. **Testing**: Run 126+ automation tests with NullRHI
7. **Validation**: Asset validation on PRs (ResavePackages)
8. **Artifacts**: Upload logs, test results, binaries, stats
9. **Reporting**: PR comments with build summary
10. **Cleanup**: Remove temporary files

### Environment Variables
```yaml
PROJECT_NAME: KatanaCombat
UE_VERSION: 5.6
BUILD_CONFIG: Development (configurable)
PLATFORM: Win64
TARGET: KatanaCombatEditor
```

---

## Setup Instructions

### Option A: Self-Hosted Runner (Recommended)

**Requirements:**
- Windows 10/11 or Windows Server 2019+
- 16+ GB RAM
- 100+ GB free disk space
- Unreal Engine 5.6 installed

**Steps:**

1. **Install Unreal Engine 5.6**
   ```powershell
   # Install to default location
   C:\Program Files\Epic Games\UE_5.6
   
   # Or custom location and set registry:
   New-Item -Path "HKLM:\SOFTWARE\EpicGames\Unreal Engine\5.6" -Force
   Set-ItemProperty -Path "HKLM:\SOFTWARE\EpicGames\Unreal Engine\5.6" `
     -Name "InstalledDirectory" -Value "D:\UnrealEngine\UE_5.6"
   ```

2. **Install Visual Studio 2022**
   - Install VS 2022 Community/Professional with:
     - Desktop development with C++
     - Windows 11 SDK (10.0.22621.0)
     - MSVC v143 - VS 2022 C++ x64/x86 build tools

3. **Setup GitHub Runner**
   ```powershell
   # Download runner
   cd C:\actions-runner
   Invoke-WebRequest -Uri https://github.com/actions/runner/releases/download/v2.311.0/actions-runner-win-x64-2.311.0.zip -OutFile actions-runner.zip
   Expand-Archive -Path actions-runner.zip -DestinationPath .
   
   # Configure runner WITH REQUIRED LABELS
   # IMPORTANT: The 'ue5' label is required for the workflow to match this runner
   .\config.cmd --url https://github.com/YOUR_ORG/KatanaCombat_Demo --token YOUR_TOKEN --labels self-hosted,Windows,ue5
   
   # Install and start as service
   .\svc.cmd install
   .\svc.cmd start
   ```

4. **Verify Setup**
   ```powershell
   # Check UE installation
   Test-Path "C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat"
   
   # Check MSBuild
   & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest
   
   # Check runner service
   Get-Service actions.runner.*
   ```

### Option B: GitHub-Hosted Runner

**Limitations:**
- No UE5.6 pre-installed
- Requires custom UE installation logic (commented out in workflow)
- Longer build times (VS Build Tools + UE setup)

**Setup:**

1. **Configure Secrets (if using custom UE download)**
   ```
   Settings → Secrets and variables → Actions → New repository secret
   
   Secrets:
   - EPIC_API_KEY: Your Epic Games API key
   - UE_DOWNLOAD_URL: Pre-authenticated UE5.6 download URL
   ```

2. **Uncomment UE Installation Step**
   Edit `.github/workflows/ue5-ci.yml`:
   ```yaml
   # Find this section and uncomment:
   - name: 📦 Download & Install Unreal Engine 5.6
     if: steps.ue-detect.outputs.ue-found == 'false' && needs.detect-environment.outputs.use-self-hosted == 'false'
     # Add your custom UE download/install logic
   ```

### Option C: Docker Container (Advanced)

**Use Epic's official UE containers:**
```yaml
container:
  image: ghcr.io/epicgames/unreal-engine:5.6-dev
  credentials:
    username: ${{ github.actor }}
    password: ${{ secrets.GITHUB_TOKEN }}
```

**Note:** Requires Epic Games account linking to GitHub.

---

## Usage

### Automatic Triggers

**On Push:**
```bash
git commit -m "feat: Add new combo attack"
git push origin feature/new-combo
# CI automatically runs
```

**On Pull Request:**
```bash
gh pr create --title "New Feature" --body "Description"
# CI runs with asset validation
```

### Manual Triggers

**From GitHub UI:**
1. Navigate to: `Actions` → `UE5.6 CI Pipeline` → `Run workflow`
2. Select branch
3. Configure options:
   - Skip tests: Yes/No
   - Skip asset validation: Yes/No
   - Build configuration: Development/DebugGame/Shipping
   - Runner type: Auto/self-hosted/windows-latest

**From GitHub CLI:**
```bash
# Run with defaults
gh workflow run ue5-ci.yml

# Run with custom configuration
gh workflow run ue5-ci.yml \
  -f skip_tests=false \
  -f skip_asset_validation=false \
  -f build_configuration=Development \
  -f runner_type=self-hosted
```

**From GitHub Mobile App:**
1. Open repository → Actions
2. Select "UE5.6 CI Pipeline"
3. Tap "Run workflow"
4. Configure options
5. Tap "Run workflow" button

---

## Mobile Development

This workflow is optimized for mobile-first development:

### Making Changes on Mobile
1. **Edit code via GitHub.dev**:
   - Press `.` in repository to open web editor
   - Make code changes
   - Commit directly to branch

2. **Trigger CI from mobile**:
   - Use GitHub mobile app
   - Navigate to Actions → Run workflow
   - Select configuration
   - Monitor build progress

3. **Review Results**:
   - PR comments show build status
   - Download artifacts via mobile browser
   - Review test results in reports

### Artifact Access
All artifacts are accessible via:
- GitHub Actions UI
- Direct links in PR comments
- GitHub API/CLI

**Example: Download latest build**
```bash
gh run download $(gh run list --limit 1 --json databaseId -q '.[0].databaseId')
```

---

## Artifacts

### Build Logs (14 days)
- `Build-{run_number}.log`: Full build output
- `BuildReport-{run_number}.md`: Formatted statistics

### Test Results (30 days)
- `AutomationTests-{run_number}.log`: Test output
- `TestReport-{run_number}.md`: Formatted test summary
- `AutomationReport.json`: Machine-readable results

### Binaries (7 days, push only)
- `*.exe`: Compiled executables
- `*.dll`: Module libraries
- `*.pdb`: Debug symbols

### Build Stats (30 days)
- `BuildReport-*.md`: Build metrics
- `Stats/**`: Performance data

---

## Caching Strategy

### Intermediate Files
**Path:** `Intermediate/`, `Saved/Shaders/`
**Key:** `{runner}-intermediate-{hash(Source/**/*.cpp,*.h,*.uproject)}`
**Purpose:** Speeds up incremental builds

### Derived Data Cache (DDC)
**Path:** `DerivedDataCache/`
**Key:** `{runner}-ddc-{hash(Content/**/*.uasset,*.umap)}`
**Purpose:** Caches compiled assets

### Build Tools
**Path:** `Binaries/DotNET/`, `Saved/UnrealBuildTool/`
**Key:** `{runner}-buildtools-{hash(*.uproject)}`
**Purpose:** Caches UnrealBuildTool artifacts

**Cache Invalidation:**
- Cache is automatically invalidated when source files change
- Separate caches for self-hosted vs GitHub-hosted runners
- Restore-keys allow partial cache hits

---

## Monitoring & Alerts

### Build Notifications
- **PR Comments**: Automatic summary on every PR build
- **Email**: GitHub sends emails on workflow failures (configurable)
- **Slack/Discord**: Integrate via webhooks (add custom step)

### Metrics Tracked
- Build duration
- Compile errors/warnings
- Test pass/fail counts
- Asset validation errors
- Disk usage
- Cache hit rates

### Example: Add Slack Notification
```yaml
- name: 📢 Notify Slack
  if: always()
  uses: slackapi/slack-github-action@v1
  with:
    payload: |
      {
        "text": "Build ${{ steps.build.outcome }}: ${{ github.event.head_commit.message }}"
      }
  env:
    SLACK_WEBHOOK_URL: ${{ secrets.SLACK_WEBHOOK }}
```

---

## Troubleshooting

### Issue: Self-Hosted Runner Not Picking Up Jobs
**Symptom:** 
- Job shows `Waiting for a runner to pick up this job...`
- Runner is online but jobs timeout
- Detect job runs on Ubuntu but build job never starts

**Error in logs:** 
```
Requested labels: self-hosted, Windows, ue5
Waiting for a runner to pick up this job...
```

**Root Cause:** Runner is missing the required `ue5` label.

**Solution:**
1. Check your runner's current labels:
   - Go to: `Repository Settings → Actions → Runners`
   - Click on your runner name
   - Verify labels include: `self-hosted`, `Windows`, AND `ue5`

2. If `ue5` label is missing, reconfigure the runner:
   ```powershell
   cd D:\actions-runner  # Or wherever your runner is installed
   
   # Stop the service if running
   .\svc.cmd stop
   .\svc.cmd uninstall
   
   # Remove old configuration
   .\config.cmd remove --token YOUR_REMOVAL_TOKEN
   
   # Reconfigure with correct labels (get new token from GitHub)
   .\config.cmd --url https://github.com/noahbutcher97/KatanaCombat_Demo --token YOUR_NEW_TOKEN --labels self-hosted,Windows,ue5
   
   # Reinstall and start service
   .\svc.cmd install
   .\svc.cmd start
   
   # Verify service is running
   .\svc.cmd status
   ```

3. Verify in GitHub:
   - Refresh the Runners page
   - Your runner should show all three labels: `self-hosted`, `Windows`, `ue5`
   - Status should be "Idle" (green)

4. Re-run the workflow to test

**Why this happens:** When configuring a runner without the `--labels` parameter, GitHub only assigns default labels (`self-hosted`, `Windows`). The workflow specifically requires the custom `ue5` label to match this project's runner requirements.

### Issue: UE5.6 Not Found
**Error:** `Unreal Engine 5.6 not found!`

**Solutions:**
1. Verify installation path:
   ```powershell
   Test-Path "C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat"
   ```

2. Check registry:
   ```powershell
   Get-ItemProperty "HKLM:\SOFTWARE\EpicGames\Unreal Engine\5.6" -ErrorAction SilentlyContinue
   ```

3. Add to registry:
   ```powershell
   New-Item -Path "HKLM:\SOFTWARE\EpicGames\Unreal Engine\5.6" -Force
   Set-ItemProperty -Path "HKLM:\SOFTWARE\EpicGames\Unreal Engine\5.6" `
     -Name "InstalledDirectory" -Value "YOUR_PATH"
   ```

### Issue: Build Failed with Exit Code 6
**Error:** `Build failed with exit code: 6`

**Solutions:**
1. Check build log for specific errors
2. Common causes:
   - Missing dependencies
   - Syntax errors in code
   - Invalid module references
   - Out of memory (increase runner RAM)

3. Local reproduction:
   ```powershell
   cd "C:\path\to\project"
   & "C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat" `
     KatanaCombatEditor Win64 Development -project="KatanaCombat.uproject"
   ```

### Issue: Tests Failing
**Error:** `X test(s) failed`

**Solutions:**
1. Review test log:
   - Download `test-results-{run}` artifact
   - Open `AutomationTests-{run}.log`
   - Search for `FAILED:` entries

2. Run locally with NullRHI:
   ```powershell
   & "C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
     "KatanaCombat.uproject" `
     -ExecCmds="Automation RunTests KatanaCombat; Quit" `
     -NullRHI -Unattended
   ```

3. Skip tests temporarily:
   - Use workflow_dispatch with `skip_tests=true`
   - Fix tests locally
   - Re-enable

### Issue: Cache Not Working
**Symptoms:** Every build is a full rebuild

**Solutions:**
1. Check cache key hash:
   - Ensure source files aren't changing on every commit
   - Remove generated files from git (e.g., `.vs/`, `Binaries/`)

2. Verify cache hit in logs:
   ```
   Cache restored from key: self-intermediate-abc123
   Cache Size: 2.3 GB
   ```

3. Clear cache manually:
   - Settings → Actions → Caches
   - Delete old caches
   - Next run will create fresh cache

### Issue: Runner Out of Space
**Error:** `No space left on device`

**Solutions:**
1. Increase runner disk size (50+ GB recommended)

2. Clean runner workspace:
   ```powershell
   # On self-hosted runner
   cd C:\actions-runner\_work
   Get-ChildItem -Directory | Where-Object { $_.Name -ne 'KatanaCombat_Demo' } | Remove-Item -Recurse -Force
   ```

3. Reduce artifact retention:
   ```yaml
   retention-days: 3  # Instead of 30
   ```

4. Disable DDC caching temporarily:
   ```yaml
   # Comment out DDC cache step
   ```

### Issue: VS Build Tools Installation Failed
**Error:** Install failed or timeout

**Solutions:**
1. Use self-hosted runner (recommended for production)

2. Increase timeout:
   ```yaml
   timeout-minutes: 180  # Instead of 120
   ```

3. Pre-install on GitHub runner image:
   - Create custom runner image with VS pre-installed
   - Use Azure DevOps hosted agents (have VS pre-installed)

### Issue: Git LFS Timeout
**Error:** `Error downloading object: timeout`

**Solutions:**
1. Increase LFS timeout:
   ```bash
   git config lfs.activitytimeout 300
   ```

2. Use LFS cache:
   ```yaml
   - name: Cache Git LFS
     uses: actions/cache@v4
     with:
       path: .git/lfs
       key: lfs-${{ hashFiles('.gitattributes') }}
   ```

3. Switch to self-hosted runner with better network

---

## Performance Optimization

### Build Times

**Typical Duration:**
- **Cold build** (no cache): 30-45 minutes
- **Warm build** (with cache): 10-15 minutes
- **Incremental build** (minor changes): 3-5 minutes

**Optimization Tips:**
1. Use self-hosted runners (faster than GitHub-hosted)
2. Enable all caching (Intermediate, DDC, BuildTools)
3. Use Unity builds for faster compilation:
   ```cpp
   // In Target.cs
   bUseUnityBuild = true;
   ```
4. Reduce artifact uploads (only upload on main/develop)
5. Skip asset validation on feature branches

### Test Execution

**Typical Duration:**
- **Full suite** (126 tests): 5-10 minutes
- **Subset**: 1-3 minutes

**Optimization Tips:**
1. Use NullRHI (no GPU rendering)
2. Run in parallel (add `-ExecCmds="Automation RunTests KatanaCombat Workers=4"`)
3. Skip slow tests in CI (tag with `Slow` and filter)
4. Use test categories for targeted runs

### Cache Efficiency

**Best Practices:**
1. Commit `.gitattributes` to exclude generated files
2. Use granular cache keys (hash specific directories)
3. Set appropriate retention (7 days for active branches)
4. Monitor cache size (GitHub has 10 GB limit)
5. Clear stale caches monthly

**Cache Size Targets:**
- Intermediate: 1-3 GB
- DDC: 2-5 GB
- BuildTools: <500 MB

---

## Advanced Configuration

### Matrix Builds

Run multiple configurations in parallel:
```yaml
strategy:
  matrix:
    config: [Development, Shipping]
    platform: [Win64]
runs-on: ${{ matrix.runner }}
env:
  BUILD_CONFIG: ${{ matrix.config }}
  PLATFORM: ${{ matrix.platform }}
```

### Nightly Builds

Create separate workflow for nightly builds:
```yaml
on:
  schedule:
    - cron: '0 2 * * *'  # 2 AM UTC daily

jobs:
  nightly:
    # Full build + extended tests + packaging
```

### Custom Test Suites

Run specific test categories:
```yaml
- name: Run Combat Tests Only
  run: |
    Automation RunTests KatanaCombat.Combat
```

### Conditional Steps

Skip steps based on conditions:
```yaml
- name: Upload Binaries
  if: github.ref == 'refs/heads/main' && success()
  # Only upload on main branch
```

---

## Security

### Secrets Required
- `EPIC_API_KEY`: (Optional) For automated UE downloads
- `UE_DOWNLOAD_URL`: (Optional) Pre-authenticated download URL
- `SLACK_WEBHOOK`: (Optional) For notifications

### Best Practices
1. Never commit credentials to repository
2. Use secrets for sensitive data
3. Limit secret access to specific workflows
4. Rotate secrets regularly
5. Use environment protection rules for production

### SBOM Generation

Add Software Bill of Materials:
```yaml
- name: Generate SBOM
  uses: anchore/sbom-action@v0
  with:
    path: ./
    format: spdx-json
```

---

## Support

### Getting Help
- **Issues**: GitHub Issues for bug reports
- **Discussions**: GitHub Discussions for questions
- **Wiki**: Project wiki for extended documentation

### Useful Links
- [GitHub Actions Docs](https://docs.github.com/en/actions)
- [Unreal Engine Automation](https://docs.unrealengine.com/5.6/en-US/automation-system-in-unreal-engine/)
- [UE Build System](https://docs.unrealengine.com/5.6/en-US/unreal-build-tool-in-unreal-engine/)

---

## Changelog

### v1.0.0 (2026-01-31)
- Initial release
- Dual runner support (self-hosted + GitHub-hosted)
- Automated VS 2022 Build Tools installation
- UE5.6 detection and validation
- Comprehensive caching strategy
- 126+ automation tests with NullRHI
- Asset validation on PRs
- Smart artifact management
- PR comment integration
- Mobile-friendly workflow_dispatch

---

*Last Updated: 2026-01-31*
*Maintained by: KatanaCombat Team*
