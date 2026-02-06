# Gap Tracker → GitHub Issues Sync

Automatically syncs gaps from `docs/plans/gap-tracker.md` to GitHub issues with comprehensive KatanaCombat context.

## Recent Improvements (2026-02-06)

### Enhanced Error Handling & Reliability

The script now includes comprehensive error handling and validation:

1. **Pre-flight Checks**
   - Verifies `GH_TOKEN`/`GITHUB_TOKEN` is set before execution
   - Validates GitHub CLI authentication
   - Checks repository issues are enabled and accessible
   - Verifies token has necessary permissions

2. **Retry Logic with Exponential Backoff**
   - Automatically retries transient errors (network issues, rate limits)
   - Uses exponential backoff (2s, 4s, 8s) for retries
   - Maximum 3 retry attempts per operation
   - Distinguishes between transient and permanent errors

3. **Failure Detection & Circuit Breaking**
   - Tracks continuous failures (stops after 5 consecutive failures)
   - Prevents infinite loops when API is unavailable
   - Provides clear error messages for each failure

4. **Enhanced Error Logging**
   - Logs HTTP response codes for all API calls
   - Captures and displays error messages from GitHub API
   - Tracks which specific gaps failed and why
   - Shows failure summary at end of execution

5. **Debug Mode**
   - New `--debug` flag for verbose logging
   - Shows timestamps, request details, and full exception traces
   - Useful for troubleshooting API issues
   - Displays command details before execution

6. **Success Criteria Validation**
   - Exits with error code if 0 issues created when operations were expected
   - Warns if failure rate exceeds 50%
   - Distinguishes between "nothing to do" vs "all failed"
   - Provides clear exit codes for CI/CD integration

### Usage Examples

```bash
# Enable debug logging for troubleshooting
python3 sync_gaps.py --create --debug --max 5

# Create issues with enhanced error handling
python3 sync_gaps.py --create --status Pending

# Test with dry-run (still validates token and permissions)
python3 sync_gaps.py --create --dry-run --debug
```

### Error Messages

The script now provides clear, actionable error messages:

- **No token**: `ERROR: No GitHub token found - Set GH_TOKEN or GITHUB_TOKEN environment variable`
- **Invalid token**: `ERROR: GitHub CLI authentication failed`
- **Issues disabled**: `ERROR: Issues are disabled in this repository - Enable issues in repository settings`
- **No permissions**: `ERROR: Token lacks permission to access issues - Ensure token has 'repo' or 'public_repo' scope`
- **Rate limit**: `WARNING: Rate limit hit, retrying in Xs (attempt Y/3)`
- **Network error**: `WARNING: Network error, retrying in Xs`
- **Multiple failures**: `ERROR: Stopping: 5 continuous failures exceeded threshold`

### Exit Codes

- `0`: Success (all operations completed, or nothing to do)
- `1`: Failure (token invalid, pre-flight check failed, or high failure rate)

## Quick Start

### Local Usage

```bash
# Set your GitHub token
export GH_TOKEN="your_token_here"

# Create all pending gaps as issues
python3 sync_gaps.py --create --status Pending

# Sync existing issues with tracker
python3 sync_gaps.py --sync

# Dry run to preview changes
python3 sync_gaps.py --create --dry-run
```

### GitHub Actions

Run from **Actions** tab → **Sync Gap Tracker to Issues**

Configure:
- **Mode**: `create` (new issues) or `sync` (update existing)
- **Status Filter**: `Pending`, `Partial`, or `All`
- **Max Issues**: `0` for unlimited or set a number

## What It Does

- **Parses** `docs/plans/gap-tracker.md` markdown tables
- **Creates** comprehensive issues with:
  - KatanaCombat 4-component architecture context
  - Proper label taxonomy (priority, status, area, type, source)
  - Implementation strategy sections
  - Acceptance criteria checklists
  - Links to all relevant documentation
- **Syncs** existing issues (closes when marked Done, can update content)
- **Rate limits** requests (2s delay between operations)

## Label Taxonomy

```
gap, system: paired-animation              # Core classification
priority: {p0|p1|p2|p3}                   # P0=critical, P1=high, P2=medium, P3=low
status: {pending|partial|done|deferred}    # Current state
area: {ai|input|animation|audio|ui|...}    # Domain-specific
type: {bug|feature|polish|edge-case}       # Nature of work
source: {audit|testing|analysis}           # Origin
```

## Features

### Create Mode
- Creates new issues for gaps without existing issues
- Skips gaps marked as Done or Deferred
- Applies all labels automatically
- Shows real-time progress

### Sync Mode
- Checks existing gap issues
- Closes issues for gaps marked Done in tracker
- Can update content (future enhancement)
- Maintains consistency between tracker and issues

### Filters
- **Status**: Only process Pending, Partial, or All gaps
- **Max**: Limit number of issues to process
- **Dry Run**: Preview changes without making them

## Requirements

- Python 3.6+
- GitHub CLI (`gh`) installed and authenticated
- GitHub token with `repo` scope (for local use)
- Or run via GitHub Actions (uses built-in token)

## Examples

```bash
# Create first 10 pending gaps
python3 sync_gaps.py --create --status Pending --max 10

# Create all pending and partial gaps
python3 sync_gaps.py --create --status All

# Sync existing issues (close Done gaps)
python3 sync_gaps.py --sync

# Preview what would be created
python3 sync_gaps.py --create --status Pending --dry-run
```

## Output

```
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  KatanaCombat Gap Tracker → GitHub Issues Sync
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

📖 Parsing gap tracker...
✅ Found 134 total gaps
🔍 Filtered to 84 gaps with status: Pending

[1/84] Creating 1.1... ✅
[2/84] Creating 1.3... ✅
...
[84/84] Creating 22.13... ✅

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
✅ Created: 84
🔄 Updated: 0
🔒 Closed: 0
⏭️  Skipped: 0
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

## Files

- **`sync_gaps.py`** - Main sync script (18KB, ~500 lines)
- **`.github/workflows/create-all-gap-issues.yml`** - GitHub Actions workflow (~120 lines)
- **`.github/ISSUE_TEMPLATE/gap-issue-template.md`** - Issue template for manual creation

## Best Practices

1. **Start with dry run** to preview changes
2. **Create in phases** - P0 first, then P1, P2, P3
3. **Run sync weekly** to close Done gaps and update content
4. **Use filters** to target specific gap categories
5. **Check output** to verify success before proceeding

## Maintenance

The script is reusable for ongoing gap tracking:

1. Update `docs/plans/gap-tracker.md` as gaps are resolved
2. Run `sync_gaps.py --sync` to close completed issues
3. Add new gaps to tracker, run `--create` to add them as issues
4. Labels and content stay consistent automatically

## Time Estimates

- **84 pending gaps**: ~3-5 minutes (2s rate limit between requests)
- **Sync check**: ~30 seconds (just checks existing issues)
- **10 issues**: ~30 seconds

## Troubleshooting

**"No GitHub token found"**
→ Set `GH_TOKEN` or `GITHUB_TOKEN` environment variable

**"gh: command not found"**
→ Install GitHub CLI: https://cli.github.com/

**Rate limit errors**
→ Script already has 2s delays; if errors persist, increase delay in code

**Permission denied**
→ Ensure token has `repo` scope with write access to issues

## Future Enhancements

- [ ] Update existing issue content when tracker changes
- [ ] Add gap→issue mapping file for faster lookups
- [ ] Support for custom issue templates per category
- [ ] Automatic milestone assignment based on priority
- [ ] Slack/Discord notifications for new gaps

---

*Part of the KatanaCombat Paired Animation System - Gap tracking automation*
