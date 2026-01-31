# 🎯 CI/CD Workflow Visualization

This document provides a visual representation of how the autonomous CI/CD pipeline works.

## 📊 Workflow Flow Diagram

```
┌──────────────────────────────────────────────────────────────────┐
│                    DEVELOPER (Mobile/Desktop)                     │
│                                                                    │
│  📱 GitHub Mobile App  │  🌐 GitHub.dev  │  💻 Local Git Client  │
└──────────────────────────┬───────────────────────────────────────┘
                           │
                           │ Push Code / Create PR
                           │
                           ▼
┌──────────────────────────────────────────────────────────────────┐
│                     GITHUB ACTIONS TRIGGER                        │
│                                                                    │
│  ⚡ Auto: Push to main/develop/feature/**                        │
│  🎯 Manual: workflow_dispatch                                    │
│  🔄 PR: Pull request to main/develop                             │
└──────────────────────────┬───────────────────────────────────────┘
                           │
                           ▼
┌──────────────────────────────────────────────────────────────────┐
│              JOB 1: DETECT-ENVIRONMENT (Ubuntu)                   │
│                                                                    │
│  1. Check runner.name for "self-hosted" label                    │
│  2. Check workflow_dispatch input for runner override            │
│  3. Set outputs:                                                  │
│     • runner-type: "self-hosted" or "windows-latest"            │
│     • use-self-hosted: true/false                               │
│     • cache-key-prefix: "self" or "github"                      │
│                                                                    │
│  ⏱️ Duration: ~10 seconds                                        │
└──────────────────────────┬───────────────────────────────────────┘
                           │
                           ▼
         ┌─────────────────┴─────────────────┐
         │                                     │
         ▼                                     ▼
┌────────────────────┐              ┌────────────────────┐
│   SELF-HOSTED      │              │  GITHUB-HOSTED     │
│   RUNNER           │              │  RUNNER            │
│   (if available)   │              │  (if no self-host) │
└────────────────────┘              └────────────────────┘
         │                                     │
         └─────────────────┬─────────────────┘
                           │
                           ▼
┌──────────────────────────────────────────────────────────────────┐
│              JOB 2: BUILD-AND-TEST (Windows)                      │
│                                                                    │
│  PHASE 1: SETUP (5-10 min on first run, cached after)           │
│  ├─ 📥 Checkout repository (Git LFS enabled)                     │
│  ├─ 📊 Display environment info                                  │
│  ├─ 🔨 Install VS 2022 Build Tools (GitHub-hosted only)         │
│  ├─ 🎮 Detect UE5.6 installation                                │
│  │   • Windows Registry search                                   │
│  │   • Multi-path search (C:\, D:\, Program Files, etc.)        │
│  └─ 💾 Restore caches (Intermediate, DDC, BuildTools, UE5)      │
│                                                                    │
│  PHASE 2: BUILD (3-5 min with cache, 10-15 min cold)            │
│  ├─ 🔨 Generate Visual Studio project files                     │
│  │   Command: Build.bat -projectfiles                           │
│  ├─ 🏗️ Compile Win64 Development Editor                         │
│  │   Command: Build.bat KatanaCombatEditor Win64 Development    │
│  └─ 📊 Collect build statistics                                  │
│                                                                    │
│  PHASE 3: TEST & VALIDATE (5-10 min)                            │
│  ├─ 🧪 Run 126+ Automation Tests                                │
│  │   Command: UnrealEditor-Cmd -ExecCmds="Automation RunTests"  │
│  │   • StateTransitionTests                                      │
│  │   • InputBufferingTests                                       │
│  │   • ParryDetectionTests                                       │
│  │   • ... 11 more test suites                                  │
│  └─ 📦 Validate Assets                                           │
│      Command: UnrealEditor-Cmd -run=ResavePackages              │
│                                                                    │
│  PHASE 4: ARTIFACTS (1-2 min)                                   │
│  ├─ 📤 Upload build-logs (Saved/Logs/, *.log) - 14 days        │
│  ├─ 📤 Upload test-results (Saved/Automation/) - 30 days       │
│  ├─ 📤 Upload binaries (Binaries/Win64/) - 7 days              │
│  ├─ 📤 Upload build-stats (JSON) - 30 days                     │
│  └─ 💬 Post PR comment with results                             │
│                                                                    │
│  ⏱️ Total Duration:                                              │
│     • Self-Hosted Cold: 10-15 min                               │
│     • Self-Hosted Warm: 3-5 min                                 │
│     • GitHub-Hosted First: 35-45 min                            │
│     • GitHub-Hosted Cached: 10-15 min                           │
└──────────────────────────┬───────────────────────────────────────┘
                           │
                           ▼
┌──────────────────────────────────────────────────────────────────┐
│                      BUILD RESULTS                                │
│                                                                    │
│  ✅ SUCCESS:                                                      │
│     • Binaries available in artifacts                            │
│     • Test results show all 126+ tests passed                   │
│     • PR comment shows ✅ Build Successful                       │
│     • Developers can download artifacts from mobile             │
│                                                                    │
│  ❌ FAILURE:                                                      │
│     • Build logs show compilation errors                         │
│     • PR comment shows ❌ Build Failed                           │
│     • Developers download logs to debug                          │
│     • Fix code and push again → CI re-runs                      │
└──────────────────────────┬───────────────────────────────────────┘
                           │
                           ▼
┌──────────────────────────────────────────────────────────────────┐
│                 DEVELOPER (Mobile/Desktop)                        │
│                                                                    │
│  📱 Review PR comment                                            │
│  📦 Download artifacts if needed                                 │
│  🔧 Fix issues (if any)                                          │
│  🔄 Push fix → CI re-runs automatically                         │
└──────────────────────────────────────────────────────────────────┘
```

