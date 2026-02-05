# KatanaCombat Gap Issues - Creation Guide

This directory provides a **structured documentation system** for converting Gap Tracker entries into GitHub issues.

## Overview

The Gap Tracker (`docs/plans/gap-tracker.md`) contains **149 gaps** across **22 categories** from the comprehensive Paired Animation System audit. This system helps convert them into trackable GitHub issues.

## What's Included

### 1. Workflow Documentation
**Location**: `..gap-workflow/WORKFLOW.md`

Comprehensive guide covering:
- Phased approach to issue creation (P0 → P1 → P2 → P3)
- Manual vs. CLI creation workflows  
- Label taxonomy and naming conventions
- GitHub Projects integration
- Maintenance and best practices

### 2. Pre-Written Issue Templates
**Location**: `../gap-issues/`

Directory structure with priority-organized issue files:
```
gap-issues/
├── critical/          # P0 gaps (immediate crash risks)
├── high-priority/     # P1 gaps (core functionality)
├── medium-priority/   # P2 gaps (quality improvements)
├── low-priority/      # P3 gaps (polish/enhancements)
└── completed/         # Reference for Done/Deferred gaps
```

### 3. Example Issues (Ready to Use)

Three fully-detailed example issues demonstrating KatanaCombat-specific context:

**GAP-22.1** (Critical): `gap-issues/critical/gap-22-1.md`
- GetWorld() null crash in hold duration queries
- Impact on hold attack system and debug visualization
- Three solution options with recommendations
- Test strategy and acceptance criteria

**GAP-22.10** (High): `gap-issues/critical/gap-22-10.md`
- CustomTimeDilation division-by-zero bug
- Impact on hitstop, slow-motion, and animation blending
- Minimum time dilation constant approach
- Physics integration safety considerations

**GAP-1.1** (High): `gap-issues/high-priority/gap-1-1.md`
- AI Attack Token System design specification
- Ghost of Tsushima-inspired coordination mechanics
- 4-phase implementation plan with timelines
- Integration with Behavior Trees and paired animations

## Quick Start

### Option 1: Manual Creation (Recommended)

1. **Read the workflow**: `../gap-workflow/WORKFLOW.md`
2. **Choose a gap** from `docs/plans/gap-tracker.md`
3. **Copy example template** from `gap-issues/` directory
4. **Customize** with gap-specific details
5. **Create issue** via GitHub web UI or CLI:
   ```bash
   gh issue create -F .github/gap-issues/critical/gap-22-1.md
   ```

### Option 2: Batch Creation via CLI

Create multiple issues from pre-written files:

```bash
# Critical issues (P0)
for file in .github/gap-issues/critical/*.md; do
  gh issue create -F "$file"
  sleep 2
done

# High priority issues (P1)
for file in .github/gap-issues/high-priority/*.md; do
  gh issue create -F "$file"
  sleep 2
done
```

### Option 3: Direct CLI Creation

Create individual issues inline:

```bash
gh issue create \
  --title "[GAP-22.1] GetWorld() null crash in GetHoldDuration" \
  --body-file .github/gap-issues/critical/gap-22-1.md \
  --label "gap,priority: p0,type: bug,area: implementation,system: paired-animation"
```

## Issue Template Structure

Each issue file includes:

1. **YAML Frontmatter** - Title and label suggestions
2. **Gap Overview** - Brief description with impact summary
3. **Classification** - Category, priority, status
4. **Combat System Context** - KatanaCombat-specific impact analysis
5. **Technical Details** - Code locations, problem explanation, crash points
6. **Proposed Solution** - Multiple options with recommendations
7. **Implementation Checklist** - Step-by-step tasks
8. **Testing Strategy** - Unit test examples
9. **Acceptance Criteria** - Clear completion criteria
10. **Documentation Links** - References to relevant docs

## Label System

All issues use consistent labeling:

