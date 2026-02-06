# Disposable Gap Issue Creator

## Quick Start (One-Time Use)

```bash
# 1. Set your GitHub token
export GH_TOKEN="your_token_here"

# 2. Run the script (creates ~84 issues in 3-5 minutes)
python3 create_issues_now.py

# 3. Delete the script when done
rm create_issues_now.py
rm DISPOSABLE_README.md
```

## What It Does

- Parses `docs/plans/gap-tracker.md`
- Creates GitHub issues for all Pending/Partial gaps (~84 issues)
- Applies proper labels automatically
- Shows progress and statistics

## Requirements

- Python 3.6+
- GitHub CLI (`gh`) installed and accessible
- GitHub token with `repo` scope

## Output

```
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  KatanaCombat Gap Issues Creator (Disposable)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

📖 Parsing gap tracker...
✅ Found 134 total gaps
🔍 Filtered to 84 active gaps (Pending/Partial)

📊 Priority Breakdown:
   P0: 5 gaps
   P1: 15 gaps
   P2: 40 gaps
   P3: 24 gaps

⚠️  About to create 84 GitHub issues
   Press Ctrl+C to cancel, or Enter to continue...

🚀 Creating issues...
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
[1/84] [GAP-1.1] No Attack Token System... ✅
[2/84] [GAP-1.3] AI Awareness of Paired State... ✅
...
[84/84] [GAP-22.13] Internal state vars exposed... ✅

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
✅ Created: 84 issues
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

🗑️  You can now delete this script - it's disposable!
```

## Time Estimate

- **Total time**: 3-5 minutes (2 second delay between each issue)
- **Created**: ~84 issues with comprehensive content
- **Labels**: Automatically applied (priority, status, area, type)

## After Completion

Delete both files:
```bash
rm create_issues_now.py DISPOSABLE_README.md
```

Issues will remain in your repository with proper labels and content.
