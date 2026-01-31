# GitHub Actions Workflows for KatanaCombat

This directory contains GitHub Actions workflows for continuous integration and deployment of the KatanaCombat Unreal Engine 5.6 project.

## Workflows

### 🔧 `ue5-ci.yml` - Main CI/CD Pipeline

Comprehensive build, test, and analysis pipeline for the KatanaCombat project.

#### Triggers
- Push to `main` or `develop` branches
- Pull requests targeting `main` or `develop`
- Manual workflow dispatch

#### Pipeline Stages

1. **Repository Checkout**
   - Clones the repository with full history
   - Includes Git LFS support for large binary files

2. **Build Caching**
   - Caches Unreal Engine build artifacts to speed up subsequent builds
   - Caches: `Intermediate/`, `DerivedDataCache/`, `Saved/BuildGraph/`
   - Cache key based on UE version and build configuration files

3. **Unreal Engine Setup**
   - Detects or configures Unreal Engine 5.6 installation
   - Sets up environment variables for build tools
   - **Note**: Requires UE5.6 to be pre-installed on the runner

4. **Project File Generation**
   - Generates Visual Studio project files
   - Required for compilation

5. **Compilation**
   - Builds the project for `Win64` platform
   - Configuration: `Development Editor`
   - Uses Unreal Build Tool (`Build.bat`)
   - Generates detailed build logs

6. **Static Analysis (clang-tidy)**
   - Performs static code analysis on C++ source files
   - Checks for: readability, performance, bugs, modernization
   - Outputs warnings to logs for review
   - **Continues on error** - won't fail the build

7. **Editor Validation**
   - Runs headless editor validation (`ResavePackages`)
   - Verifies all assets can be loaded without errors
   - **Continues on error** - won't fail the build

8. **Automation Tests**
   - Executes Unreal Engine's Automation Test Framework
   - Runs all tests in the `KatanaCombat` test suite
   - Uses headless mode with null RHI for performance
   - **Fails the build if tests fail**

9. **Artifact Upload**
   - Build logs (retention: 14 days)
   - Static analysis results (retention: 14 days)
   - Test results (retention: 30 days)
   - Validation logs (retention: 14 days)
   - Compiled binaries - DLLs and PDBs (retention: 7 days)

10. **Job Summary**
    - Generates a markdown summary of the pipeline results
    - Visible in the GitHub Actions UI

## Requirements

### Self-Hosted Runner Setup

This workflow is designed for **self-hosted Windows runners** with Unreal Engine 5.6 pre-installed.

#### Runner Requirements:
- **OS**: Windows Server 2019/2022 or Windows 10/11
- **Unreal Engine**: Version 5.6 installed at `C:\Program Files\Epic Games\UE_5.6`
- **Visual Studio**: 2022 with C++ game development workload
- **Disk Space**: Minimum 100GB free for build artifacts and caching
- **RAM**: Minimum 16GB (32GB recommended)
- **CPU**: Multi-core processor (8+ cores recommended)