**Core**: `gap`, `system: paired-animation`  
**Priority**: `priority: p0/p1/p2/p3`  
**Status**: `status: pending/partial/done/deferred`  
**Area**: `area: ai/input/animation/audio/ui/etc`  
**Type**: `type: bug/feature/polish/edge-case`  
**Source**: `source: audit/testing/analysis`

See `../gap-workflow/WORKFLOW.md` for complete label taxonomy.

## Phased Approach

**Don't create all 149 issues at once!** Use a phased approach:

### Phase 1: Critical Path (5-10 issues)
- GAP-22.1, 22.10, 22.3 (crash prevention)
- GAP-13.1, 13.2, 13.5 (null safety)
- GAP-1.2 (interrupt mechanic)

### Phase 2: Feature Completion (15 issues)
- Remaining P1 gaps
- Core paired animation functionality

### Phase 3: Quality & Polish (40 issues)
- P2 gaps (audio, camera, VFX)
- Component caching and optimization

### Phase 4: Enhancement (24 issues)
- P3 gaps (UI/HUD, music, ragdoll)
- Nice-to-have features

## Creating Your Own Issue Files

To add more pre-written issues:

1. **Copy an example** from appropriate priority directory
2. **Extract gap details** from `docs/plans/gap-tracker.md`
3. **Add KatanaCombat context**:
   - Which combat components are affected?
   - How does it impact player experience?
   - What systems depend on this?
4. **Research solution**: Review codebase and documentation
5. **Write clear acceptance criteria**
6. **Link related gaps** and documentation

**Naming Convention**: `gap-X-Y.md` (e.g., `gap-1-1.md` for GAP-1.1)

## GitHub Projects Integration

After creating issues, set up tracking:

1. **Create Project**: "Paired Animation System - Gap Closure"
2. **Add Views**:
   - By Priority (Board: P0, P1, P2, P3)
   - By Area (Table grouped by area label)
   - By Status (Kanban: Pending → In Progress → Done)
   - Timeline (Roadmap for phased delivery)
3. **Link issues** to project automatically
4. **Add custom fields**: Effort estimate, target milestone, blockers

## Maintenance Workflow

**After Creating Issues**:
1. Update gap tracker with issue numbers: `| 22.1 | Description | P0 | Pending → #123 |`
2. Link related issues in issue descriptions
3. Assign to appropriate milestone (Phase 5d, Phase 6, etc.)
4. Tag team members for assignment

**Weekly**:
- Triage new gaps
- Update status labels
- Close completed issues
- Sync with gap tracker

**Monthly**:
- Review P0/P1 assignments
- Unblock dependencies
- Update project timeline

## Best Practices

✅ **DO**:
- Create issues in priority order
- Include combat system context
- Link related gaps and docs
- Update gap tracker with issue numbers
- Close issues when gaps marked "Done"

❌ **DON'T**:
- Create all 149 issues at once
- Use generic descriptions
- Forget bidirectional linking
- Create duplicates
- Ignore the phased approach

## Example Workflow

```bash
# 1. Review workflow
cat .github/gap-workflow/WORKFLOW.md

# 2. Check example issues
ls .github/gap-issues/critical/

# 3. Create first batch (critical)
gh issue create -F .github/gap-issues/critical/gap-22-1.md
gh issue create -F .github/gap-issues/critical/gap-22-10.md

# 4. Update gap tracker with issue numbers
# (manual edit of docs/plans/gap-tracker.md)

# 5. Set up GitHub Project
# (use web UI)

# 6. Begin implementation!
```

## Support Resources

- **Workflow Guide**: `.github/gap-workflow/WORKFLOW.md`
- **Gap Tracker**: `docs/plans/gap-tracker.md`
- **Audit Findings**: `docs/audits/AUDIT_SYNTHESIS_2026-02-03.md`
- **Architecture Docs**: `docs/architecture/ARCHITECTURE.md`
- **Paired Animation Spec**: `docs/specs/PAIRED_ANIMATION_SPEC.md`
