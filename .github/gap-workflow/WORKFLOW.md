# KatanaCombat Gap Tracker → GitHub Issues Workflow

This document provides a structured workflow for converting Gap Tracker entries into GitHub issues for better visibility and tracking.

## Philosophy

Rather than bulk-creating all 149 gaps at once, this workflow emphasizes **thoughtful, phased issue creation** that aligns with the project's development priorities and the Paired Animation System roadmap.

## Directory Structure

```
.github/
├── gap-issues/                    # Individual issue definitions
│   ├── critical/                  # P0 gaps (5 issues)
│   ├── high-priority/             # P1 gaps (15 issues)  
│   ├── medium-priority/           # P2 gaps (40 issues)
│   ├── low-priority/              # P3 gaps (24 issues)
│   └── completed/                 # Done/Deferred for reference
├── ISSUE_TEMPLATE/
│   └── gap-issue-template.md     # Base template
└── gap-workflow/
    └── WORKFLOW.md               # This file
```

## Phased Approach

### Phase 1: Critical Path (P0 + High-Impact P1)
Create issues for gaps that block core functionality or risk system stability.

**Target Gaps:**
- 22.1: GetWorld() null crash
- 22.10: CustomTimeDilation division-by-zero
- 22.3: const_cast undefined behavior
- 1.2: Interrupt finisher mechanic (partial)
- 13.1, 13.2, 13.5: Null references and crash prevention

**Process:**
1. Review each gap in `docs/plans/gap-tracker.md`
2. Copy template from `.github/gap-issues/critical/`
3. Fill in KatanaCombat-specific context
4. Create issue manually or via `gh issue create -F <file>`
5. Link issue number back to gap tracker

### Phase 2: Feature Completion (Remaining P1)
Issues needed to complete the Paired Animation System.

**Target Gaps:**
- 7.2: Pre-sync invulnerability
- 9.1-9.2: State recovery and cleanup
- 18.4-18.9: Partner validity and warp tracking

### Phase 3: Quality & Polish (P2)
Quality improvements and feature enhancements.

**Target Gaps:**
- Audio wiring (4.1, 4.2)
- Camera improvements (14.2)
- VFX wiring (15.3)
- Component caching (22.11)

### Phase 4: Enhancement (P3)
Nice-to-have features and polish.

**Target Gaps:**
- UI/HUD integration (5.x)
- Music ducking (4.3)
- Ragdoll settling (14.1)

## Manual Issue Creation Workflow

### Option A: Using GitHub Web UI

1. Navigate to repository Issues tab
2. Click "New Issue"
3. Choose "Gap Issue" template (if configured)
4. Copy content from prepared markdown file in `gap-issues/`
5. Apply labels manually:
   - `gap`
   - `system: paired-animation`
   - `priority: p0/p1/p2/p3`
   - `status: pending/partial/done/deferred`
   - Area-specific label (e.g., `area: ai`, `area: animation`)
6. Create issue
7. Note issue number in gap tracker

### Option B: Using GitHub CLI

```bash
# Navigate to repository root
cd /path/to/KatanaCombat_Demo

# Create from file
gh issue create -F .github/gap-issues/critical/gap-22-1.md

# Or inline
gh issue create \
  --title "[GAP-22.1] GetWorld() null crash in GetHoldDuration" \
  --body "$(cat .github/gap-issues/critical/gap-22-1.md)" \
  --label "gap,priority: p0,area: implementation,system: paired-animation"
```

### Option C: Batch Creation Script

For users comfortable with shell scripting, a simple loop:

```bash
for file in .github/gap-issues/critical/*.md; do
  echo "Creating issue from: $file"
  gh issue create -F "$file"
  sleep 2  # Rate limiting courtesy
done
```

## Label Taxonomy

### Core Labels (Always Applied)
- `gap` - Identifies issue as from gap tracker
- `system: paired-animation` - System scope

### Priority Labels (Mutually Exclusive)
- `priority: p0` - Critical (crash/data loss)
- `priority: p1` - High (core functionality)
- `priority: p2` - Medium (quality improvement)
- `priority: p3` - Low (polish/enhancement)

### Status Labels (Current State)
- `status: pending` - Not yet started
- `status: partial` - Partially complete
- `status: done` - Complete (should close issue)
- `status: deferred` - Postponed to Phase 6+