#### Installing Unreal Engine on the Runner:
1. Download UE5.6 from [unrealengine.com](https://www.unrealengine.com/download)
2. Install to default location: `C:\Program Files\Epic Games\UE_5.6`
3. Verify installation by running: `"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor.exe" -version`

#### Setting up a Self-Hosted Runner:
1. Go to your GitHub repository settings
2. Navigate to **Actions** → **Runners** → **New self-hosted runner**
3. Follow the installation instructions for Windows
4. Configure the runner as a service for automatic startup

### GitHub Hosted Runners (Alternative)

While this workflow is optimized for self-hosted runners, it can be adapted for GitHub-hosted runners by:
1. Adding a step to download and install UE5.6 (requires Epic Games launcher credentials)
2. Using secrets to authenticate with Epic Games
3. Note: GitHub-hosted runners have time and storage limitations that may not be suitable for UE5 builds

## Secrets and Configuration

### Required Secrets (for future enhancements)

While the current workflow doesn't require secrets, you may need to configure these for advanced features:

#### Epic Games Authentication (if using auto-install):
```
EPIC_GAMES_USERNAME - Epic Games account email
EPIC_GAMES_PASSWORD - Epic Games account password
```

#### Code Signing (for release builds):
```
CODE_SIGNING_CERTIFICATE - Base64 encoded certificate
CODE_SIGNING_PASSWORD - Certificate password
```

#### Deployment (for future use):
```
STEAM_USERNAME - Steam publishing credentials
STEAM_PASSWORD - Steam publishing credentials
STEAM_APP_ID - Steam application ID
```

### Adding Secrets:
1. Go to repository **Settings** → **Secrets and variables** → **Actions**
2. Click **New repository secret**
3. Add the secret name and value
4. Access in workflow using: `${{ secrets.SECRET_NAME }}`

## Usage

### Automatic Triggers

The workflow runs automatically on:
- Any push to `main` or `develop` branches
- Any pull request targeting `main` or `develop`

### Manual Trigger

To run the workflow manually:
1. Go to **Actions** tab in your repository
2. Select **UE5 CI - Build, Test, and Analyze** workflow
3. Click **Run workflow**
4. Select the branch and click **Run workflow**

## Customization

### Modifying Build Configuration

To change the build configuration, update the `env` section in `ue5-ci.yml`:

```yaml
env:
  UE_VERSION: '5.6'           # Unreal Engine version
  PROJECT_NAME: 'KatanaCombat' # Your project name
  PLATFORM: 'Win64'            # Target platform
  CONFIGURATION: 'Development Editor' # Build configuration
```

### Adjusting Test Filters

To run specific test categories, modify the test command in Step 8:

```yaml
# Run all tests
-ExecCmds=Automation RunTests KatanaCombat

# Run only specific tests
-ExecCmds=Automation RunTests KatanaCombat.CombatComponent

# Run multiple test groups
-ExecCmds=Automation RunTests KatanaCombat.CombatComponent+KatanaCombat.TargetingComponent
```

### Adjusting Artifact Retention

Modify retention periods in upload steps:

```yaml
retention-days: 14  # Change to desired number of days
```

### Enabling/Disabling Pipeline Stages

To skip a stage, add `if: false` to the step:

```yaml
- name: Static Analysis - clang-tidy
  if: false  # This will skip the step
  continue-on-error: true
  ...
```

## Troubleshooting

### Build Fails: "Unreal Engine not found"

**Solution**: Ensure UE5.6 is installed at the expected path on the runner.

```powershell
# Verify installation
Test-Path "C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor.exe"
```

### Build Fails: "Project files not found"

**Solution**: The project file generation may have failed. Check the "Generate Visual Studio Project Files" step logs.

### Tests Fail: "Test timeout"

**Solution**: Increase the timeout for long-running tests or run tests in smaller batches.

### Cache Not Working

**Solution**: Verify cache paths exist and the cache key is unique:
- Check that `Intermediate/` and `DerivedDataCache/` directories are created during build
- Ensure `.gitignore` doesn't prevent cache restoration

### Static Analysis Warnings

**Solution**: clang-tidy warnings don't fail the build by default. To enforce:
```yaml
continue-on-error: false  # Change to fail on warnings
```

## Performance Optimization

### Build Times

Typical build times (on self-hosted runner with 16-core CPU, 32GB RAM):
- **First build**: 15-30 minutes (no cache)
- **Incremental builds**: 5-10 minutes (with cache)
- **Tests**: 3-5 minutes
- **Total pipeline**: 20-40 minutes

### Caching Best Practices

1. **Maximize cache hits**: Keep build configuration files stable
2. **Parallel builds**: Enable multi-core compilation in UE settings
3. **Distributed builds**: Consider Incredibuild or FASTBuild for large teams

### Reducing Artifact Sizes

1. **Exclude debug symbols**: Remove PDBs if not needed
2. **Compress logs**: Add a compression step before upload
3. **Shorter retention**: Reduce retention days for large artifacts

## Future Enhancements

### Planned Features
- [ ] Multi-platform builds (Linux, macOS)
- [ ] Automated packaging and deployment
- [ ] Performance profiling integration
- [ ] Code coverage reporting
- [ ] Integration with static analysis tools (SonarQube, Coverity)
- [ ] Slack/Discord notifications
- [ ] Automated changelog generation
- [ ] Steam/Epic Games Store deployment

### Optional Integrations

**Code Coverage**:
```yaml
- name: Generate Code Coverage
  run: |
    # Run tests with coverage
    # Upload to Codecov or Coveralls
```

**Performance Benchmarking**:
```yaml
- name: Run Performance Tests
  run: |
    # Execute performance benchmarks
    # Compare against baseline
```

## Support

For issues or questions:
1. Check the [Troubleshooting](#troubleshooting) section
2. Review workflow run logs in the Actions tab
3. Open an issue in the repository

## References

- [Unreal Engine Documentation](https://docs.unrealengine.com/)
- [GitHub Actions Documentation](https://docs.github.com/en/actions)
- [UnrealBuildTool Reference](https://docs.unrealengine.com/en-US/ProductionPipelines/BuildTools/UnrealBuildTool/)
- [Automation System Overview](https://docs.unrealengine.com/en-US/TestingAndOptimization/Automation/)
