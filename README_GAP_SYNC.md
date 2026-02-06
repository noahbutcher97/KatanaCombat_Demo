# Gap Tracker → GitHub Issues Sync

Automatically syncs gaps from `docs/plans/gap-tracker.md` to GitHub issues with comprehensive KatanaCombat context.

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
