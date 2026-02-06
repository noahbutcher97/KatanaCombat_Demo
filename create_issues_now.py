#!/usr/bin/env python3
"""
Disposable Gap Issue Creator
Run once to create all gap issues, then delete this file.

Usage:
    export GH_TOKEN="your_github_token"
    python3 create_issues_now.py

The script will:
1. Parse docs/plans/gap-tracker.md
2. Create comprehensive GitHub issues for all pending/partial gaps
3. Apply proper labels automatically
4. Show progress and final statistics

After completion, you can delete this file - it's disposable.
"""

import re
import subprocess
import sys
import os
from typing import List, Dict, Optional

class GapIssue:
    def __init__(self, gap_id: str, description: str, priority: str, status: str, 
                 category: str, category_num: int, notes: str = ""):
        self.gap_id = gap_id
        self.description = description
        self.priority = priority
        self.status = status
        self.category = category
        self.category_num = category_num
        self.notes = notes
    
    def get_title(self) -> str:
        return f"[GAP-{self.gap_id}] {self.description}"
    
    def get_labels(self) -> List[str]:
        labels = ["gap", "system: paired-animation", f"priority: {self.priority.lower()}"]
        
        if "Pending" in self.status:
            labels.append("status: pending")
        elif "Partial" in self.status:
            labels.append("status: partial")
        
        area_map = {
            "AI/ENEMY COORDINATION": "area: ai",
            "INPUT HANDLING": "area: input",
            "ANIMATION/TIMING": "area: animation",
            "AUDIO SYNCHRONIZATION": "area: audio",
            "UI/HUD": "area: ui",
            "ENVIRONMENTAL INTERACTION": "area: environment",
            "STATE TRANSITIONS": "area: state-machine",
            "PERFORMANCE": "area: performance",
            "RECOVERY & CLEANUP": "area: cleanup",
            "BUG/CRASH PREVENTION": "type: bug",
            "POLISH": "type: polish",
            "VFX SCAFFOLDING": "area: vfx",
            "IMPLEMENTATION": "area: implementation",
            "EDGE CASES": "type: edge-case",
            "AUDIT FINDINGS": "source: audit",
        }
        
        if self.category in area_map:
            labels.append(area_map[self.category])
        
        return labels
    
    def get_body(self) -> str:
        context_map = {
            "AI/ENEMY COORDINATION": "AI behavior and enemy coordination during paired animations",
            "INPUT HANDLING": "Player input management in cinematic combat sequences",
            "ANIMATION/TIMING": "Animation synchronization between paired animation participants",
            "AUDIO SYNCHRONIZATION": "Audio timing and synchronization",
            "UI/HUD": "User interface behavior during paired animations",
            "STATE TRANSITIONS": "Combat state management in paired animation workflows",
            "PERFORMANCE": "Performance optimization",
            "BUG/CRASH PREVENTION": "Preventing crashes and bugs",
            "IMPLEMENTATION": "Implementation details and code quality",
            "AUDIT FINDINGS": "Discovered in comprehensive audit (2026-02-03)",
        }
        
        context = context_map.get(self.category, "Part of Paired Animation System development")
        
        priority_desc = {
            "P0": "CRITICAL - Immediate action required",
            "P1": "HIGH - Core functionality impact",
            "P2": "MEDIUM - Quality improvement",
            "P3": "LOW - Enhancement or polish"
        }.get(self.priority, "Priority TBD")
        
        body = f"""## Gap Overview
{self.description}

## Classification
**Category:** {self.category} (Section {self.category_num})  
**Priority:** {self.priority} - {priority_desc}  
**Status:** {self.status}

## Combat System Context
{context}

This gap was identified during comprehensive system auditing of the Paired Animation System. The KatanaCombat project implements a Ghost of Tsushima-inspired melee combat system with paired animations for finishers, counters, and parries.

### System Architecture
The combat system uses a 4-component architecture:
- **CombatComponent** - State machine, input buffering, attack execution
- **TargetingComponent** - Soft-lock targeting, motion warp setup
- **WeaponComponent** - Hit detection, weapon traces
- **HitReactionComponent** - Damage reception, reactions, death
"""
        
        if self.notes:
            body += f"\n## Additional Notes\n{self.notes}\n"
        
        body += """
## Implementation Strategy
<!-- To be completed during implementation planning -->

## Acceptance Criteria
- [ ] Gap resolved and implementation verified
- [ ] Unit tests added or updated
- [ ] Integration tests pass
- [ ] Code review completed
- [ ] Documentation updated
- [ ] Gap tracker status updated to 'Done'

## Related Documentation
- **Gap Tracker**: [`docs/plans/gap-tracker.md`](../blob/main/docs/plans/gap-tracker.md)
- **Audit Synthesis**: [`docs/audits/AUDIT_SYNTHESIS_2026-02-03.md`](../blob/main/docs/audits/AUDIT_SYNTHESIS_2026-02-03.md)
- **Architecture**: [`docs/architecture/ARCHITECTURE.md`](../blob/main/docs/architecture/ARCHITECTURE.md)
- **Paired Animation Spec**: [`docs/specs/PAIRED_ANIMATION_SPEC.md`](../blob/main/docs/specs/PAIRED_ANIMATION_SPEC.md)
"""
        return body