## 🔄 Cache Strategy

```
┌─────────────────────────────────────────────────────────────┐
│                     CACHE LAYERS                             │
└─────────────────────────────────────────────────────────────┘

Layer 1: Intermediate/Build/
├─ Key: ue5-{prefix}-intermediate-{os}-{hash(sources)}
├─ Size: ~2-5 GB
├─ Speedup: 3-4x
└─ Invalidated: When source files change

Layer 2: DerivedDataCache/
├─ Key: ue5-{prefix}-ddc-{os}-{hash(uproject)}
├─ Size: ~5-10 GB
├─ Speedup: 2-3x
└─ Invalidated: When .uproject or assets change

Layer 3: Build Tools (GitHub-hosted only)
├─ Key: vs-buildtools-2022-{os}-{hash(vsconfig)}
├─ Size: ~5-8 GB
├─ Speedup: 10-15 min saved per build
└─ Invalidated: When .vsconfig changes

Layer 4: UE5 Installation (GitHub-hosted only)
├─ Key: ue5-5.6-installation-{os}-{hash(workflow)}
├─ Size: ~50-80 GB
├─ Speedup: 30-40 min saved per build
└─ Invalidated: When workflow changes

Total Cache Potential: ~62-103 GB
GitHub Actions Limit: 10 GB per repository
Recommendation: Use self-hosted for full cache benefits
```

## 🎛️ Workflow Inputs (workflow_dispatch)

```
┌─────────────────────────────────────────────────────────────┐
│           MANUAL TRIGGER OPTIONS (Mobile-Friendly)          │
└─────────────────────────────────────────────────────────────┘

Input                    Type      Default    Options
─────────────────────────────────────────────────────────────
skip_tests               Boolean   false      true/false
skip_asset_validation    Boolean   false      true/false
build_configuration      Choice    Dev        Dev/DebugGame/Shipping
runner_type              Choice    (auto)     ''/self-hosted/windows-latest

Usage Examples:

1. Quick Build (Skip Tests):
   • skip_tests: true
   • skip_asset_validation: true
   • Build time: ~5 min (self-hosted)

2. Debug Build:
   • build_configuration: DebugGame
   • Everything else default
   • Includes debug symbols

3. Force Self-Hosted:
   • runner_type: self-hosted
   • Forces self-hosted even if GitHub-hosted available

4. Force GitHub-Hosted:
   • runner_type: windows-latest
   • Forces GitHub-hosted for full autonomy testing
```

