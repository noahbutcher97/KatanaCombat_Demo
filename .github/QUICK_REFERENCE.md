# CI/CD Quick Reference

Quick reference guide for developers working with the KatanaCombat GitHub Actions CI/CD pipeline.

---

## 🚀 Quick Start

### Running the Pipeline Manually
1. Go to [Actions Tab](https://github.com/noahbutcher97/KatanaCombat_Demo/actions)
2. Select **"UE5 CI - Build, Test, and Analyze"**
3. Click **"Run workflow"**
4. Select branch → Click **"Run workflow"**

### Checking Build Status
- View status badge in README (once configured)
- Check the [Actions tab](https://github.com/noahbutcher97/KatanaCombat_Demo/actions) for recent runs
- Pull request checks show status automatically

---

## 📋 What Gets Tested

| Stage | What It Does | Fails Build? |
|-------|--------------|--------------|
| **Build** | Compiles Win64 Development Editor | ✅ Yes |
| **Static Analysis** | Runs clang-tidy on C++ files | ❌ No (warnings only) |
| **Editor Validation** | Verifies assets load correctly | ❌ No (warnings only) |
| **Automation Tests** | Runs all KatanaCombat tests | ✅ Yes |

---

## 🔍 Common Scenarios

### Scenario 1: My PR Build Failed

**Steps to diagnose:**
1. Click the red ❌ next to your commit
2. Click "Details" to open the workflow run
3. Check which stage failed
4. Expand the failed step to see error logs

**Common causes:**
- **Build Failed**: Compilation error in your code
  - Fix: Review the build log for compiler errors
- **Tests Failed**: Unit test failure
  - Fix: Run tests locally, fix the failing test
- **Editor Validation Warning**: Asset corruption
  - Fix: Check the asset mentioned in the log

### Scenario 2: Running Tests Locally

Before pushing, run tests locally to catch issues early:

```powershell
# Navigate to project directory
cd C:\Path\To\KatanaCombat_Demo

# Run all tests
& "C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
    "KatanaCombat.uproject" `
    -ExecCmds="Automation RunTests KatanaCombat" `
    -unattended -nopause -nosplash -nullrhi -log
```

### Scenario 3: Understanding Artifacts

After a workflow run, artifacts are available at the bottom of the run page:

- **build-logs**: Compiler output and warnings
- **test-results**: Test execution logs and results
- **static-analysis**: clang-tidy warnings
- **validation-logs**: Asset validation output
- **compiled-binaries**: DLLs and PDBs (if build succeeded)

**Retention:** 7-30 days depending on artifact type

---

## ⚡ Performance Tips

### Speed Up Your Builds

1. **Use feature branches for WIP**
   - Push to `feature/*` branches don't trigger CI by default
   - Only PR to `main`/`develop` triggers full pipeline

2. **Skip CI for documentation changes**
   ```bash
   git commit -m "docs: Update README [skip ci]"
   ```

3. **Cache hits improve build time**
   - Avoid changing `.uproject`, `*.Build.cs`, `*.Target.cs` unnecessarily
   - Cache hit: ~5-10 min build time
   - Cache miss: ~15-30 min build time

---

## 🛠️ Workflow Configuration

### Key Environment Variables

Located at the top of `.github/workflows/ue5-ci.yml`:

```yaml
env:
  UE_VERSION: '5.6'              # Unreal Engine version
  PROJECT_NAME: 'KatanaCombat'   # Project name
  PLATFORM: 'Win64'               # Target platform
  CONFIGURATION: 'Development Editor' # Build config
```

### Trigger Conditions

**Automatic triggers:**
- Push to `main` or `develop`
- Pull request to `main` or `develop`

**Manual trigger:**
- `workflow_dispatch` (from Actions tab)

### Customizing Test Execution

To run specific test categories, edit Step 8 in the workflow:

```yaml
# Run all tests (default)
-ExecCmds=Automation RunTests KatanaCombat

# Run only combat component tests
-ExecCmds=Automation RunTests KatanaCombat.CombatComponent

# Run multiple test groups
-ExecCmds=Automation RunTests KatanaCombat.CombatComponent+KatanaCombat.Targeting
```

---

## 📊 Interpreting Results

### Build Summary

At the end of each workflow run, a summary is generated:

```
# 🎮 KatanaCombat CI/CD Pipeline Results

## Results Summary

| Stage | Status |
|-------|--------|
| Build | ✅ Success |
| Tests | ✅ Success |

## Test Results
- ✅ Passed: 47
- ❌ Failed: 0
```

### Exit Codes

| Exit Code | Meaning |
|-----------|---------|
| 0 | Success |
| 1 | Build failed or tests failed |
| Other | System error (runner issue) |

---

## 🐛 Troubleshooting

### Self-Hosted Runner Not Picking Up Jobs

**Symptoms:**
- Workflow shows "Waiting for a runner to pick up this job..."
- Jobs timeout after 30 minutes
- Runner appears online in GitHub settings

**Cause:** Missing `ue5` label on your runner

**Quick Fix:**
```powershell
cd D:\actions-runner  # Your runner location
.\svc.cmd stop
.\svc.cmd uninstall
.\config.cmd remove --token YOUR_REMOVAL_TOKEN
.\config.cmd --url https://github.com/noahbutcher97/KatanaCombat_Demo --token YOUR_TOKEN --labels self-hosted,Windows,ue5
.\svc.cmd install
.\svc.cmd start
```

**Verify:** Check Settings → Actions → Runners → Your runner shows labels: `self-hosted`, `Windows`, `ue5`

See [Setup Guide](.github/SETUP_GUIDE.md) for detailed instructions.

### Build Fails: "Unreal Engine not found"

**Cause:** Runner doesn't have UE5.6 installed or path is wrong

**Solution (for self-hosted runners):**
1. Install UE5.6 at `C:\Program Files\Epic Games\UE_5.6`
2. Or update the path in workflow file

### Tests Fail Locally But Pass in CI (or vice versa)

**Possible causes:**
- Different UE version
- Missing assets (not committed to Git)
- Different project settings

**Solution:**
1. Verify UE version matches: `5.6`
2. Check that all required assets are committed
3. Verify `.uproject` is committed with latest changes

### Static Analysis Shows Too Many Warnings

**To suppress warnings:**

Create or edit `.clang-tidy` in project root:

```yaml
Checks: '-readability-identifier-length,-modernize-use-trailing-return-type'
```

Or modify the workflow to skip specific checks.

### Workflow Takes Too Long

**Optimization steps:**
1. Enable parallel builds in UE settings
2. Use faster hardware for self-hosted runner
3. Review which tests are running (some may be slow)
4. Consider splitting into multiple workflows

---

## 📚 Additional Resources

### Documentation
- [Workflow README](.github/workflows/README.md) - Detailed workflow documentation
- [Setup Guide](.github/SETUP_GUIDE.md) - Complete setup instructions
- [Secrets Template](.github/SECRETS_TEMPLATE.md) - Security configuration

### External Links
- [GitHub Actions Docs](https://docs.github.com/en/actions)
- [Unreal Build Tool](https://docs.unrealengine.com/en-US/ProductionPipelines/BuildTools/UnrealBuildTool/)
- [UE Automation System](https://docs.unrealengine.com/en-US/TestingAndOptimization/Automation/)

---

## 🆘 Getting Help

### Quick Diagnostics

```powershell
# Check runner status (on runner machine)
cd C:\actions-runner
.\svc.cmd status

# Check disk space
Get-PSDrive C | Select-Object Used,Free

# Verify UE installation
Test-Path "C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor.exe"

# Test local build
& "C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat" `
    KatanaCombatEditor Win64 Development "$(Get-Location)\KatanaCombat.uproject" -WaitMutex
```

### Contact

For issues or questions:
1. Check the documentation first
2. Review workflow run logs
3. Open an issue with:
   - Workflow run URL
   - Error logs
   - Steps to reproduce

---

## ✅ Pre-Push Checklist

Before pushing changes that will trigger CI:

- [ ] Code compiles locally
- [ ] All tests pass locally
- [ ] No new compiler warnings
- [ ] Assets are committed (if new assets were added)
- [ ] `.uproject` is updated (if modules changed)
- [ ] Changes are on the correct branch

---

*Last Updated: 2026-01-31*
*For detailed information, see the [Setup Guide](.github/SETUP_GUIDE.md)*
