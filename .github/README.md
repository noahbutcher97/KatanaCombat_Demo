# GitHub Actions CI/CD Infrastructure

This directory contains the GitHub Actions workflow configurations for the KatanaCombat project.

## Workflows

### `ue5-ci.yml` - Main CI/CD Pipeline

**Purpose**: Automated building, testing, and validation of the KatanaCombat UE5 project.

**Features**:
- ✅ Dual runner support (self-hosted + GitHub-hosted)
- ✅ Automatic UE5 detection and installation
- ✅ Intelligent caching for faster builds
- ✅ Automation test execution
- ✅ Asset validation
- ✅ Artifact management with retention policies

**Triggers**:
- Push to `main` or `develop` branches
- Pull requests to `main` or `develop` branches
- Manual workflow dispatch with runner type selection

**Documentation**: See [docs/CI_CD_GUIDE.md](../docs/CI_CD_GUIDE.md) for detailed information.

## Runner Types

### Self-Hosted Runners

**Labels**: `[self-hosted, Windows, ue5]`

**Setup**: See [docs/SETUP_GUIDE.md](../docs/SETUP_GUIDE.md)

**Best For**:
- Production builds
- When UE5 is pre-installed
- Faster build times
- Local infrastructure

### GitHub-Hosted Runners

**Runner**: `windows-latest`

**Best For**:
- Testing and validation
- When self-hosted unavailable
- Open-source contributions
- Zero infrastructure maintenance

## Configuration

### Environment Variables

Set in workflow file:
- `PROJECT_NAME`: KatanaCombat
- `UE_VERSION`: 5.6
- `BUILD_CONFIGURATION`: Development Editor

### Secrets (Optional)

Configure in repository settings:
- `EPIC_API_KEY`: For automated UE5 installation
- `STEAM_USERNAME`: For Steam deployment
- `STEAM_PASSWORD`: For Steam deployment
- `DEPLOY_TOKEN`: For custom deployment

## Artifacts

| Artifact | Retention | Contents |
|----------|-----------|----------|
| build-logs-* | 14 days | Build and compiler logs |
| test-results-* | 30 days | Automation test reports |
| binaries-* | 7 days | Compiled executables |
| build-stats-* | 30 days | Build performance metrics |
| build-report | 90 days | Summary report |

## Quick Start

### Running a Build

1. Go to **Actions** tab
2. Select **UE5 CI/CD Pipeline**
3. Click **Run workflow**
4. Choose runner type:
   - `auto`: Try self-hosted first, fallback to GitHub-hosted
   - `self-hosted`: Only self-hosted
   - `github-hosted`: Only GitHub-hosted
5. Click **Run workflow**

### Viewing Results

1. Click on the workflow run
2. View job logs
3. Download artifacts from the **Artifacts** section

## Maintenance

- Workflow updates: Edit `.github/workflows/ue5-ci.yml`
- Runner updates: See [SETUP_GUIDE.md](../docs/SETUP_GUIDE.md#updating-the-runner)
- Cache management: Automatic, with 7-day retention

## Support

- **Documentation**: [docs/CI_CD_GUIDE.md](../docs/CI_CD_GUIDE.md)
- **Setup Guide**: [docs/SETUP_GUIDE.md](../docs/SETUP_GUIDE.md)
- **Issues**: [github.com/noahbutcher97/KatanaCombat_Demo/issues](https://github.com/noahbutcher97/KatanaCombat_Demo/issues)

---

**Version**: 1.0.0  
**Last Updated**: 2026-01-31
