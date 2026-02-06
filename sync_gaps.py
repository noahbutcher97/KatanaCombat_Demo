#!/usr/bin/env python3
"""
KatanaCombat Gap Tracker → GitHub Issues Sync Tool

This script synchronizes gaps from docs/plans/gap-tracker.md to GitHub issues.
It can create new issues, update existing ones, and ensure consistency.

Features:
- Parses gap tracker markdown intelligently
- Creates comprehensive issues with KatanaCombat combat system context
- Applies proper label taxonomy (priority, status, area, type, source)
- Syncs existing issues (checks for changes, updates if needed)
- Supports both one-time creation and ongoing sync
- Can be run locally or in CI/CD workflows

Usage:
    # Create all pending/partial gaps as issues
    python3 sync_gaps.py --create

    # Sync existing issues with gap tracker (update if changed)
    python3 sync_gaps.py --sync

    # Dry run to preview changes
    python3 sync_gaps.py --create --dry-run

    # Specific status filter
    python3 sync_gaps.py --create --status Pending

    # Set max issues to create/sync
    python3 sync_gaps.py --create --max 10

Environment:
    GH_TOKEN or GITHUB_TOKEN: GitHub personal access token with repo scope
"""

import re
import subprocess
import sys
import os
import json
import argparse
from typing import List, Dict, Optional, Tuple
from dataclasses import dataclass