def parse_gap_tracker(filepath: str) -> List[GapIssue]:
    """Parse gap tracker markdown and extract all gaps."""
    gaps = []
    current_category = ""
    current_category_num = 0
    
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()
    
    sections = re.split(r'\n### ', content)
    
    for section in sections[1:]:
        lines = section.split('\n')
        header = lines[0].strip()
        
        match = re.match(r'(\d+)\.\s+(.+)', header)
        if match:
            current_category_num = int(match.group(1))
            current_category = match.group(2).upper()
        else:
            current_category = header.upper()
            current_category_num = 0
        
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
                    
                    if len(parts) == 4:
                        priority = parts[2]
                        status = parts[3]
                    elif len(parts) == 3:
                        priority = "P2"
                        status = parts[2]
                    else:
                        continue
                    
                    # Clean markdown
                    for char in ['*', '~', '`']:
                        description = description.replace(char * 2, '')
                        priority = priority.replace(char * 2, '')
                        status = status.replace(char * 2, '')
                    
                    notes = ""
                    if '(' in status:
                        note_match = re.search(r'\(([^)]+)\)', status)
                        if note_match:
                            notes = note_match.group(1)
                        status = re.sub(r'\s*\([^)]+\)', '', status).strip()
                    
                    gap = GapIssue(gap_id, description, priority, status,
                                 current_category, current_category_num, notes)
                    gaps.append(gap)
    
    return gaps


def create_issue(gap: GapIssue) -> bool:
    """Create a GitHub issue using gh CLI."""
    try:
        cmd = [
            'gh', 'issue', 'create',
            '--title', gap.get_title(),
            '--body', gap.get_body(),
            '--label', ','.join(gap.get_labels())
        ]
        
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
        return result.returncode == 0
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        return False


def main():
    print("━" * 70)
    print("  KatanaCombat Gap Issues Creator (Disposable)")
    print("━" * 70)
    print()
    
    # Check for token
    if not os.environ.get('GH_TOKEN') and not os.environ.get('GITHUB_TOKEN'):
        print("❌ Error: No GitHub token found")
        print("   Set GH_TOKEN or GITHUB_TOKEN environment variable")
        print()
        print("   export GH_TOKEN='your_token_here'")
        return 1
    
    # Parse gaps
    print("📖 Parsing gap tracker...")
    try:
        gaps = parse_gap_tracker('docs/plans/gap-tracker.md')
        print(f"✅ Found {len(gaps)} total gaps")
    except Exception as e:
        print(f"❌ Failed to parse: {e}")
        return 1
    
    # Filter to pending/partial only
    active_gaps = [g for g in gaps if "Pending" in g.status or "Partial" in g.status]
    print(f"🔍 Filtered to {len(active_gaps)} active gaps (Pending/Partial)")
    
    # Show breakdown
    priority_counts = {}
    for gap in active_gaps:
        priority_counts[gap.priority] = priority_counts.get(gap.priority, 0) + 1
    
    print("\n📊 Priority Breakdown:")
    for p in ["P0", "P1", "P2", "P3"]:
        if p in priority_counts:
            print(f"   {p}: {priority_counts[p]} gaps")
    
    print(f"\n⚠️  About to create {len(active_gaps)} GitHub issues")
    print("   Press Ctrl+C to cancel, or Enter to continue...")
    input()
    
    print("\n🚀 Creating issues...")
    print("━" * 70)
    
    created = 0
    failed = 0
    
    for i, gap in enumerate(active_gaps, 1):
        title_short = gap.get_title()[:60]
        print(f"[{i}/{len(active_gaps)}] {title_short}...", end=" ", flush=True)
        
        if create_issue(gap):
            print("✅")
            created += 1
        else:
            print("❌")
            failed += 1
        
        # Rate limiting
        if i < len(active_gaps):
            import time
            time.sleep(2)
    
    print("\n" + "━" * 70)
    print(f"✅ Created: {created} issues")
    if failed > 0:
        print(f"❌ Failed: {failed} issues")
    print("━" * 70)
    print()
    print("🗑️  You can now delete this script - it's disposable!")
    print()
    
    return 0 if failed == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
