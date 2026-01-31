# GitHub Actions CI/CD Infrastructure

This directory contains the GitHub Actions workflow configurations and supporting scripts for the KatanaCombat project.

## 🎯 Overview

The CI/CD pipeline provides comprehensive automated building, testing, and validation for the Unreal Engine 5.6 project with:

- ✅ **Dual Runner Support**: Works with both self-hosted and GitHub-hosted runners
- ✅ **Automatic Fallback**: Tries self-hosted first, falls back to GitHub-hosted if unavailable
- ✅ **Complete Cloud Independence**: Can operate without any local infrastructure
- ✅ **Mobile-Friendly**: Trigger and monitor builds from GitHub Mobile app
- ✅ **Comprehensive Testing**: Automation tests, asset validation, and static analysis
- ✅ **Smart Caching**: Optimized caching strategies for fast incremental builds
- ✅ **Artifact Management**: Configurable retention policies for logs, test results, and binaries

## 📁 Structure

```
.github/
├── workflows/
│   ├── ue5-ci.yml          # Main CI/CD pipeline workflow
│   ├── README.md            # Workflow documentation
│   └── QUICKSTART.md        # Quick setup guide
├── scripts/
│   └── setup-ue5.ps1        # PowerShell script for automated UE5 setup
├── SETUP_GUIDE.md           # Detailed setup instructions
├── IMPLEMENTATION_SUMMARY.md # Technical implementation details
├── QUICK_REFERENCE.md       # Common operations reference
├── SECRETS_TEMPLATE.md      # Secrets configuration guide
└── STATUS_BADGE.md          # Status badge setup
```

## 🚀 Quick Start

### For Self-Hosted Runners (Recommended)

1. **Install Prerequisites**:
   - Windows 10/11 or Windows Server 2019/2022
   - Unreal Engine 5.6 (via Epic Games Launcher)
   - Visual Studio 2022 with C++ game development workload
   - Git with LFS enabled

2. **Setup Runner**:
   ```powershell
   # Download and configure runner
   # Add labels: [self-hosted, Windows, ue5]
   ```

3. **Verify Installation**:
   - UE5.6 at: `C:\Program Files\Epic Games\UE_5.6`
   - MSBuild available in PATH

### For GitHub-Hosted Runners

GitHub-hosted runners require UE5.6 to be available. Options:

1. **Use Self-Hosted** (Recommended)
2. **Custom Docker Image** with pre-installed UE5.6
3. **Automated Installation** (requires `UE_DOWNLOAD_URL` secret)

See [SETUP_GUIDE.md](SETUP_GUIDE.md) for detailed instructions.

## 📋 Workflows

### Main CI/CD Pipeline (`ue5-ci.yml`)

**Triggers**:
- Push to `main`, `develop`, or feature branches
- Pull requests to `main` or `develop`
- Manual dispatch via Actions tab or GitHub Mobile

**Jobs**:
1. **Environment Detection**: Automatically selects optimal runner type
2. **Build & Test (Self-Hosted)**: Executes on self-hosted runners if available
3. **Build & Test (GitHub-Hosted)**: Fallback to GitHub-hosted runners
4. **Report Results**: Aggregates results and posts PR comments

**Fallback Mechanism**:

The pipeline implements intelligent fallback to prevent workflow stalling:

- **Self-hosted job** (`timeout-minutes: 60`):
  - Attempts to run first when `runner-type` is `auto` (default)
  - 60-minute timeout prevents indefinite waiting on unavailable runners
  - `continue-on-error: true` allows workflow to proceed even on failure
  - If runner is unavailable, times out, or fails → triggers GitHub-hosted

- **GitHub-hosted job** (`timeout-minutes: 180`):
  - Runs conditionally: `if self-hosted.result in ['skipped', 'failure', 'timeout']`
  - Longer timeout accommodates automated VS2022 and UE5.6 setup
  - Provides cloud-based builds without local infrastructure dependency
  - Falls back gracefully when self-hosted infrastructure is down

- **Success criteria**:
  - Workflow succeeds if **either** runner completes successfully
  - Both runners can run in parallel when forced via workflow inputs
  - Results aggregated and posted to PR comments

**Stages**:
- 📥 Checkout with Git LFS
- 💾 Restore build cache
- 🎮 Detect/Setup Unreal Engine
- 🔨 Setup MSBuild
- 📦 Generate project files
- 🏗️ Compile project (`Win64 Development Editor`)
- 🔍 Static analysis with clang-tidy
- 📋 Asset validation via ResavePackages
- 🧪 Automation tests (headless, NullRHI)
- 📤 Upload artifacts (logs, tests, binaries)

