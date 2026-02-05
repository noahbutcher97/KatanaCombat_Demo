# Workflow Results - Quick Check Guide

## How to Check if Issues Were Created

### Method 1: Quick Web Check (Fastest)

**Go directly to issues page**:
https://github.com/noahbutcher97/KatanaCombat_Demo/issues

**What to look for**:
- New issues with `[GAP-X.Y]` prefix in titles
- Labels: `gap`, `system: paired-animation`, `priority: p0/p1/p2/p3`
- Should see dozens of new issues if workflow succeeded

**Expected count based on what you selected**:
- If you selected "Pending" status: ~84 new issues
- If you selected "All" status: ~134 new issues  
- If you set a max count: That many issues

### Method 2: Check Workflow Status

**Go to Actions tab**:
https://github.com/noahbutcher97/KatanaCombat_Demo/actions/workflows/create-all-gap-issues.yml

**Look for**:
- ✅ Green check = Success (issues created!)
- ⏳ Yellow circle = Still running (wait a bit)
- ❌ Red X = Failed (need to troubleshoot)

**Click on the run** to see:
- Real-time logs showing each issue being created
- Final summary with success/failure count
- Any error messages if problems occurred

### Method 3: Use Verification Script

From repository root:
```bash
./check_workflow_results.sh
```

This will show you:
- Workflow status (running/completed/failed)
- Count of issues created
- Breakdown by priority (P0/P1/P2/P3)
- Sample issue titles
- Links to view everything

## What Success Looks Like

### In the Actions Tab:
```
✅ Create All Gap Issues (Automated)
   Triggered by [your username]
   Completed in 3m 24s
```

### In the Workflow Logs:
```
🚀 KatanaCombat Gap Tracker → GitHub Issues Creator
======================================================================
✅ Found 134 total gaps
🔍 Filtered to 84 gaps with status: Pending

📊 Creating 84 issues...

[1/84] [GAP-1.1] No Attack Token System... ✅
[2/84] [GAP-1.3] AI Awareness of Paired State... ✅
[3/84] [GAP-2.2] Camera Input Handling Undefined... ✅
...
[84/84] [GAP-22.13] Internal state vars exposed... ✅

======================================================================
✅ Created: 84 issues
❌ Failed: 0 issues
======================================================================
```

### In Issues Page:
- Dozens of new issues
- All labeled with `gap` and `system: paired-animation`
- Proper priority labels (p0, p1, p2, p3)
- Titles starting with `[GAP-X.Y]`
- Each issue has comprehensive content:
  - Classification section
  - Combat system context
  - Implementation strategy
  - Acceptance criteria
  - Documentation links

## Sample Issue Check

Click on any issue like `[GAP-22.1]` and verify it contains:

✅ **Title**: `[GAP-22.1] GetWorld() null crash in GetHoldDuration const query`

✅ **Labels**: 
- `gap`
- `system: paired-animation`
- `priority: p0`
- `type: bug`
- `area: implementation`
- `status: pending`

✅ **Content**:
- Gap Overview section
- Classification (Category, Priority, Status)
- Combat System Context
- Implementation Strategy
- Acceptance Criteria checklist
- Related Documentation links

## Quick Statistics Check

Run these commands to see what was created:

```bash
# Total gap issues
gh issue list --label "gap" --limit 1000 --json number | jq length

# Critical issues (P0)
gh issue list --label "priority: p0" --limit 100

# High priority issues (P1)  
gh issue list --label "priority: p1" --limit 100

# By area
gh issue list --label "area: ai" --limit 100
gh issue list --label "area: animation" --limit 100
gh issue list --label "area: audio" --limit 100
```

## If Workflow Failed

### Check the Error

1. Go to Actions tab → Click the failed run
2. Click on "create-issues" job
3. Scroll to find the error (usually in red text)
4. Look for patterns:

**Common Errors**:

```
❌ HTTP 403: Forbidden
```
→ Permission issue. Check Settings → Actions → Workflow permissions

```
❌ FileNotFoundError: docs/plans/gap-tracker.md
```
→ Gap tracker not found. Verify file exists at correct path.

```
[50/84] [GAP-X.Y] Title... ❌ HTTP 422: Validation Failed
```
→ Specific issue had invalid content. Check that gap in tracker.

```
❌ Failed: 84 issues (Created: 0)
```
→ Systematic failure. Check permissions and repository settings.

### Re-running

If it failed:
1. Note the error message
2. Fix the underlying issue
3. Go back to Actions tab
4. Click "Re-run all jobs" button

Or run again with adjusted settings:
- Reduce max issues to test with smaller batch
- Try different status filter
- Check if labels need to be created first

## Next Steps After Success

### 1. Verify Quality
Spot-check 5-10 random issues to ensure:
- Content is comprehensive
- Labels are correct
- Links work
- Formatting is good

### 2. Organize Issues
- Set up GitHub Project board
- Assign to team members
- Add to milestones
- Link related issues

### 3. Update Gap Tracker
Add issue numbers to the gap tracker for cross-reference:
```markdown
| 1.1 | No Attack Token System | P1 | Pending → #123 |
```

### 4. Close Completed Gaps
Any issues for gaps marked "Done" in tracker should be closed:
```bash
gh issue close <issue_number> --comment "Gap already resolved"
```

## Current Status

**Check Now**:
1. Visit: https://github.com/noahbutcher97/KatanaCombat_Demo/issues
2. Count issues with `gap` label
3. Verify they have proper content
4. If not there yet, check Actions tab for workflow status

**Expected Result**: 
- If you selected "Pending": ~84 new issues
- If you selected "All": ~134 new issues
- Each with comprehensive content and proper labels

---

**Quick Links**:
- Issues: https://github.com/noahbutcher97/KatanaCombat_Demo/issues?q=is%3Aissue+label%3Agap
- Actions: https://github.com/noahbutcher97/KatanaCombat_Demo/actions
- Workflow: https://github.com/noahbutcher97/KatanaCombat_Demo/actions/workflows/create-all-gap-issues.yml
