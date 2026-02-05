# Workflow Run Monitoring Guide

## Current Status: Monitoring Workflow Run

You've just triggered the "Create All Gap Issues (Automated)" workflow. Here's what to expect and how to monitor it.

## How to Monitor the Workflow

### Via GitHub Web UI

1. **Go to Actions Tab**:
   https://github.com/noahbutcher97/KatanaCombat_Demo/actions

2. **Find Your Run**:
   - Look for "Create All Gap Issues (Automated)" workflow
   - Should show as "Running" (yellow circle) or "Completed" (green check)
   - Click on the most recent run

3. **View Progress**:
   - Click on the "create-issues" job
   - Watch real-time log output as issues are created
   - Each issue creation will show: `[N/Total] [GAP-X.Y] Title... ✅`

4. **Check Summary**:
   - After completion, view the job summary at the top
   - Shows total created vs failed

### What You Should See

**Successful Run**:
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

## Expected Timeline

Based on your configuration:

| Configuration | Time Estimate |
|--------------|---------------|
| 5 test issues | ~30 seconds |
| 10 issues | ~45 seconds |
| 50 issues | ~2 minutes |
| 84 pending gaps | ~3-4 minutes |
| 149 all gaps | ~5-6 minutes |

*Time includes: checkout (5s) + parsing (2s) + creation (2s per issue)*

## Verifying Success

### 1. Check Issue Count

Visit: https://github.com/noahbutcher97/KatanaCombat_Demo/issues

**Expected**: New issues labeled with:
- `gap`
- `system: paired-animation`
- `priority: p0/p1/p2/p3`
- `status: pending`
- Area labels (e.g., `area: ai`, `area: animation`)

### 2. Spot Check Issues

Open a few issues and verify they contain:
- ✅ Proper title: `[GAP-X.Y] Description`
- ✅ Classification section (Category, Priority, Status)
- ✅ Combat System Context
- ✅ Implementation Strategy section
- ✅ Acceptance Criteria checklist
- ✅ Related Documentation links
- ✅ Proper labels applied

### 3. Check Label Distribution

Filter issues by label:
- `priority: p0` - Should have ~5 critical issues
- `priority: p1` - Should have ~15 high priority issues
- `priority: p2` - Should have ~40 medium priority issues
- `priority: p3` - Should have ~24 low priority issues
- `area: ai` - AI-related gaps
- `area: animation` - Animation-related gaps
- etc.

## Common Issues and Solutions

### Issue: Workflow Fails to Start

**Symptoms**: No workflow run appears in Actions tab

**Causes**:
- Workflow file not merged to main branch yet
- Workflow file has syntax errors
- Branch doesn't have workflows enabled

**Solutions**:
1. Verify workflow file exists at `.github/workflows/create-all-gap-issues.yml`
2. Check YAML syntax: `yamllint .github/workflows/create-all-gap-issues.yml`
3. Merge PR to main branch if workflow only on feature branch

### Issue: Workflow Starts but Fails Immediately

**Symptoms**: Red X, fails within seconds

**Causes**:
- Python syntax error in embedded script
- Missing dependencies
- Git checkout issues

**Solutions**:
1. Check workflow logs for error message
2. Look for Python traceback
3. Verify gap tracker file exists at `docs/plans/gap-tracker.md`

### Issue: Creates Some Issues but Fails Partway

**Symptoms**: Some issues created, then stops with errors

**Causes**:
- Rate limiting (should not occur with 2s delay)
- Permission issues with `github.token`
- Invalid issue content (e.g., malformed labels)

**Solutions**:
1. Check how many issues were created successfully
2. Look at the last failed issue in logs
3. Note the gap ID where it failed
4. Re-run workflow with `Max Issues` set to remaining count

### Issue: All Issues Fail to Create

**Symptoms**: 0 issues created, all marked as failed

**Causes**:
- `github.token` lacks permissions (unlikely in Actions context)
- Repository settings block issue creation
- Network/API issues

**Solutions**:
1. Check workflow has `permissions: issues: write`
2. Verify repository allows issues (Settings → Features → Issues enabled)
3. Check Actions permissions (Settings → Actions → Workflow permissions)

### Issue: Issues Created but Without Labels

**Symptoms**: Issues exist but no labels applied

**Causes**:
- Label names don't exist in repository yet
- Insufficient permissions to add labels

**Solutions**:
1. Manually create missing labels in repository
2. Re-run workflow with just failed issues
3. Or manually add labels to existing issues

## Workflow Logs Interpretation

### Key Log Sections

**1. Startup**:
```
🚀 KatanaCombat Gap Tracker → GitHub Issues Creator
✅ Found 134 total gaps
```
✅ Good: Gap tracker parsed successfully

**2. Filtering**:
```
🔍 Filtered to 84 gaps with status: Pending
```
✅ Good: Filtering logic working

**3. Creation Progress**:
```
[1/84] [GAP-1.1] No Attack Token System... ✅
```
✅ Good: Issue created successfully

```
[5/84] [GAP-2.5] Menu Input During Cinematics... ❌ HTTP 422: Validation Failed
```
❌ Problem: Issue creation failed - check error message

**4. Summary**:
```
✅ Created: 84 issues
❌ Failed: 0 issues
```
✅ Good: All issues created successfully

## Re-running the Workflow

If you need to run again:

1. **Full Re-run**: 
   - Same configuration
   - Useful if first run failed completely

2. **Adjust Configuration**:
   - Change `Max Issues` to limit scope
   - Change `Status Filter` to target specific gaps
   - Use for partial re-runs or testing

3. **Clean Up First** (if needed):
   - If duplicates created, close old issues
   - Or delete test issues before production run

## Next Steps After Completion

### If Successful:

1. **Verify Issue Quality**: Spot-check 5-10 random issues
2. **Set Up GitHub Project**: Create project board to track progress
3. **Assign Issues**: Tag team members on relevant gaps
4. **Update Gap Tracker**: Add issue numbers to tracker for cross-reference
5. **Close Completed Gaps**: Issues for "Done" gaps should be closed

### If Failed:

1. **Share Error Logs**: Copy error messages for troubleshooting
2. **Identify Pattern**: Was it one gap type? Specific category?
3. **Adjust Script**: May need to fix parsing logic or issue template
4. **Re-run Selectively**: Target just the failed gaps

## Monitoring Commands

From repository root:

```bash
# Count created issues with 'gap' label
gh issue list --label "gap" --json number | jq length

# List P0 critical issues
gh issue list --label "priority: p0" --limit 20

# Check specific gap
gh issue list --search "[GAP-1.1]" --limit 1

# View recent issue
gh issue view <issue_number>
```

## Current Workflow Configuration

Based on what you selected when running:
- **Branch**: copilot/create-git-issues-for-gaps
- **Status Filter**: (Your selection - Pending/Partial/All)
- **Max Issues**: (Your selection - 0 for unlimited or specific number)

## Contact Points

If issues arise:
1. Check this monitoring guide
2. Review workflow logs in Actions tab
3. Share specific error messages for troubleshooting
4. Can re-run workflow with adjusted parameters

---

**Status**: Workflow running - monitoring in progress
**Refresh this guide**: After completion with actual results
