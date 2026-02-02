# CI/CD Quick Reference

Quick reference guide for common CI/CD operations in the KatanaCombat project.

## Running Workflows

### Trigger a Build Manually

1. Navigate to repository: https://github.com/noahbutcher97/KatanaCombat_Demo
2. Click **Actions** tab
3. Select **UE5 CI/CD Pipeline** from the left sidebar
4. Click **Run workflow** button
5. Configure options:
   - **Branch**: Select branch to build (e.g., `main`, `develop`, `feature/my-feature`)
   - **Runner type**: Choose `auto`, `self-hosted`, or `github-hosted`
6. Click **Run workflow**

### Check Build Status

**For Pull Requests:**
- Status checks appear automatically at the bottom of the PR
- Green checkmark ✅ = Build passed
- Red X ❌ = Build failed
- Yellow circle 🟡 = Build in progress

**For All Builds:**
- Go to **Actions** tab
- Recent workflow runs are listed with status icons
- Click on a run to see detailed logs

## Viewing Build Results

### Download Artifacts

1. Click on a completed workflow run
2. Scroll to bottom → **Artifacts** section
3. Click artifact name to download:
   - `build-logs-*` - Compiler and build logs
   - `test-results-*` - Test reports
   - `binaries-*` - Compiled executables
   - `build-stats-*` - Performance metrics

### View Build Logs

1. Click on workflow run
2. Click on job name (e.g., "Build (Self-Hosted)")
3. Expand steps to view detailed logs
4. Use search (Ctrl+F) to find specific errors

## Common Scenarios

### Scenario: Build Only on Self-Hosted Runner

**When**: You want to test only on your local infrastructure

**Steps**:
1. Actions → UE5 CI/CD Pipeline → Run workflow
2. Set **runner_type** to `self-hosted`
3. Run workflow

### Scenario: Test GitHub-Hosted Build

**When**: Testing cloud-based builds or validating GitHub Actions setup

**Steps**:
1. Actions → UE5 CI/CD Pipeline → Run workflow
2. Set **runner_type** to `github-hosted`
3. Run workflow (first run will be slow due to UE5 installation)

### Scenario: Automatic Build on PR

**When**: Pull request created or updated

**Behavior**:
- Builds automatically trigger
- Uses auto mode (self-hosted with fallback)
- Results appear as PR status checks

### Scenario: Automatic Build on Push to Main

**When**: Code pushed directly to `main` or `develop`

**Behavior**:
- Build triggers automatically
- Uses auto mode
- Can be used for deployment triggers

## Troubleshooting

### Build Failed: "No runner available"

**Problem**: No self-hosted runners online

**Quick Fix**:
- Option 1: Re-run workflow with `github-hosted` runner type
- Option 2: Check self-hosted runner status and restart
- Option 3: Wait for self-hosted runner to come online (auto mode will retry)

### Build Failed: "UE5 not found"

**Self-Hosted**: 
- Verify UE5 installed at `C:\Program Files\Epic Games\UE_5.6`
- See [SETUP_GUIDE.md](SETUP_GUIDE.md)

**GitHub-Hosted**:
- Add `EPIC_API_KEY` secret
- Or wait for cache to populate

### Tests Failed in CI but Pass Locally

**Cause**: Environment differences

**Debug Steps**:
1. Download `test-results-*` artifact
2. Review JSON reports for failure details
3. Run tests locally with `-NullRHI` flag
4. Check for GPU/rendering dependencies

### Slow Build Times

**For Self-Hosted**:
- Ensure DDC caching is configured (see SETUP_GUIDE.md)
- Check disk I/O performance
- Review Windows Defender exclusions

**For GitHub-Hosted**:
- First build is always slow (UE5 installation)
- Subsequent builds use cache (much faster)
- Consider using self-hosted for regular builds

## Configuration

### Add New Secret

1. Settings → Secrets and variables → Actions
2. Click **New repository secret**
3. Enter name and value
4. Secrets available:
   - `EPIC_API_KEY` - For UE5 installation
   - `STEAM_USERNAME` / `STEAM_PASSWORD` - For Steam deployment
   - `DEPLOY_TOKEN` - For custom deployment

### Modify Build Configuration

Edit `.github/workflows/ue5-ci.yml`:

```yaml
env:
  PROJECT_NAME: KatanaCombat           # Project name
  UE_VERSION: "5.6"                    # UE version
  BUILD_CONFIGURATION: Development Editor  # Build type
```

### Add Custom Build Step

Insert between existing steps:

```yaml
- name: Custom Build Step
  shell: pwsh
  run: |
    Write-Host "Running custom script..."
    .\Scripts\MyScript.ps1
```

## Runner Management

### Check Self-Hosted Runner Status

**Via GitHub UI**:
1. Settings → Actions → Runners
2. View runner status (Idle/Active/Offline)

**Via Runner Machine**:
```powershell
cd C:\actions-runner
.\svc.sh status
```

### Restart Self-Hosted Runner

```powershell
cd C:\actions-runner
.\svc.sh stop
.\svc.sh start
```

### Update Self-Hosted Runner

1. Stop service: `.\svc.sh stop`
2. Download latest runner from GitHub
3. Extract and replace files
4. Start service: `.\svc.sh start`

## Performance Tips

### Speed Up Self-Hosted Builds

1. **Enable DDC caching** (see SETUP_GUIDE.md)
2. **Exclude from antivirus**:
   - UE5 installation directory
   - Build workspace directory
3. **Use NVMe SSD** for build directories
4. **Increase RAM** (64GB recommended)

### Speed Up GitHub-Hosted Builds

1. **Cache UE5 installation** (automatic after first run)
2. **Minimize package updates** in project
3. **Use incremental builds** when possible

## Monitoring

### View Build Statistics

Download `build-stats-*` artifact for:
- Build time
- Runner type used
- Success/failure status
- Disk usage (GitHub-hosted)

### Check Cache Effectiveness

In workflow logs, look for:
- `Cache restored successfully` (cache hit)
- `Cache miss` (no cache available)

### Monitor Artifact Storage

**Check usage**:
1. Settings → Actions → General
2. View storage usage under "Artifact and log retention"

**Cleanup old artifacts**:
- Automatic based on retention policy
- Manual: Actions → Workflow run → Delete artifacts

## Best Practices

### For Contributors

✅ **Always create PR** before merging to main
✅ **Wait for CI checks** to pass before merging
✅ **Review build logs** if tests fail
✅ **Use descriptive commit messages** for better logs

### For Maintainers

✅ **Monitor self-hosted runner health** regularly
✅ **Update runner software** when GitHub releases updates
✅ **Review failed builds** within 24 hours
✅ **Clean build cache** monthly on self-hosted runners
✅ **Keep UE5 up to date** on self-hosted runners

## Links

- **Full CI/CD Documentation**: [docs/CI_CD_GUIDE.md](CI_CD_GUIDE.md)
- **Self-Hosted Setup**: [docs/SETUP_GUIDE.md](SETUP_GUIDE.md)
- **GitHub Actions Docs**: https://docs.github.com/en/actions
- **Project Documentation**: [docs/README.md](README.md)

---

**Quick Help**: For issues, see [Troubleshooting](CI_CD_GUIDE.md#troubleshooting) in the full guide.