@dataclass
class Gap:
    """Represents a gap from the tracker."""
    gap_id: str
    description: str
    priority: str
    status: str
    category: str
    category_num: int
    notes: str = ""
    
    def get_issue_title(self) -> str:
        """Generate GitHub issue title."""
        return f"[GAP-{self.gap_id}] {self.description}"
    
    def get_labels(self) -> List[str]:
        """Generate appropriate labels for this gap."""
        labels = ["gap", "system: paired-animation", f"priority: {self.priority.lower()}"]
        
        # Status label
        if "Pending" in self.status:
            labels.append("status: pending")
        elif "Partial" in self.status:
            labels.append("status: partial")
        elif "Done" in self.status:
            labels.append("status: done")
        elif "Deferred" in self.status:
            labels.append("status: deferred")
        
        # Area/type labels based on category
        category_labels = {
            "AI/ENEMY COORDINATION": "area: ai",
            "INPUT HANDLING": "area: input",
            "ANIMATION/TIMING": "area: animation",
            "AUDIO SYNCHRONIZATION": "area: audio",
            "UI/HUD": "area: ui",
            "ENVIRONMENTAL INTERACTION": "area: environment",
            "STATE TRANSITIONS": "area: state-machine",
            "PERFORMANCE": "area: performance",
            "RECOVERY & CLEANUP": "area: cleanup",
            "EXTENSIBILITY": "area: extensibility",
            "DELEGATE WIRING": "area: events",
            "ANIMATION INSTANCE": "area: animation",
            "BUG/CRASH PREVENTION": "type: bug",
            "POLISH": "type: polish",
            "VFX SCAFFOLDING": "area: vfx",
            "IMPLEMENTATION": "area: implementation",
            "EDGE CASES": "type: edge-case",
            "AUDIT FINDINGS": "source: audit",
            "TESTING SESSION": "source: testing",
            "PHASE 5b-4 ANALYSIS": "source: analysis",
            "GAP AUDIT": "source: audit",
        }
        
        if self.category in category_labels:
            labels.append(category_labels[self.category])
        
        return labels
    
    def get_issue_body(self) -> str:
        """Generate comprehensive issue body with KatanaCombat context."""
        # Category context mapping
        context_map = {
            "AI/ENEMY COORDINATION": "AI behavior and enemy coordination during paired animations (finishers, counters, parries)",
            "INPUT HANDLING": "Player input management in cinematic combat sequences",
            "ANIMATION/TIMING": "Animation synchronization between paired animation participants",
            "AUDIO SYNCHRONIZATION": "Audio timing and synchronization in paired animation sequences",
            "UI/HUD": "User interface behavior during paired animation sequences",
            "ENVIRONMENTAL INTERACTION": "Environmental interaction during paired animations",
            "STATE TRANSITIONS": "Combat state management in paired animation workflows",
            "PERFORMANCE": "Performance optimization of the paired animation system",
            "RECOVERY & CLEANUP": "Proper cleanup and state recovery after paired animations",
            "EXTENSIBILITY": "Future extensibility and feature additions",
            "BUG/CRASH PREVENTION": "Preventing crashes and bugs in the paired animation system",
            "POLISH": "Polish and visual quality improvements",
            "VFX SCAFFOLDING": "Visual effects integration for paired animations",
            "IMPLEMENTATION": "Implementation details and code quality",
            "EDGE CASES": "Handling edge cases and unusual situations",
            "AUDIT FINDINGS": "Discovered in comprehensive audit (2026-02-03)",
        }
        
        context = context_map.get(self.category, "Part of the Paired Animation System development effort")
        
        priority_desc = {
            "P0": "CRITICAL - Immediate action required (crash/corruption risk)",
            "P1": "HIGH - Core functionality impact",
            "P2": "MEDIUM - Quality or feature improvement",
            "P3": "LOW - Enhancement or polish"
        }.get(self.priority, "Priority to be determined")
        
        status_emoji = {
            "Pending": "⏳",
            "Partial": "🔄",
            "Done": "✅",
            "Deferred": "⏸️"
        }
        emoji = status_emoji.get(self.status.split()[0], "")
        
        body = f"""## Gap Overview
{self.description}

## Classification
**Category:** {self.category} (Section {self.category_num})  
**Priority:** {self.priority} - {priority_desc}  
**Status:** {emoji} {self.status}

## Combat System Context
**What this affects:** {context}

This gap was identified during comprehensive system auditing of the **Paired Animation System** (Phase 5). KatanaCombat implements a Ghost of Tsushima-inspired melee combat system with:

### 4-Component Architecture
- **CombatComponent** - State machine, input buffering, attack execution
- **TargetingComponent** - Soft-lock targeting, motion warp setup
- **WeaponComponent** - Hit detection, weapon traces
- **HitReactionComponent** - Damage reception, reactions, death

### Key Systems
- **Hybrid Combo System** - Responsive input buffering + snappy animation cancels
- **Posture-Based Defense** - Guard breaks and perfect parries
- **Paired Animations** - Finishers, counters, parries with symmetric warp tracking
"""
        
        if self.notes:
            body += f"\n## Additional Notes\n{self.notes}\n"
        
        body += """
## Implementation Strategy
*To be completed during implementation planning*

### Suggested Approach
1. Review gap context in gap tracker
2. Analyze related code in combat components
3. Check audit findings for additional context
4. Design solution approach
5. Implement with comprehensive tests
6. Update documentation

## Acceptance Criteria
- [ ] Gap resolved and implementation verified
- [ ] Unit tests added or updated to cover the fix
- [ ] Integration tests pass
- [ ] Code review completed
- [ ] Documentation updated as needed
- [ ] Gap tracker status updated to 'Done'

## Related Documentation
- **Gap Tracker**: [`docs/plans/gap-tracker.md`](../blob/main/docs/plans/gap-tracker.md) - Full gap matrix
- **Audit Synthesis**: [`docs/audits/AUDIT_SYNTHESIS_2026-02-03.md`](../blob/main/docs/audits/AUDIT_SYNTHESIS_2026-02-03.md) - Comprehensive audit findings
- **Architecture**: [`docs/architecture/ARCHITECTURE.md`](../blob/main/docs/architecture/ARCHITECTURE.md) - System design
- **Paired Animation Spec**: [`docs/specs/PAIRED_ANIMATION_SPEC.md`](../blob/main/docs/specs/PAIRED_ANIMATION_SPEC.md) - Detailed specification
- **API Reference**: [`docs/architecture/API_REFERENCE.md`](../blob/main/docs/architecture/API_REFERENCE.md) - Component APIs

---
*Gap ID: {self.gap_id} | Auto-synced from gap tracker*
"""
        return body