### Area Labels (Component/Domain)
- `area: ai` - AI and enemy coordination
- `area: input` - Player input handling
- `area: animation` - Animation system
- `area: audio` - Sound and music
- `area: ui` - User interface
- `area: environment` - World interaction
- `area: state-machine` - State transitions
- `area: performance` - Optimization
- `area: cleanup` - Recovery and cleanup
- `area: vfx` - Visual effects
- `area: implementation` - Code quality

### Type Labels (Nature of Work)
- `type: bug` - Bug fix or crash prevention
- `type: polish` - Quality improvement
- `type: edge-case` - Edge case handling
- `type: feature` - New functionality

### Source Labels (Origin)
- `source: audit` - From comprehensive audit
- `source: testing` - From test sessions
- `source: analysis` - From code analysis

## Issue Naming Convention

Format: `[GAP-X.Y] Brief Description`

Examples:
- `[GAP-1.1] No Attack Token System`
- `[GAP-22.1] GetWorld() null crash in GetHoldDuration`
- `[GAP-3.5] Incomplete animation interrupt handling`

## Linking Issues to Gap Tracker

After creating an issue, update the gap tracker:

```markdown
| 22.1 | GetWorld() null crash | P0 | Pending → #123 |
```

This creates bidirectional linking:
- Issue references gap tracker in body
- Gap tracker references issue number

## GitHub Projects Integration

Consider creating a GitHub Project to organize issues:

**Project Name:** Paired Animation System - Gap Closure

**Views:**
1. **By Priority** - Board view with columns P0, P1, P2, P3
2. **By Area** - Table view grouped by area label
3. **By Status** - Kanban board (Pending → In Progress → Review → Done)
4. **Timeline** - Roadmap view for phased delivery

**Fields to Add:**
- Estimated Effort (S/M/L/XL)
- Target Milestone (Phase 5d, Phase 6, etc.)
- Blocked By (dependency tracking)
- Related Gaps (linked issues)

## Automation Opportunities

For repositories with Actions enabled, consider:

1. **Auto-label**: Automatically apply labels based on gap ID
2. **Gap Sync**: Bot to sync issue status back to gap tracker
3. **Milestone Assignment**: Auto-assign to milestone based on priority
4. **Stale Bot**: Close completed issues after verification period

## Maintenance

### Weekly Review
- Triage new issues
- Update status labels as work progresses
- Close completed issues
- Update gap tracker with issue numbers

### Monthly Audit
- Verify all P0/P1 issues are assigned
- Review blocked issues and dependencies
- Update project timeline
- Archive resolved issues to gap tracker

## Best Practices

### DO
✅ Create issues in priority order (P0 → P1 → P2 → P3)
✅ Include KatanaCombat-specific context in issue body
✅ Link related gaps in issue description
✅ Update gap tracker with issue numbers
✅ Use consistent labeling
✅ Close issues when gap marked "Done" in tracker

### DON'T
❌ Create all 149 issues at once (overwhelming)
❌ Create issues for "Done" gaps without closing them immediately
❌ Forget to link issue back to gap tracker
❌ Use generic descriptions (be specific to combat system)
❌ Create duplicate issues for same gap

## Getting Started

### Immediate Actions

1. **Review Priority Buckets** in gap tracker (lines 304-337)
2. **Start with P0/P1 Critical** section
3. **Create 5-10 issues** from critical path
4. **Set up GitHub Project** for tracking
5. **Begin implementation** on highest priority gaps

### Example First Batch

Create these 5 issues first (highest impact):
1. GAP-22.1: GetWorld() null crash (P0)
2. GAP-22.10: CustomTimeDilation 0.0f (P1)
3. GAP-22.3: const_cast UB (P1)
4. GAP-13.1: Division by zero in sync (P1)
5. GAP-13.2: Null reference in warp (P1)

## Support Resources

- **Gap Tracker**: `docs/plans/gap-tracker.md`
- **Audit Findings**: `docs/audits/AUDIT_SYNTHESIS_2026-02-03.md`
- **Architecture**: `docs/architecture/ARCHITECTURE.md`
- **Paired Animation Spec**: `docs/specs/PAIRED_ANIMATION_SPEC.md`
- **Combat Polish Plan**: `docs/plans/combat-polish-plan.md`

## Questions?

For questions about the gap workflow or issue creation, review:
1. Gap tracker for gap details
2. Audit synthesis for context
3. Architecture docs for system understanding
