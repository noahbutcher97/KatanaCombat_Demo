# CI/CD Documentation Index

Complete documentation for the KatanaCombat GitHub Actions CI/CD pipeline.

---

## 📚 Documentation Overview

This directory contains all documentation related to the continuous integration and deployment pipeline for the KatanaCombat project.

### Quick Links

| Document | Purpose | Audience |
|----------|---------|----------|
| [Quick Reference](QUICK_REFERENCE.md) | Day-to-day usage guide | All developers |
| [Setup Guide](SETUP_GUIDE.md) | Complete installation instructions | DevOps / Admins |
| [Workflow README](workflows/README.md) | Detailed workflow documentation | Technical leads |
| [Secrets Template](SECRETS_TEMPLATE.md) | Security configuration | DevOps / Admins |
| [Status Badge](STATUS_BADGE.md) | README badge integration | Documentation maintainers |

---

## 🚀 Getting Started

### For Developers

**First time working with the CI/CD pipeline?** Start here:

1. Read the [Quick Reference](QUICK_REFERENCE.md) - 5 minutes
2. Understand what gets tested and when
3. Learn how to interpret build results
4. Know how to run tests locally before pushing

### For DevOps/Administrators

**Setting up the pipeline from scratch?** Follow this path:

1. Review [Setup Guide](SETUP_GUIDE.md) - Complete installation walkthrough
2. Prepare hardware according to [requirements](SETUP_GUIDE.md#minimum-hardware-requirements)
3. Install and configure the [self-hosted runner](SETUP_GUIDE.md#self-hosted-runner-setup)
4. Configure [secrets](SECRETS_TEMPLATE.md) if needed
5. Run a test workflow
6. Monitor and maintain

### For Technical Leads

**Understanding the pipeline architecture?** Explore:

1. [Workflow README](workflows/README.md) - Deep dive into pipeline stages
2. Review the [workflow file](workflows/ue5-ci.yml) - Actual implementation
3. Understand [customization options](workflows/README.md#customization)
4. Plan [future enhancements](workflows/README.md#future-enhancements)

---

## 📋 Pipeline Overview

### What It Does

The CI/CD pipeline automatically:

✅ **Builds** the project for Win64 Development Editor  
✅ **Analyzes** code with clang-tidy for quality issues  
✅ **Validates** editor can load all assets  
✅ **Tests** with Unreal Engine's Automation Framework  
✅ **Uploads** artifacts for debugging and sharing  
✅ **Caches** build outputs for faster subsequent runs  
✅ **Reports** results with detailed job summaries  

### When It Runs

- **Automatic**: Push to `main` or `develop` branches
- **Automatic**: Pull requests targeting `main` or `develop`
- **Manual**: Triggered from GitHub Actions tab

### Typical Timeline

| Stage | Duration (with cache) | Duration (no cache) |
|-------|----------------------|---------------------|
| Setup & Checkout | 1-2 min | 1-2 min |
| Project Generation | 1-2 min | 1-2 min |
| Build | 5-10 min | 15-30 min |
| Static Analysis | 2-5 min | 2-5 min |
| Editor Validation | 3-5 min | 3-5 min |
| Tests | 3-5 min | 3-5 min |
| Upload Artifacts | 1-2 min | 1-2 min |
| **Total** | **15-30 min** | **30-50 min** |

---

## 🏗️ Architecture

### Pipeline Stages

```
┌─────────────────┐
│   Checkout      │  Clone repository + LFS
└────────┬────────┘
         │
┌────────▼────────┐
│  Cache Restore  │  Restore build artifacts
└────────┬────────┘
         │
┌────────▼────────┐
│   Setup UE5.6   │  Configure environment
└────────┬────────┘
         │
┌────────▼────────┐
│ Generate Files  │  Create VS project files
└────────┬────────┘
         │
┌────────▼────────┐
│     Build       │  Compile Win64 Dev Editor ⚠️ FAILS BUILD
└────────┬────────┘
         │
┌────────▼────────┐
│ Static Analysis │  Run clang-tidy (warnings only)
└────────┬────────┘
         │
┌────────▼────────┐
│   Validation    │  Verify assets (warnings only)
└────────┬────────┘
         │
┌────────▼────────┐
│     Tests       │  Run automation tests ⚠️ FAILS BUILD
└────────┬────────┘
         │
┌────────▼────────┐
│   Artifacts     │  Upload logs, binaries, reports
└────────┬────────┘
         │
┌────────▼────────┐
│    Summary      │  Generate results report
└─────────────────┘
```

### Key Files

```
KatanaCombat_Demo/
├── .github/
│   ├── workflows/
│   │   ├── ue5-ci.yml          # Main workflow definition
│   │   └── README.md            # Workflow documentation
│   ├── SETUP_GUIDE.md           # Setup instructions
│   ├── QUICK_REFERENCE.md       # Developer guide
│   ├── SECRETS_TEMPLATE.md      # Security configuration
│   ├── STATUS_BADGE.md          # Badge integration
│   └── README.md                # This file
├── .clang-tidy                  # Static analysis config
└── .gitignore                   # Excludes build artifacts
```

---

## 🔧 Configuration

### Environment Variables

Set at the top of [ue5-ci.yml](workflows/ue5-ci.yml):

```yaml
env:
  UE_VERSION: '5.6'                    # Must match .uproject
  PROJECT_NAME: 'KatanaCombat'         # Project name
  PLATFORM: 'Win64'                     # Target platform
  CONFIGURATION: 'Development Editor'   # Build config
```

### Trigger Configuration

Current triggers in [ue5-ci.yml](workflows/ue5-ci.yml):

```yaml
on:
  push:
    branches: [ main, develop ]
  pull_request:
    branches: [ main, develop ]
  workflow_dispatch:  # Manual trigger
```

### Caching Strategy

The workflow caches:
- `Intermediate/` - Compiled objects
- `DerivedDataCache/` - Unreal's DDC
- `Saved/BuildGraph/` - Build graph cache

Cache key based on:
- OS (Windows)
- UE version (5.6)
- Build configuration files (*.uproject, *.Build.cs, *.Target.cs)

---

## 🎯 Common Tasks

### How to...

#### ...Run the workflow manually
1. Go to [Actions tab](https://github.com/noahbutcher97/KatanaCombat_Demo/actions)
2. Select "UE5 CI - Build, Test, and Analyze"
3. Click "Run workflow"
4. Select branch → Run

#### ...Debug a failed build
1. Click the ❌ in your PR or commit
2. Click "Details" to open the run
3. Find the failed step (highlighted in red)
4. Expand the step to see error logs
5. Download artifacts for detailed logs

#### ...Add the CI badge to README
See [Status Badge Guide](STATUS_BADGE.md)

#### ...Skip CI for a commit
```bash
git commit -m "docs: Update documentation [skip ci]"
```

#### ...Run tests locally
```powershell
& "C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
    "KatanaCombat.uproject" `
    -ExecCmds="Automation RunTests KatanaCombat" `
    -unattended -nosplash -nullrhi -log
```

#### ...Configure secrets
See [Secrets Template](SECRETS_TEMPLATE.md)

---

## 📊 Artifacts

After each workflow run, the following artifacts are available:

| Artifact | Contents | Retention | Use Case |
|----------|----------|-----------|----------|
| `build-logs-<sha>` | Compiler output, warnings | 14 days | Debug build failures |
| `test-results-<sha>` | Test logs, JSON reports | 30 days | Analyze test failures |
| `static-analysis-<sha>` | clang-tidy warnings | 14 days | Code quality review |
| `validation-logs-<sha>` | Asset validation output | 14 days | Asset issues |
| `compiled-binaries-<sha>` | DLLs, PDBs | 7 days | Debug/testing |

**Accessing artifacts:**
1. Go to workflow run page
2. Scroll to "Artifacts" section at the bottom
3. Click to download

---

## 🐛 Troubleshooting

### Quick Diagnostics

| Symptom | Likely Cause | Solution |
|---------|--------------|----------|
| Build fails: "UE not found" | UE5.6 not installed on runner | Install UE5.6 or update path |
| Tests fail in CI but pass locally | Version mismatch or missing assets | Verify UE version, commit all assets |
| Workflow takes > 1 hour | No cache hits, slow runner | Check cache configuration, upgrade hardware |
| Static analysis errors | Code doesn't meet standards | Review clang-tidy output, fix warnings |
| Artifacts missing | Step didn't run or failed | Check step status, review logs |

**For detailed troubleshooting, see:**
- [Quick Reference - Troubleshooting](QUICK_REFERENCE.md#troubleshooting)
- [Setup Guide - Troubleshooting](SETUP_GUIDE.md#troubleshooting-common-issues)

---

## 🔐 Security

### Best Practices

✅ **DO:**
- Store sensitive values in GitHub Secrets
- Use dedicated accounts for CI/CD
- Enable 2FA on all accounts
- Rotate secrets regularly
- Review workflow permissions
- Audit secret access

❌ **DON'T:**
- Commit secrets to repository
- Use personal accounts for automation
- Share secrets between environments
- Log secret values
- Grant excessive permissions

### Secrets Management

See [Secrets Template](SECRETS_TEMPLATE.md) for:
- Available secret types
- Configuration instructions
- Security best practices
- Usage examples

---

## 🚀 Future Enhancements

### Planned Features

- [ ] Multi-platform builds (Linux, macOS)
- [ ] Automated packaging for distribution
- [ ] Performance profiling integration
- [ ] Code coverage reporting
- [ ] SonarQube integration
- [ ] Slack/Discord notifications
- [ ] Automated changelog generation
- [ ] Steam/Epic Games Store deployment
- [ ] Nightly build artifacts
- [ ] Release automation

### Contributing

To suggest improvements:
1. Open an issue describing the enhancement
2. Discuss implementation approach
3. Submit a PR with changes
4. Update documentation

---

## 📞 Support

### Getting Help

**For developers:**
- Check [Quick Reference](QUICK_REFERENCE.md)
- Review workflow run logs
- Search existing issues

**For administrators:**
- Review [Setup Guide](SETUP_GUIDE.md)
- Check runner health
- Verify configuration

**For urgent issues:**
1. Gather diagnostic information
2. Check recent changes
3. Open an issue with:
   - Workflow run URL
   - Error logs
   - Steps to reproduce
   - System information

### Resources

- [GitHub Actions Docs](https://docs.github.com/en/actions)
- [Unreal Engine Docs](https://docs.unrealengine.com/)
- [UnrealBuildTool Reference](https://docs.unrealengine.com/en-US/ProductionPipelines/BuildTools/UnrealBuildTool/)
- [Automation Testing](https://docs.unrealengine.com/en-US/TestingAndOptimization/Automation/)

---

## ✅ Quick Start Checklist

### For First-Time Setup

- [ ] Review this README
- [ ] Read the [Setup Guide](SETUP_GUIDE.md)
- [ ] Install UE5.6 on build machine
- [ ] Configure self-hosted runner
- [ ] Test local build
- [ ] Run test workflow
- [ ] Add status badge to README
- [ ] Document any customizations
- [ ] Train team on CI/CD usage

### For Daily Development

- [ ] Read [Quick Reference](QUICK_REFERENCE.md)
- [ ] Run tests locally before pushing
- [ ] Monitor CI results on PRs
- [ ] Download artifacts if build fails
- [ ] Report any pipeline issues

---

*Last Updated: 2026-01-31*
*Version: 1.0*

For questions or suggestions, open an issue or contact the DevOps team.