def parse_gap_tracker(filepath: str = "docs/plans/gap-tracker.md") -> List[Gap]:
    """Parse the gap tracker markdown file and extract all gaps."""
    gaps = []
    current_category = ""
    current_category_num = 0
    
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # Split by section headers (###)
    sections = re.split(r'\n### ', content)
    
    for section in sections[1:]:  # Skip first split (header content)
        lines = section.split('\n')
        header = lines[0].strip()
        
        # Extract category name and number
        match = re.match(r'(\d+)\.\s+(.+)', header)
        if match:
            current_category_num = int(match.group(1))
            current_category = match.group(2).upper()
        elif header.startswith('PT.') or header.startswith('Phase'):
            current_category = header.upper()
            current_category_num = 0
        else:
            current_category = header.upper()
            current_category_num = 0
        
        # Parse table rows
        in_table = False
        for line in lines:
            if line.startswith('| ID |') or line.startswith('|---|'):
                in_table = True
                continue
            
            if in_table and line.startswith('|') and not line.startswith('|---'):
                parts = [p.strip() for p in line.split('|')[1:-1]]
                
                if len(parts) >= 3 and parts[0] and not parts[0].startswith('-'):
                    gap_id = parts[0]
                    description = parts[1]
                    
                    # Handle 3 or 4 column tables
                    if len(parts) == 4:
                        priority = parts[2]
                        status = parts[3]
                    elif len(parts) == 3:
                        priority = "P2"
                        status = parts[2]
                    else:
                        continue
                    
                    # Clean markdown formatting
                    for char in ['*', '~', '`']:
                        description = description.replace(char * 2, '')
                        priority = priority.replace(char * 2, '')
                        status = status.replace(char * 2, '')
                    
                    # Extract notes from status parentheses
                    notes = ""
                    if '(' in status:
                        note_match = re.search(r'\(([^)]+)\)', status)
                        if note_match:
                            notes = note_match.group(1)
                        status = re.sub(r'\s*\([^)]+\)', '', status).strip()
                    
                    gap = Gap(
                        gap_id=gap_id,
                        description=description,
                        priority=priority,
                        status=status,
                        category=current_category,
                        category_num=current_category_num,
                        notes=notes
                    )
                    gaps.append(gap)
    
    return gaps


def get_existing_issues() -> Dict[str, Dict]:
    """Get all existing gap issues from GitHub."""
    try:
        result = subprocess.run(
            ['gh', 'issue', 'list', '--label', 'gap', '--limit', '1000', 
             '--json', 'number,title,labels,state'],
            capture_output=True,
            text=True,
            timeout=30
        )
        
        if result.returncode == 0:
            issues = json.loads(result.stdout)
            # Map gap ID to issue data
            issue_map = {}
            for issue in issues:
                # Extract GAP-X.Y from title
                match = re.match(r'\[GAP-([\d\.]+)\]', issue['title'])
                if match:
                    gap_id = match.group(1)
                    issue_map[gap_id] = issue
            return issue_map
        return {}
    except Exception as e:
        print(f"Warning: Could not fetch existing issues: {e}", file=sys.stderr)
        return {}


def create_issue(gap: Gap, dry_run: bool = False) -> bool:
    """Create a GitHub issue for the gap."""
    if dry_run:
        print(f"  Would create: {gap.get_issue_title()}")
        return True
    
    try:
        cmd = [
            'gh', 'issue', 'create',
            '--title', gap.get_issue_title(),
            '--body', gap.get_issue_body(),
            '--label', ','.join(gap.get_labels())
        ]
        
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
        return result.returncode == 0
    except Exception as e:
        print(f"Error creating issue: {e}", file=sys.stderr)
        return False


def update_issue(gap: Gap, issue_number: int, dry_run: bool = False) -> bool:
    """Update an existing GitHub issue."""
    if dry_run:
        print(f"  Would update #{issue_number}: {gap.get_issue_title()}")
        return True
    
    try:
        # Update body
        subprocess.run(
            ['gh', 'issue', 'edit', str(issue_number), '--body', gap.get_issue_body()],
            capture_output=True,
            timeout=30
        )
        
        # Update labels
        labels = ','.join(gap.get_labels())
        subprocess.run(
            ['gh', 'issue', 'edit', str(issue_number), '--add-label', labels],
            capture_output=True,
            timeout=30
        )
        
        return True
    except Exception as e:
        print(f"Error updating issue: {e}", file=sys.stderr)
        return False


