# Quick Start: GitHub Actions CI for KatanaCombat

## 🚀 TL;DR
The CI pipeline automatically builds, tests, and validates your UE5.6 project on every push and PR. Works with both self-hosted and GitHub-hosted Windows runners.

---

## For Self-Hosted Runners

### Prerequisites
```powershell
# 1. Install UE 5.6 to default location
C:\Program Files\Epic Games\UE_5.6

# 2. Install Visual Studio 2022 (any edition)
# Include: Desktop C++ Development + Windows SDK

# 3. Setup GitHub runner
cd C:\actions-runner
.\config.cmd --url https://github.com/YOUR_ORG/KatanaCombat_Demo --token YOUR_TOKEN
.\svc.cmd install
.\svc.cmd start
```

### Verify Setup
```powershell
# Check UE
Test-Path "C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat"

# Check Runner
Get-Service actions.runner.*
```

**Done!** Push code and CI runs automatically.

---

## For GitHub-Hosted Runners

### Quick Setup
1. **Option A**: Use self-hosted (recommended)
2. **Option B**: Configure secrets for UE download:
   - `Settings` → `Secrets` → Add `EPIC_API_KEY`
   - Uncomment UE installation step in workflow

**Note:** GitHub runners don't have UE pre-installed. Builds will take 45+ min on first run.

---

## Usage

### Automatic
```bash
# Just push code
git push

# Or create PR
gh pr create
```

### Manual (Mobile-Friendly)
**From GitHub Mobile App:**
1. Open repo → Actions
2. Tap "UE5.6 CI Pipeline"
3. Tap "Run workflow"
4. Select options → Run

**From CLI:**
```bash
gh workflow run ue5-ci.yml
```

**From Web:**
1. Navigate to `Actions` tab
2. Click "UE5.6 CI Pipeline"
3. Click "Run workflow"
4. Configure and run

---

## What Gets Built

### On Every Push/PR
✅ Build Win64 Development Editor
✅ Run 126+ automation tests (NullRHI)
✅ Generate build reports

### On PRs Only
✅ Asset validation (ResavePackages)
✅ PR comment with results

### Artifacts Uploaded
- 📄 Build logs (14 days)
- 🧪 Test results (30 days)
- 📦 Binaries (7 days, main branch only)
- 📊 Build stats (30 days)

---

## Configuration Options

### Skip Tests
```bash
gh workflow run ue5-ci.yml -f skip_tests=true
```

### Change Build Config
```bash
gh workflow run ue5-ci.yml -f build_configuration=Shipping
```

### Force Runner Type
```bash
gh workflow run ue5-ci.yml -f runner_type=self-hosted
```

---

## Typical Build Times

| Scenario | Self-Hosted | GitHub-Hosted |
|----------|-------------|---------------|
| Cold build | 10-15 min | 35-45 min |
| Warm build (cached) | 3-5 min | 10-15 min |
| Tests only | 5-10 min | 5-10 min |

**Tip:** Use caching for 3-5x faster builds!

---

## Troubleshooting

### Build Failed?
1. Check build log artifact
2. Look for "ERROR:" entries
3. Reproduce locally:
   ```powershell
   & "C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat" `
     KatanaCombatEditor Win64 Development -project="KatanaCombat.uproject"
   ```

### Tests Failed?
1. Download `test-results-{run}` artifact
2. Open `AutomationTests-{run}.log`
3. Search for "FAILED:" entries
4. Run locally:
   ```powershell
   & UnrealEditor-Cmd.exe KatanaCombat.uproject `
     -ExecCmds="Automation RunTests KatanaCombat; Quit" -NullRHI
   ```

### Cache Not Working?
- Check if source files are changing unexpectedly
- Clear cache: Settings → Actions → Caches → Delete
- Next run will recreate cache

---

## Key Files

```
.github/workflows/
├── ue5-ci.yml          # Main workflow file
├── README.md           # Full documentation (you are here in the quick start)
└── QUICKSTART.md       # This file
```

---

## Common Scenarios

### Scenario 1: Feature Branch
```bash
# Make changes
git checkout -b feature/new-attack
# Edit code
git commit -m "feat: Add spinning attack"
git push origin feature/new-attack

# CI runs automatically:
# ✅ Build
# ✅ Tests
# ❌ Asset validation (PR only)
# ❌ Binary upload (main only)
```

### Scenario 2: Pull Request
```bash
gh pr create --title "New Attack" --body "Adds spinning attack"

# CI runs with full validation:
# ✅ Build
# ✅ Tests
# ✅ Asset validation
# ✅ PR comment with results
```

### Scenario 3: Mobile Edit
1. Open GitHub mobile app
2. Browse to file → Edit
3. Make change → Commit
4. Navigate to Actions
5. Tap "Run workflow"
6. Monitor progress
7. View PR comment for results

---

## Need Help?

- 📖 **Full Docs**: See `.github/workflows/README.md`
- 🐛 **Bug Reports**: GitHub Issues
- 💬 **Questions**: GitHub Discussions
- 📧 **Support**: team@example.com

---

## Quick Reference

| Action | Command |
|--------|---------|
| Run workflow | `gh workflow run ue5-ci.yml` |
| List runs | `gh run list` |
| View run | `gh run view {run_id}` |
| Download artifacts | `gh run download {run_id}` |
| Watch run | `gh run watch {run_id}` |
| Cancel run | `gh run cancel {run_id}` |

---

**That's it!** You're ready to use the CI pipeline. For advanced configuration, see the full README.

*Version: 1.0.0 | Last Updated: 2024-01-XX*