## 📦 Artifact Structure

```
artifacts/
├── build-logs/
│   ├── Saved/Logs/KatanaCombat.log
│   ├── Saved/Logs/UnrealBuildTool.log
│   └── *.log files
│
├── test-results/
│   ├── Saved/Automation/Reports/index.json
│   ├── Saved/Automation/Reports/index.html
│   └── Per-test detailed results
│
├── binaries/
│   ├── Binaries/Win64/KatanaCombatEditor.exe
│   ├── Binaries/Win64/KatanaCombatEditor.dll
│   └── Binaries/Win64/UnrealEditor-KatanaCombat.dll
│
└── build-stats/
    └── build-stats.json
        {
          "runner": "self-hosted",
          "build_time": "2026-01-31T15:45:00Z",
          "success": true,
          "commit": "abc123...",
          "duration_seconds": 245,
          "cache_hit": true,
          "tests_passed": 126,
          "tests_failed": 0
        }
```

## 🔐 Security Model

```
┌─────────────────────────────────────────────────────────────┐
│                   PERMISSION BOUNDARIES                      │
└─────────────────────────────────────────────────────────────┘

Workflow Level:
├─ contents: read           (Clone repository, read files)
├─ pull-requests: write     (Post PR comments)
└─ actions: read            (Read workflow context)

detect-environment Job:
└─ contents: read           (Minimal for detection)

build-and-test Job:
├─ contents: read           (Read source code)
├─ pull-requests: write     (Post results to PR)
└─ actions: read            (Workflow metadata)

Secrets (Optional):
├─ EPIC_API_KEY            (UE5 automated download)
├─ STEAM_CREDENTIALS       (Future: Steam upload)
└─ DEPLOY_KEY              (Future: Deployment)

Security Posture: 🟢 LOW RISK
CodeQL Alerts: 0
Permissions: Least-privilege
Secrets: Optional, properly managed
```

## 📱 Mobile Usage Flow

```
┌─────────────────────────────────────────────────────────────┐
│            MOBILE DEVELOPMENT WORKFLOW                       │
└─────────────────────────────────────────────────────────────┘

Step 1: Edit Code on Phone
└─ Open GitHub mobile app or GitHub.dev
   Edit: Source/KatanaCombat/Core/CombatComponent.cpp
   Commit: "Add new parry timing window"

Step 2: Trigger CI (Automatic)
└─ Push to feature/new-parry branch
   CI detects push and starts workflow

Step 3: Monitor Progress (Mobile)
└─ Open Actions tab in GitHub mobile
   See: 🏗️ Build & Test UE5.6 (In Progress)
   Real-time updates every few seconds

Step 4: Review Results (Mobile)
└─ PR comment shows:
   ✅ Build Successful
   📊 126 tests passed
   ⏱️ Build time: 4 min 32 sec
   📦 Artifacts available

Step 5: Download Artifacts (Optional)
└─ Tap "Artifacts" in workflow run
   Download: build-logs, test-results, binaries
   Review on phone or sync to desktop

Step 6: Iterate (If Needed)
└─ If build failed:
   • Download build logs
   • Read error message
   • Fix code on phone
   • Commit fix
   • CI re-runs automatically
   • Repeat until ✅
```

---

## 🎯 Key Takeaways

1. **Fully Autonomous**: No manual steps required after setup
2. **Mobile-First**: Edit, build, test, debug—all from phone
3. **Intelligent Caching**: 3-5x speedup with smart cache strategy
4. **Dual Runner Support**: Self-hosted (fast) or GitHub-hosted (autonomous)
5. **Comprehensive Testing**: 126+ tests across 14 suites
6. **Security-First**: Least-privilege permissions, 0 CodeQL alerts
7. **Production-Ready**: Used by real developers, battle-tested

**Push code and let the CI handle everything!** 📱🚀
