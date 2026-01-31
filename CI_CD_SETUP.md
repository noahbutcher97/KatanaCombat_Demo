# 🚀 CI/CD Pipeline Setup Guide

This guide explains how to use the fully autonomous GitHub Actions CI/CD pipeline for the KatanaCombat UE5.6 project.

## 📱 Mobile-First Development

This pipeline is designed to enable development directly from your phone:

1. **Make code changes** via GitHub mobile app or GitHub.dev
2. **Trigger builds** automatically on push or manually via workflow_dispatch
3. **Monitor progress** in real-time through GitHub mobile
4. **Review results** via PR comments with formatted status
5. **Download artifacts** (logs, test results, binaries) directly from mobile

## 🎯 Quick Start

### Option 1: Self-Hosted Runner (Recommended)

**Advantages:**
- ⚡ Fastest builds (3-5 min with cache)
- 💰 No GitHub Actions minutes consumed
- 🔧 Full UE5.6 control

**Setup:**
1. Install UE5.6 on a Windows machine
2. Follow GitHub's [self-hosted runner setup](https://docs.github.com/en/actions/hosting-your-own-runners/adding-self-hosted-runners)
3. Label your runner: `self-hosted`, `Windows`, `ue5`
4. Push code → CI runs automatically! ✅

### Option 2: GitHub-Hosted Runner (Fully Autonomous)

**Advantages:**
- 🌐 Zero local setup required
- 📱 100% mobile-friendly
- ☁️ Works from anywhere

**Setup:**
1. **(Optional)** Add `EPIC_API_KEY` to repository secrets for UE5 auto-install
2. Push code → CI runs automatically! ✅

**Note:** First run takes 35-45 minutes (VS + UE5 install), then 10-15 min with cache.

### Option 3: Hybrid (Auto-Detection)

The workflow automatically tries self-hosted first, then falls back to GitHub-hosted if unavailable.

## 🎮 Manual Trigger (Mobile)

1. Go to **Actions** tab (mobile app or web)
2. Select **"UE5.6 CI Pipeline"** workflow
3. Click **"Run workflow"**
4. Configure options:
   - Skip tests (faster builds)
   - Skip asset validation
   - Choose build configuration (Development/DebugGame/Shipping)
   - Force runner type (self-hosted/windows-latest)
5. Click **"Run workflow"** button

## 📊 What Gets Built & Tested

### Build Targets
- **KatanaCombat Editor** (Win64 Development)
  - Main game module
  - Editor-only module
  - Test module (126+ automation tests)

### Automated Tests
The pipeline runs **126+ automation tests** across 14 test suites:
- State Transition Tests
- Input Buffering Tests
- Hold Window Tests
- Parry Detection Tests
- Attack Execution Tests
- Phases vs Windows Tests
- Targeting Component Tests
- Weapon Component Tests
- Hit Reaction Tests
- Damage Application Tests
- Death System Tests
- Combat Integration Tests
- Debug Visualization Tests
- Memory Safety Tests

**Command:** `UnrealEditor-Cmd.exe -ExecCmds="Automation RunTests KatanaCombat;Quit" -NullRHI`

### Asset Validation
Validates all project assets using UE5's ResavePackages commandlet.

## 💾 Caching Strategy

The workflow intelligently caches:

| Cache Type | Location | Speed Improvement |
|------------|----------|-------------------|
| Intermediate Build | `Intermediate/Build/` | 3-4x faster |
| Derived Data Cache | `DerivedDataCache/` | 2-3x faster |
| VS Build Tools | `C:\BuildTools\` | GitHub-hosted only |
| UE5 Installation | `C:\UE5\` | GitHub-hosted only |

**Cache Invalidation:** Automatic when .uproject, .Build.cs, or source files change.

## 📦 Artifacts

All builds upload artifacts with smart retention:

| Artifact | Contents | Retention | Use Case |
|----------|----------|-----------|----------|
| **build-logs** | `Saved/Logs/`, `*.log` | 14 days | Debugging build failures |
| **test-results** | `Saved/Automation/Reports/` | 30 days | Test failure analysis |
| **binaries** | Editor DLLs, EXEs | 7 days | Quick testing without rebuild |
| **build-stats** | JSON stats (timing, disk space) | 30 days | Performance tracking |
| **build-report** | Markdown summary | 90 days | Historical reference |

**Download from mobile:** Actions tab → Select run → Artifacts section

## 🔐 Security & Secrets

### Optional Secrets

Add these in **Settings → Secrets and variables → Actions**:

| Secret | Purpose | Required? |
|--------|---------|-----------|
| `EPIC_API_KEY` | Automated UE5 install on GitHub-hosted | Optional |
| `STEAM_CREDENTIALS` | Future: Steam deployment | No |
| `DEPLOY_KEY` | Future: Deployment automation | No |

### Permissions

The workflow uses **least-privilege** permissions:
- **Read** access to repository contents
- **Write** access only for PR comments
- **No** access to other resources

## 📈 Build Status

### Via PR Comments

On pull requests, the workflow automatically posts:
```markdown
# 🏗️ Build Results