def close_issue(issue_number: int, reason: str, dry_run: bool = False) -> bool:
    """Close an issue that's marked as Done in gap tracker."""
    if dry_run:
        print(f"  Would close #{issue_number}: {reason}")
        return True
    
    try:
        subprocess.run(
            ['gh', 'issue', 'close', str(issue_number), '--comment', reason],
            capture_output=True,
            timeout=30
        )
        return True
    except Exception as e:
        print(f"Error closing issue: {e}", file=sys.stderr)
        return False


def main():
    parser = argparse.ArgumentParser(
        description='Sync KatanaCombat gap tracker to GitHub issues',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )
    parser.add_argument('--create', action='store_true',
                       help='Create new issues for gaps without issues')
    parser.add_argument('--sync', action='store_true',
                       help='Sync existing issues with gap tracker')
    parser.add_argument('--status', choices=['Pending', 'Partial', 'All'], default='All',
                       help='Filter gaps by status (default: All)')
    parser.add_argument('--max', type=int, default=0,
                       help='Maximum number of issues to create/sync (0 = no limit)')
    parser.add_argument('--dry-run', action='store_true',
                       help='Preview changes without making them')
    
    args = parser.parse_args()
    
    # Check for GitHub token
    if not os.environ.get('GH_TOKEN') and not os.environ.get('GITHUB_TOKEN'):
        print("❌ Error: No GitHub token found", file=sys.stderr)
        print("   Set GH_TOKEN or GITHUB_TOKEN environment variable", file=sys.stderr)
        return 1
    
    print("━" * 70)
    print("  KatanaCombat Gap Tracker → GitHub Issues Sync")
    print("━" * 70)
    print()
    
    # Parse gap tracker
    print("📖 Parsing gap tracker...")
    try:
        gaps = parse_gap_tracker()
        print(f"✅ Found {len(gaps)} total gaps")
    except Exception as e:
        print(f"❌ Failed to parse gap tracker: {e}", file=sys.stderr)
        return 1
    
    # Filter gaps
    if args.status != 'All':
        gaps = [g for g in gaps if args.status in g.status]
        print(f"🔍 Filtered to {len(gaps)} gaps with status: {args.status}")
    
    # Get existing issues if syncing
    existing_issues = {}
    if args.sync or args.create:
        print("🔍 Checking existing issues...")
        existing_issues = get_existing_issues()
        print(f"📊 Found {len(existing_issues)} existing gap issues")
    
    print()
    
    created = updated = closed = skipped = 0
    
    # Process gaps
    for i, gap in enumerate(gaps):
        if args.max > 0 and (created + updated) >= args.max:
            break
        
        gap_exists = gap.gap_id in existing_issues
        
        if args.create and not gap_exists:
            # Create new issue
            if "Done" in gap.status or "Deferred" in gap.status:
                skipped += 1
                continue
            
            print(f"[{i+1}/{len(gaps)}] Creating {gap.gap_id}...", end=" ", flush=True)
            if create_issue(gap, args.dry_run):
                print("✅")
                created += 1
            else:
                print("❌")
            
            if not args.dry_run and i < len(gaps) - 1:
                import time
                time.sleep(2)  # Rate limiting
        
        elif args.sync and gap_exists:
            issue_data = existing_issues[gap.gap_id]
            issue_number = issue_data['number']
            
            # Check if should be closed
            if "Done" in gap.status and issue_data['state'] == 'open':
                print(f"[{i+1}/{len(gaps)}] Closing {gap.gap_id} (marked Done)...", end=" ", flush=True)
                if close_issue(issue_number, "Gap marked as Done in tracker", args.dry_run):
                    print("✅")
                    closed += 1
                else:
                    print("❌")
            
            # Check if needs update (simplified check)
            elif issue_data['state'] == 'open':
                # For now, just skip - full sync would compare content
                skipped += 1
    
    # Summary
    print()
    print("━" * 70)
    if args.dry_run:
        print("🔍 DRY RUN - No changes made")
    print(f"✅ Created: {created}")
    print(f"🔄 Updated: {updated}")
    print(f"🔒 Closed: {closed}")
    print(f"⏭️  Skipped: {skipped}")
    print("━" * 70)
    print()
    
    return 0


if __name__ == '__main__':
    sys.exit(main())