**Build Times & Timeouts**:
- Self-Hosted (cached): 3-5 minutes (timeout: 60 minutes)
- Self-Hosted (cold): 15-25 minutes (timeout: 60 minutes)
- GitHub-Hosted: 40-60 minutes initial setup (timeout: 180 minutes)

**Note**: Self-hosted timeout set to 60 minutes for faster failure detection and fallback to GitHub-hosted when runners are unavailable.

## 🎮 Using from GitHub Mobile

The workflow is fully mobile-compatible:

1. Open GitHub Mobile app
2. Navigate to repository → Actions
3. Select "UE5.6 CI/CD - Comprehensive Pipeline"
4. Tap "Run workflow"
5. Choose options:
   - Runner type (auto/self-hosted/github-hosted)
   - Build configuration
   - Skip tests/validation options
6. Monitor progress in real-time
7. View logs and download artifacts

## 🔐 Secrets Configuration

Optional secrets for extended functionality:

| Secret | Purpose | Required |
|--------|---------|----------|
| `UE_DOWNLOAD_URL` | Automated UE5.6 installation | Optional |
| `EPIC_API_KEY` | Epic Games API access | Optional |
| `STEAM_USERNAME` | Steam deployment | Optional |
| `STEAM_PASSWORD` | Steam deployment | Optional |
| `DEPLOY_TOKEN` | Custom deployment endpoints | Optional |

See [SECRETS_TEMPLATE.md](SECRETS_TEMPLATE.md) for details.

## 📊 Status Badge

Add to your README.md:

```markdown
[![UE5 CI](https://github.com/noahbutcher97/KatanaCombat_Demo/actions/workflows/ue5-ci.yml/badge.svg)](https://github.com/noahbutcher97/KatanaCombat_Demo/actions/workflows/ue5-ci.yml)
```

## 📦 Artifacts

The pipeline generates and uploads:

| Artifact | Retention | Description |
|----------|-----------|-------------|
| Build Logs | 14 days | Complete build output and error logs |
| Test Results | 30 days | Automation test reports (JSON) |
| Binaries | 7 days | Compiled editor and game binaries |

## 🛠️ Customization

### Modify Build Configuration

Edit workflow inputs or environment variables:

```yaml
env:
  BUILD_CONFIG: Development  # or DebugGame, Shipping
  PLATFORM: Win64
  TARGET: KatanaCombatEditor
```

### Add Custom Build Steps

Insert steps in the appropriate job:

```yaml
- name: Custom Build Step
  shell: pwsh
  run: |
    # Your custom logic here
```

### Configure Caching

Adjust cache paths and keys for your project structure:

```yaml
- name: Cache Dependencies
  uses: actions/cache@v4
  with:
    path: |
      Intermediate
      YourCustomCachePath
    key: custom-key-${{ hashFiles('**/*.uproject') }}
```

## 📚 Documentation

- [**QUICKSTART.md**](workflows/QUICKSTART.md) - Get started in 5 minutes
- [**SETUP_GUIDE.md**](SETUP_GUIDE.md) - Comprehensive setup instructions
- [**QUICK_REFERENCE.md**](QUICK_REFERENCE.md) - Common operations and troubleshooting
- [**IMPLEMENTATION_SUMMARY.md**](IMPLEMENTATION_SUMMARY.md) - Technical architecture
- [**Workflow README**](workflows/README.md) - Workflow-specific documentation

## 🤝 Contributing

When modifying workflows:

1. Test changes in a feature branch
2. Verify YAML syntax: `yamllint .github/workflows/*.yml`
3. Test with manual workflow dispatch before merging
4. Update documentation to reflect changes
5. Add/update status checks as needed

## 🐛 Troubleshooting

### Build Fails to Find UE5.6

**Self-Hosted**:
- Verify UE5.6 is installed at: `C:\Program Files\Epic Games\UE_5.6`
- Check runner has `ue5` label
- Restart runner service

**GitHub-Hosted**:
- Configure `UE_DOWNLOAD_URL` secret
- Or use self-hosted runner (recommended)

### Cache Not Restoring

- Check cache key matches
- Verify paths are correct
- Cache may be expired (7 days max)
- Try clearing cache via Actions settings

### Tests Timeout

- Increase `timeout-minutes` in job configuration
- Check for hanging tests in logs
- Verify NullRHI is working correctly

### Static Analysis Errors

- Review `.clang-tidy` configuration
- Check clang-tidy is in PATH
- Some warnings are informational only

## 📞 Support

- **Issues**: Create an issue in this repository
- **Discussions**: Use GitHub Discussions for questions
- **Documentation**: Check the docs/ directory

## 🔄 Updates

This pipeline consolidates features from:
- PR #6: Core CI implementation
- PR #7: Dual-runner support
- PR #8: Cloud independence and mobile support

Last Updated: 2026-01-31