✅ **Build Successful**

| Runner | Status |
|--------|--------|
| Self-Hosted | success |
| GitHub-Hosted | skipped |

📦 **Artifacts**: Available in Actions tab
```

### Via Actions Tab

1. Go to **Actions** tab
2. See workflow runs with status icons:
   - ✅ Green = Success
   - ❌ Red = Failure
   - 🟡 Yellow = In Progress
   - ⚫ Gray = Cancelled/Skipped

## 🛠️ Troubleshooting

### Build Fails on GitHub-Hosted Runner

**Symptom:** "UE5 installation not found"

**Solution:**
1. Use self-hosted runner (recommended), OR
2. Add `EPIC_API_KEY` secret, OR
3. Pre-cache UE5 installation (see [README.md](.github/workflows/README.md))

### Tests Fail but Build Succeeds

**Symptom:** Green build, red tests

**Cause:** Tests are `continue-on-error: true` by design

**Solution:**
1. Download test-results artifact
2. Review `Saved/Automation/Reports/index.json`
3. Fix failing tests
4. Re-run workflow

### Cache Not Working

**Symptom:** Every build is slow (cold build)

**Check:**
1. Actions cache size (limit: 10GB)
2. Cache key matches (check workflow logs)
3. Branch naming (feature branches share cache with main)

**Solution:**
- Clear old caches: Settings → Actions → Caches
- Verify file hashes in cache key

### Build Timeout

**Symptom:** Workflow cancelled after 180 min

**Solution:**
- Increase `timeout-minutes` in workflow
- Use faster runner (self-hosted)
- Skip tests: `skip_tests: true`

## 📚 Documentation

- **[.github/workflows/README.md](.github/workflows/README.md)** - Detailed workflow documentation
- **[.github/workflows/QUICKSTART.md](.github/workflows/QUICKSTART.md)** - Mobile quick reference
- **[docs/GETTING_STARTED.md](docs/GETTING_STARTED.md)** - Project setup guide
- **[Source/KatanaCombatTest/README.md](Source/KatanaCombatTest/README.md)** - Test suite documentation

## 🎯 Advanced Usage

### Custom Build Configuration

Trigger with DebugGame or Shipping configuration:

```yaml
# Via workflow_dispatch
build_configuration: Shipping
```

### Skip Tests for Faster Iteration

```yaml
# Via workflow_dispatch
skip_tests: true
skip_asset_validation: true
```

### Force Specific Runner

```yaml
# Via workflow_dispatch
runner_type: self-hosted  # or windows-latest
```

## 🌟 Best Practices

1. **Always review PR comments** for build status before merging
2. **Check test results** even if build succeeds (tests are `continue-on-error`)
3. **Use self-hosted runners** for production workflows (faster + cheaper)
4. **Keep caches warm** by running CI regularly (max 7 day cache lifetime)
5. **Download artifacts promptly** (14-30 day retention limits)
6. **Use workflow_dispatch** for manual testing before pushing

## 🚦 CI/CD Pipeline Status

| Check | Status |
|-------|--------|
| Workflow Syntax | ✅ Valid YAML |
| Security Scan | ✅ 0 CodeQL alerts |
| Permissions | ✅ Least privilege |
| Mobile Support | ✅ Full support |
| Self-Hosted | ✅ Ready |
| GitHub-Hosted | ⚠️ Requires EPIC_API_KEY |
| Documentation | ✅ Complete |

---

**Ready to build from your phone!** 📱🚀

Push code to any branch and watch the magic happen. Questions? See [.github/workflows/README.md](.github/workflows/README.md) for full documentation.
