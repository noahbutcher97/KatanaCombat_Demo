# Claude Code Scripts Directory

## Claude Code Infrastructure Overview

The `.claude` directory contains two complementary systems:

1. **Context Mode System** - Auto-switches documentation focus based on file type
2. **Agent Coordination System** - Routes complex tasks to specialist subagents

These systems work together to provide intelligent session management:
- **Context modes** determine WHAT documentation/principles to prioritize
- **Agent routing** determines WHO should handle complex tasks

---

## Production Scripts (Active)

### Mode Detection System
- **`intelligent-mode-detector.ps1`** ✅ **PRIMARY** - Unified 5-factor ML detection
  - File patterns (35%), Conversation (25%), Learning (20%), History (15%), Time (5%)
  - Loads config from `.context-config.json`
  - Used by `auto-context.ps1` hook

- **`detect-mode.ps1`** ✅ **FALLBACK** - Basic file pattern detection
  - Simple file extension + path matching
  - Used as final fallback if intelligent detector missing

- **`holistic-mode-detector.ps1`** ⚠️ **DEPRECATED/FALLBACK** - Legacy 3-factor system
  - File (50%), Conversation (35%), Learning (15%)
  - Kept as intermediate fallback
  - **Recommendation**: Can be removed after intelligent detector proven stable

- **`confidence-calculator.ps1`** ⚠️ **INTERNAL ONLY** - 4-factor standalone calculator
  - Created during development of intelligent detector
  - **NOT USED** by any hooks or commands
  - **Recommendation**: Keep as reference or remove

### ML Learning System
- **`learning-tracker.ps1`** ✅ **ACTIVE** - Bayesian inference + temporal decay
  - Actions: record, query, status, feedback, correlate, reset
  - Data: `.context-learning.json`

- **`context-tracker.ps1`** ✅ **ACTIVE** - Switch history and analytics
  - Tracks mode switches, accuracy, usage patterns
  - Data: `.context-history.json`

### Conversation Analysis
- **`conversation-analyzer.ps1`** ✅ **ACTIVE** - Topic extraction and complexity analysis
  - Keyword matching across 6 categories
  - Task complexity heuristics
  - Called by intelligent-mode-detector

### Display & UI
- **`display-confidence-breakdown.ps1`** ✅ **ACTIVE** - Rich visual feedback
  - Full mode: Comprehensive confidence report with bars
  - Compact mode: One-line summary
  - Can be called manually or integrated into hooks

### Agent Coordination System
- **`agent-coordinator.ps1`** ✅ **ACTIVE** - Pipeline orchestration and agent management
  - 6 specialist agents: router, ue-code-generator, design-compliance-auditor, code-auditor, pipeline-feature, pipeline-bugfix
  - 4 predefined pipelines: feature, bugfix, validation, refactoring
  - Agent chain validation and pipeline info display
  - Called by `/agent` command

### Integration
- **`install-git-hooks.ps1`** ✅ **ACTIVE** - Automated git hook installation
  - Called by `/hooks install` command

---

## Test Scripts (Development/QA)

Located in main directory (should be moved to `tests/` subdirectory):

### Integration Tests
- `test-integration.ps1` - End-to-end intelligent detector tests
- `test-auto-context.ps1` - Auto-context hook testing
- `test-auto-context-ml.ps1` - ML integration tests

### Component Tests
- `test-bayesian.ps1` - Bayesian inference validation
- `test-correlations.ps1` - Topic correlation testing
- `test-correlation-boost.ps1` - Correlation boost validation
- `test-latency.ps1` - Performance benchmarks
- `test-bayesian-latency.ps1` - Bayesian performance

### Feature Tests
- `test-count-issue.ps1` - Counter validation
- `test-feedback-simple.ps1` - Feedback system
- `test-gradient-descent.ps1` - Weight optimization
- `test-robustness.ps1` - Edge case handling

### Real-World Tests
- `test-real-world-usage.ps1` - Practical workflow simulation
- `test-e2e-real-world.ps1` - End-to-end real scenarios
- `test-full-integration.ps1` - Complete system integration

### Debug Scripts
- `test-accuracy-debug.ps1`
- `test-correlate-debug.ps1`
- `test-debug-deserialization.ps1`
- `test-reset-debug.ps1`
- `test-e2e-debug.ps1`
- `test-single-scenario.ps1`
- `test-weight-adapt.ps1`

**Total**: 21 test scripts

---

## Architecture Flow

```
User opens file
    ↓
afterFileOpen hook
    ↓
auto-context.ps1
    ↓
intelligent-mode-detector.ps1 (PRIMARY)
    ├─→ detect-mode.ps1 (file patterns)
    ├─→ conversation-analyzer.ps1 (topics)
    ├─→ learning-tracker.ps1 (ML patterns)
    └─→ .context-learning.json (historical data)
    ↓
Confidence score + suggested mode
    ↓
auto-context.ps1 applies thresholds
    ↓
Auto-switch (if enabled + confidence ≥50%)
    ├─→ context-tracker.ps1 (record switch)
    └─→ learning-tracker.ps1 (record feedback)
```

### Fallback Chain (Graceful Degradation)
```
intelligent-mode-detector.ps1
    ↓ (if missing)
holistic-mode-detector.ps1
    ↓ (if missing)
detect-mode.ps1
    ↓ (if missing)
Exit silently (no detection)
```

---

## Configuration Files

- **`.context-config.json`** - Thresholds, weights, presets
- **`.context-learning.json`** - ML learning database
- **`.context-history.json`** - Switch history and analytics

---

## Recommendations

1. **Move test scripts**: Create `tests/` subdirectory
2. **Remove or deprecate**: `holistic-mode-detector.ps1` after validation period
3. **Remove or archive**: `confidence-calculator.ps1` (unused)
4. **Consider integrating**: `display-confidence-breakdown.ps1` into `/mode status` or `/mode suggest`

---

**Last Updated**: 2025-11-19
**System Version**: v3.0 (Intelligent Mode Detection)
