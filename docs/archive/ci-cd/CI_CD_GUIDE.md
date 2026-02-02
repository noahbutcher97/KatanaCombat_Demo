# CI/CD Pipeline Documentation

This document provides comprehensive information about the KatanaCombat CI/CD pipeline, including architecture, configuration, and usage.

## Table of Contents
- [Overview](#overview)
- [Architecture](#architecture)
- [Runner Types](#runner-types)
- [Workflow Triggers](#workflow-triggers)
- [Build Process](#build-process)
- [Testing & Validation](#testing--validation)
- [Artifacts](#artifacts)
- [Configuration](#configuration)
- [Usage Examples](#usage-examples)
- [Troubleshooting](#troubleshooting)

---

## Overview

The KatanaCombat CI/CD pipeline is designed to support both self-hosted and GitHub-hosted runners, providing complete cloud independence while maintaining the option for on-premise builds.

### Key Features

✅ **Dual Runner Support**: Automatically selects between self-hosted and GitHub-hosted runners
✅ **Smart Detection**: Detects UE5 installation automatically on self-hosted runners
✅ **Automated Setup**: Installs Visual Studio and UE5 on GitHub-hosted runners
✅ **Intelligent Caching**: Caches intermediate files, DDC, and UE5 installation
✅ **Comprehensive Testing**: Runs automation tests and asset validation
✅ **Artifact Management**: Stores build logs (14d), test results (30d), and binaries (7d)
✅ **Fallback Support**: Falls back to GitHub-hosted runners if self-hosted unavailable

---

## Architecture

```
┌─────────────────────────────────────────────────────┐
│              Workflow Trigger                        │
│  (Push/PR to main/develop OR Manual Dispatch)       │
└─────────────────────┬───────────────────────────────┘
                      │
                      ▼
┌─────────────────────────────────────────────────────┐
│           Detect Runner Type Job                     │
│  - Auto (default): Try self-hosted first            │
│  - Self-Hosted Only                                  │
│  - GitHub-Hosted Only                                │
└─────────────┬───────────────────┬───────────────────┘
              │                   │
    ┌─────────▼────────┐    ┌────▼──────────┐
    │  Self-Hosted     │    │ GitHub-Hosted │
    │  Build Job       │    │ Build Job     │
    │                  │    │               │
    │ • Detect UE5     │    │ • Install VS  │
    │ • Cache restore  │    │ • Install UE5 │
    │ • Build project  │    │ • Cache setup │
    │ • Run tests      │    │ • Build proj  │
    │ • Validate       │    │ • Run tests   │
    │ • Upload logs    │    │ • Validate    │
    └─────────┬────────┘    └────┬──────────┘
              │                  │
              └─────────┬────────┘
                        ▼
              ┌──────────────────┐
              │  Finalize Job    │
              │ • Check results  │
              │ • Collect stats  │
              │ • Generate report│
              └──────────────────┘
```

### Job Dependencies

- `detect-runner`: Runs first, outputs configuration for other jobs
- `build-self-hosted`: Runs if self-hosted enabled, continues on error
- `build-github-hosted`: Runs if GitHub-hosted enabled AND (self-hosted failed OR explicitly requested)
- `finalize`: Runs always, collects results from all build jobs

---

## Runner Types

### Self-Hosted Runners

**When to Use**:
- Production builds
- When UE5 is already installed locally
- When you want faster builds (dedicated hardware)
- When you need full control over the environment

**Requirements**:
- Windows 10/11 or Windows Server 2019/2022
- Unreal Engine 5.6 pre-installed
- Visual Studio 2022 with required workloads
- Runner labels: `self-hosted`, `Windows`, `ue5`

**Advantages**:
- ✅ Faster builds (no setup time)
- ✅ No UE5 installation required
- ✅ Can use cached DDC across builds
- ✅ Full control over hardware specs

**Disadvantages**:
- ❌ Requires manual setup and maintenance
- ❌ Infrastructure cost
- ❌ Single point of failure if runner goes offline

**See**: [SETUP_GUIDE.md](SETUP_GUIDE.md) for self-hosted runner setup

### GitHub-Hosted Runners

**When to Use**:
- Testing the CI pipeline
- When self-hosted runners are unavailable
- For open-source contributors without access to self-hosted runners
- For one-off builds

**Requirements**:
- None (automatic setup)
- Optional: `EPIC_API_KEY` secret for UE5 installation

**Advantages**:
- ✅ Zero infrastructure maintenance
- ✅ Always available
- ✅ Clean environment for each build
- ✅ Good for testing and validation

**Disadvantages**:
- ❌ Slower (includes setup time)
- ❌ Limited to 6 hours per job
- ❌ Requires UE5 installation/caching
- ❌ May incur GitHub Actions costs

### Auto Mode (Default)

The workflow runs in **auto mode** by default, which:
1. Attempts to run on self-hosted runners first
2. If self-hosted runners are unavailable or the build fails, falls back to GitHub-hosted
3. Ensures builds always succeed on at least one runner type

---

## Workflow Triggers

### Automatic Triggers

**Push to Main/Develop Branches**:
```yaml
on:
  push:
    branches: [ main, develop ]
```
- Triggered on every push to `main` or `develop`
- Uses auto mode (self-hosted with GitHub-hosted fallback)

**Pull Requests to Main/Develop**:
```yaml
on:
  pull_request:
    branches: [ main, develop ]
```
- Triggered when a PR is opened or updated
- Uses auto mode
- Results shown as PR checks

### Manual Triggers

Navigate to: **Actions** → **UE5 CI/CD Pipeline** → **Run workflow**

**Options**:
- **Branch**: Select branch to build
- **Runner Type**: 
  - `auto`: Try self-hosted first, fallback to GitHub-hosted
  - `self-hosted`: Only use self-hosted runners
  - `github-hosted`: Only use GitHub-hosted runners

---

## Build Process

### Self-Hosted Build Steps

1. **Checkout Repository**
   - Clones the repository
   - Enables Git LFS for large files
   - Checks out submodules recursively

2. **Detect UE5 Installation**
   - Searches standard paths for UE5 5.6
   - Sets environment variables for build tools
   - Fails if UE5 not found

3. **Setup MSBuild**
   - Configures Microsoft Build Tools
   - Sets up Visual Studio environment

4. **Cache Intermediate Files**
   - Restores cached `Intermediate/`, `DerivedDataCache/`, `.vs/`
   - Uses hash of `.uproject` and `.Build.cs` files as cache key
   - Speeds up incremental builds

5. **Generate Project Files**
   - Runs `Build.bat -projectfiles`
   - Generates Visual Studio solution files

6. **Build Project**
   - Compiles `KatanaCombatEditor Win64 Development`
   - Uses Unreal Build Tool (UBT)

7. **Run Automation Tests**
   - Executes project tests with `NullRHI` (headless)
   - Exports test reports to `Saved/Automation/Reports/`

8. **Asset Validation**
   - Runs `ResavePackages` to validate assets
   - Detects outdated or corrupted assets

9. **Collect Statistics & Upload Artifacts**
   - Gathers build metrics
   - Uploads logs, test results, binaries

### GitHub-Hosted Build Steps

1. **Checkout Repository**
   - Same as self-hosted

2. **Free Disk Space**
   - Cleans temp directories
   - Ensures sufficient space for UE5 installation

3. **Install Visual Studio 2022 Components**
   - Downloads VS Build Tools installer
   - Installs required workloads and components
   - Takes ~15-20 minutes

4. **Cache UE5 Installation**
   - Checks for cached UE5 at `C:\UE5`
   - Restores if available (saves ~30 minutes)

5. **Install Unreal Engine 5.6** (if not cached)
   - **Option A**: Uses `EPIC_API_KEY` if provided
   - **Option B**: Uses pre-cached build artifacts
   - **Option C**: Manual setup required (documented in output)

6. **Setup UE5 Environment**
   - Sets environment variables
   - Adds UE5 to PATH

7. **Cache Project Dependencies**
   - Caches `Intermediate/`, `DerivedDataCache/`, `Saved/Cooked/`

8. **Build, Test, Validate** (same as self-hosted)

---

## Testing & Validation

### Automation Tests

**What is Tested**:
- Unit tests in `KatanaCombatTest` module
- Functional tests
- Blueprint tests

**Execution**:
```cmd
UnrealEditor.exe "KatanaCombat.uproject" \
  -ExecCmds="Automation RunTests KatanaCombat;Quit" \
  -unattended -nopause -NullRHI -log \
  -ReportExportPath="Saved\Automation\Reports"
```

**Test Reports**:
- JSON format reports in `Saved/Automation/Reports/`
- Uploaded as artifact with 30-day retention
- Viewable in workflow run artifacts

### Asset Validation

**What is Validated**:
- Asset integrity
- Outdated package formats
- Missing references
- Deprecated property usage

**Execution**:
```cmd
UnrealEditor.exe "KatanaCombat.uproject" \
  -run=ResavePackages \
  -unattended -nopause -log
```

**Validation Reports**:
- Logs in `Saved/Logs/`
- Identifies assets requiring updates

---

## Artifacts

### Build Logs (14-day retention)

**Contents**:
- UBT build logs
- Compiler output
- Link errors
- General application logs

**Location**: `Saved/Logs/`

**Access**: Download from workflow run → Artifacts → `build-logs-*`

### Test Results (30-day retention)

**Contents**:
- Automation test JSON reports
- Test pass/fail status
- Performance metrics
- Error messages and stack traces

**Location**: `Saved/Automation/Reports/`

**Access**: Download from workflow run → Artifacts → `test-results-*`

### Binaries (7-day retention)

**Contents**:
- Compiled game/editor binaries
- DLL dependencies
- Executable files

**Location**: `Binaries/Win64/`

**Exclusions**: Debug symbols (`.pdb` files) excluded to save space

**Access**: Download from workflow run → Artifacts → `binaries-*`

### Build Statistics (30-day retention)

**Contents**:
- Runner type
- Build timestamp
- Success/failure status
- Disk space usage (GitHub-hosted only)

**Format**: JSON

**Access**: Download from workflow run → Artifacts → `build-stats-*`

### Build Report (90-day retention)

**Contents**:
- Summary of build results
- Runner type used
- Workflow metadata
- Links to logs and artifacts

**Format**: Markdown

**Access**: Download from workflow run → Artifacts → `build-report`

---

## Configuration

### Environment Variables

Defined in workflow file:

```yaml
env:
  PROJECT_NAME: KatanaCombat        # Project name (must match .uproject)
  UE_VERSION: "5.6"                 # Unreal Engine version
  BUILD_CONFIGURATION: Development Editor  # Build configuration
```

### GitHub Secrets (Optional)

Configure at: **Settings** → **Secrets and variables** → **Actions** → **New repository secret**

| Secret Name | Description | Required For | Example |
|-------------|-------------|--------------|---------|
| `EPIC_API_KEY` | Epic Games API key | GitHub-hosted UE5 install | `abc123...` |
| `STEAM_USERNAME` | Steam account username | Steam deployment | `yourusername` |
| `STEAM_PASSWORD` | Steam account password | Steam deployment | `yourpassword` |
| `DEPLOY_TOKEN` | Custom deployment token | Custom deployment | `token123...` |

**Note**: Secrets are never exposed in logs and are only available during workflow execution.

### Runner Labels

For self-hosted runners, use these labels:

```yaml
runs-on: [self-hosted, Windows, ue5]
```

**Label Meanings**:
- `self-hosted`: Indicates a self-hosted runner
- `Windows`: Runner OS
- `ue5`: Custom label indicating UE5 is installed

---

## Usage Examples

### Example 1: Run Build on Self-Hosted Only

1. Go to **Actions** → **UE5 CI/CD Pipeline**
2. Click **Run workflow**
3. Select branch (e.g., `develop`)
4. Set **runner_type** to `self-hosted`
5. Click **Run workflow**

### Example 2: Test GitHub-Hosted Build

1. Go to **Actions** → **UE5 CI/CD Pipeline**
2. Click **Run workflow**
3. Select branch
4. Set **runner_type** to `github-hosted`
5. Click **Run workflow**

**Note**: First run will be slow (UE5 installation). Subsequent runs will be faster (cached UE5).

### Example 3: Automatic Build on PR

1. Create a feature branch:
   ```bash
   git checkout -b feature/new-combat-move
   ```

2. Make changes and commit:
   ```bash
   git add .
   git commit -m "Add spinning slash attack"
   git push origin feature/new-combat-move
   ```

3. Create PR via GitHub UI

4. CI automatically runs and reports status on PR

### Example 4: Override Runner Type via Commit Message

Not directly supported, but you can create branch-specific configurations:

```yaml
# .github/workflows/ue5-ci-dev.yml
on:
  push:
    branches: [ develop ]
    
# ... use github-hosted only for develop branch
```

---

## Troubleshooting

### Build Fails: "No runner available"

**Cause**: No runners with required labels are online

**Solutions**:
1. Check runner status: **Settings** → **Actions** → **Runners**
2. Ensure runner service is running on self-hosted machine
3. Verify runner labels match: `[self-hosted, Windows, ue5]`
4. Try manual dispatch with `github-hosted` runner type

### Build Fails: "UE5 not found" (Self-Hosted)

**Cause**: UE5 not installed or not in expected location

**Solutions**:
1. Install UE5 5.6 via Epic Games Launcher
2. Verify installation path: `C:\Program Files\Epic Games\UE_5.6`
3. Create symbolic link if installed elsewhere (see SETUP_GUIDE.md)

### Build Fails: "UE5 installation not available" (GitHub-Hosted)

**Cause**: UE5 not cached and automatic installation not configured

**Solutions**:
1. **Recommended**: Add `EPIC_API_KEY` secret for automatic installation
2. Pre-cache UE5 by running workflow once (builds may fail initially)
3. Use self-hosted runner instead

### Workflow Stuck/Running Too Long

**Cause**: Step taking longer than expected or hanging

**Solutions**:
1. Cancel workflow: Workflow run → Cancel
2. Check self-hosted runner logs if using self-hosted
3. Increase `timeout-minutes` in workflow file if needed

### Cache Miss Every Build

**Cause**: Cache key changes on every run

**Solutions**:
1. Ensure `.uproject` and `.Build.cs` files are committed
2. Check cache key in workflow logs
3. Verify `hashFiles()` pattern is correct

### Artifacts Not Uploading

**Cause**: Paths don't exist or artifact too large

**Solutions**:
1. Check if paths exist before upload step
2. Verify `Saved/Logs/` contains files
3. Check artifact size limit (GitHub: 10GB max per artifact)

### Tests Failing in CI but Passing Locally

**Possible Causes**:
- Environment differences
- Missing dependencies
- Timing issues in headless mode

**Solutions**:
1. Run tests locally with `-NullRHI` flag:
   ```cmd
   UnrealEditor.exe "KatanaCombat.uproject" -ExecCmds="Automation RunTests" -NullRHI
   ```
2. Check test logs in artifacts
3. Add debug logging to tests
4. Ensure tests don't depend on GPU/rendering

---

## Performance Metrics

### Expected Build Times

| Runner Type | First Build | Incremental | Cache Hit |
|-------------|-------------|-------------|-----------|
| Self-Hosted (8 core, 32GB) | 30-45 min | 8-12 min | 5-8 min |
| Self-Hosted (16 core, 64GB) | 15-25 min | 5-8 min | 3-5 min |
| GitHub-Hosted (First run) | 90-120 min | N/A | N/A |
| GitHub-Hosted (Cached UE5) | 40-60 min | 15-25 min | 10-15 min |

**Notes**:
- First build includes UE5 setup (GitHub-hosted only)
- Incremental builds assume only code changes
- Cache hit assumes Intermediate/ and DDC are cached

### Artifact Sizes

| Artifact | Typical Size | Max Size |
|----------|--------------|----------|
| Build Logs | 10-50 MB | 500 MB |
| Test Results | 1-10 MB | 100 MB |
| Binaries (Win64) | 500 MB - 2 GB | 5 GB |
| Build Stats | < 1 KB | 1 MB |

---

## Maintenance

### Weekly Tasks
- [ ] Review failed builds and address issues
- [ ] Check artifact storage usage
- [ ] Verify self-hosted runner is online and healthy

### Monthly Tasks
- [ ] Clean old artifacts (if manual cleanup needed)
- [ ] Review and optimize cache strategy
- [ ] Update workflow dependencies (actions/checkout, etc.)

### As Needed
- [ ] Update UE5 version in workflow when upgrading project
- [ ] Add new secrets for deployment
- [ ] Adjust timeout values based on project growth

---

## Advanced Configuration

### Customizing Build Configuration

Edit the workflow file to build different configurations:

```yaml
env:
  BUILD_CONFIGURATION: Development Editor  # or: Shipping, DebugGame, etc.
```

### Adding Custom Build Steps

Add steps before or after the build:

```yaml
- name: Run Custom Script
  shell: pwsh
  run: |
    .\Scripts\PreBuild.ps1
```

### Conditional Steps

Run steps only for specific branches:

```yaml
- name: Deploy to Staging
  if: github.ref == 'refs/heads/develop'
  run: |
    # Deployment script
```

### Matrix Builds

Build multiple configurations in parallel:

```yaml
strategy:
  matrix:
    config: [Development, Shipping]
    platform: [Win64, Linux]
```

---

## Security Best Practices

1. **Never commit secrets** to the repository
2. **Use GitHub Secrets** for sensitive data
3. **Limit runner access** to necessary repositories only
4. **Review workflow changes** in PRs before merging
5. **Use separate runners** for public/private repos
6. **Enable branch protection** on main/develop branches
7. **Require status checks** before merging PRs

---

## Additional Resources

- [GitHub Actions Documentation](https://docs.github.com/en/actions)
- [Unreal Engine Build Documentation](https://docs.unrealengine.com/5.6/en-US/BuildGraph/)
- [Self-Hosted Runner Setup Guide](SETUP_GUIDE.md)
- [KatanaCombat Project Documentation](README.md)

---

## Support & Contribution

Found a bug in the CI pipeline? Have suggestions for improvements?

1. **Check existing issues**: [github.com/noahbutcher97/KatanaCombat_Demo/issues](https://github.com/noahbutcher97/KatanaCombat_Demo/issues)
2. **Open a new issue**: Use template: "CI/CD Pipeline Issue"
3. **Submit a PR**: Improvements to the workflow are welcome!

---

**Last Updated**: 2026-01-31
**Workflow Version**: 1.0.0
