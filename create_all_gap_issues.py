#!/usr/bin/env python3
"""
Automated Gap Tracker to GitHub Issues Creator
Parses the gap tracker and creates comprehensive GitHub issues with proper labeling.
"""

import re
import json
import time
import sys
import os
from typing import List, Dict, Optional
from dataclasses import dataclass
import urllib.request
import urllib.error

@dataclass
class GapEntry:
    gap_id: str
    description: str
    priority: str
    status: str
    category: str
    category_num: int
    notes: str = ""
    
    def get_issue_title(self) -> str:
        return f"[GAP-{self.gap_id}] {self.description}"
    
    def get_priority_desc(self) -> str:
        mapping = {
            "P0": "CRITICAL - Immediate action required (crash/corruption risk)",
            "P1": "HIGH - Core functionality impact",
            "P2": "MEDIUM - Quality or feature improvement",
            "P3": "LOW - Enhancement or polish"
        }
        return mapping.get(self.priority, "Priority to be determined")
    
    def get_status_emoji(self) -> str:
        if "Done" in self.status:
            return "✅ Resolved and completed"
        elif "Pending" in self.status:
            return "⏳ Awaiting implementation"
        elif "Partial" in self.status:
            return "🔄 Partially complete, needs additional work"
        elif "Deferred" in self.status:
            return "⏸️ Postponed to future development phase"
        elif "Intentional" in self.status or "Working" in self.status:
            return "ℹ️ Functioning as designed"
        return ""
    
    def get_category_context(self) -> str:
        contexts = {
            "AI/ENEMY COORDINATION": "This gap relates to AI behavior and enemy coordination during paired animations (finishers, counters, parries).",
            "INPUT HANDLING": "This gap concerns player input management in cinematic combat sequences.",
            "ANIMATION/TIMING": "This gap involves animation synchronization between paired animation participants.",
            "AUDIO SYNCHRONIZATION": "This gap relates to audio timing and synchronization in paired animation sequences.",
            "UI/HUD": "This gap concerns user interface behavior during paired animation sequences.",
            "ENVIRONMENTAL INTERACTION": "This gap relates to environmental interaction during paired animations.",
            "STATE TRANSITIONS": "This gap involves combat state management in paired animation workflows.",
            "PERFORMANCE": "This gap relates to performance optimization of the paired animation system.",
            "RECOVERY & CLEANUP": "This gap concerns proper cleanup and state recovery after paired animations.",
            "EXTENSIBILITY": "This gap relates to future extensibility and feature additions to the paired animation system.",
            "DELEGATE WIRING": "This gap involves event delegate connections for paired animation effects.",
            "ANIMATION INSTANCE": "This gap relates to AnimInstance integration for paired animations.",
            "BUG/CRASH PREVENTION": "This gap concerns preventing crashes and bugs in the paired animation system.",
            "POLISH": "This gap relates to polish and visual quality improvements.",
            "VFX SCAFFOLDING": "This gap involves visual effects integration for paired animations.",
            "IMPLEMENTATION": "This gap relates to implementation details and code quality.",
            "EDGE CASES": "This gap concerns handling edge cases and unusual situations.",
            "PHASE 5b-4 ANALYSIS": "This gap was identified during Phase 5b-4 comprehensive analysis.",
            "GAP AUDIT": "This gap was identified during the comprehensive gap audit.",
            "TESTING SESSION": "This gap was identified during system testing.",
            "DEATH ANIMATION": "This gap relates to death animation handling in paired sequences.",
            "AUDIT FINDINGS": "This gap was discovered in the comprehensive audit (2026-02-03).",
        }
        return contexts.get(self.category, "This gap is part of the Paired Animation System development effort.")
    
    def get_labels(self) -> List[str]:
        labels = ["gap", "system: paired-animation"]
        
        # Priority label
        labels.append(f"priority: {self.priority.lower()}")
        
        # Status label
        if "Pending" in self.status:
            labels.append("status: pending")
        elif "Partial" in self.status:
            labels.append("status: partial")
        elif "Done" in self.status:
            labels.append("status: done")
        elif "Deferred" in self.status:
            labels.append("status: deferred")
        
        # Area label based on category
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
        
        if self.category in area_map:
            labels.append(area_map[self.category])
        
        return labels
    
    def generate_issue_body(self) -> str:
        body = f"""## Gap Overview
{self.description}

## Classification
**Category:** {self.category} (Section {self.category_num})  
**Priority:** {self.priority} - {self.get_priority_desc()}  
**Status:** {self.status} {self.get_status_emoji()}

## Combat System Context
{self.get_category_context()}

This gap was identified during comprehensive system auditing of the Paired Animation System (Phase 5). The KatanaCombat project implements a Ghost of Tsushima-inspired melee combat system with paired animations for finishers, counters, and parries.

### System Architecture
The combat system uses a 4-component architecture:
- **CombatComponent** - State machine, input buffering, attack execution
- **TargetingComponent** - Soft-lock targeting, motion warp setup
- **WeaponComponent** - Hit detection, weapon traces
- **HitReactionComponent** - Damage reception, reactions, death

This gap affects the Paired Animation System which orchestrates cinematic combat sequences between characters.
"""

        if self.notes:
            body += f"""
## Additional Notes
{self.notes}
"""

        body += """
## Implementation Strategy
<!-- To be completed during implementation planning -->

### Suggested Approach
1. Review gap context in `docs/plans/gap-tracker.md`
2. Analyze related code in combat components
3. Review audit findings in `docs/audits/AUDIT_SYNTHESIS_2026-02-03.md`
4. Design solution approach
5. Implement with tests
6. Update documentation

## Acceptance Criteria
- [ ] Gap resolved and implementation verified
- [ ] Unit tests added or updated to cover the fix
- [ ] Integration tests pass
- [ ] Code review completed
- [ ] Documentation updated as needed
- [ ] Gap tracker status updated to 'Done'

## Related Documentation
- **Gap Tracker**: [`docs/plans/gap-tracker.md`](../blob/main/docs/plans/gap-tracker.md)
- **Audit Synthesis**: [`docs/audits/AUDIT_SYNTHESIS_2026-02-03.md`](../blob/main/docs/audits/AUDIT_SYNTHESIS_2026-02-03.md)
- **Architecture Overview**: [`docs/architecture/ARCHITECTURE.md`](../blob/main/docs/architecture/ARCHITECTURE.md)
- **Paired Animation Spec**: [`docs/specs/PAIRED_ANIMATION_SPEC.md`](../blob/main/docs/specs/PAIRED_ANIMATION_SPEC.md)
- **API Reference**: [`docs/architecture/API_REFERENCE.md`](../blob/main/docs/architecture/API_REFERENCE.md)

## References
- Combat System: Hybrid responsive/snappy combo system with posture-based defense
- Paired Animations: Finisher execution with symmetric warp tracking, partner collision management
- Phase 5 Status: ~50% complete - Foundation laid, core combat flow needs implementation
"""
        return body


def parse_gap_tracker(filepath: str) -> List[GapEntry]:
    """Parse the gap tracker markdown file and extract all gaps."""
    gaps = []
    current_category = ""
    current_category_num = 0
    
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # Split by section headers (###)
    sections = re.split(r'\n### ', content)
    
    for section in sections[1:]:  # Skip first split (header)
        lines = section.split('\n')
        header = lines[0].strip()
        
        # Extract category
        match = re.match(r'(\d+)\.\s+(.+)', header)
        if match:
            current_category_num = int(match.group(1))
            current_category = match.group(2).upper()
        elif header.startswith('PT.'):
            current_category = header
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
                    
                    # Extract notes from status
                    notes = ""
                    if '(' in status:
                        note_match = re.search(r'\(([^)]+)\)', status)
                        if note_match:
                            notes = note_match.group(1)
                        status = re.sub(r'\s*\([^)]+\)', '', status).strip()
                    
                    gap = GapEntry(
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


def create_github_issue(gap: GapEntry, token: str, repo: str) -> Optional[str]:
    """Create a GitHub issue using REST API."""
    url = f"https://api.github.com/repos/{repo}/issues"
    
    data = {
        "title": gap.get_issue_title(),
        "body": gap.generate_issue_body(),
        "labels": gap.get_labels()
    }
    
    headers = {
        "Authorization": f"token {token}",
        "Accept": "application/vnd.github.v3+json",
        "Content-Type": "application/json"
    }
    
    try:
        req = urllib.request.Request(
            url,
            data=json.dumps(data).encode('utf-8'),
            headers=headers,
            method='POST'
        )
        
        with urllib.request.urlopen(req) as response:
            result = json.loads(response.read().decode('utf-8'))
            return result.get('html_url')
    
    except urllib.error.HTTPError as e:
        error_body = e.read().decode('utf-8')
        print(f"❌ HTTP Error {e.code}: {error_body}", file=sys.stderr)
        return None
    except Exception as e:
        print(f"❌ Error: {str(e)}", file=sys.stderr)
        return None


def main():
    print("🚀 KatanaCombat Gap Tracker → GitHub Issues Creator")
    print("=" * 70)
    
    # Configuration
    token = os.environ.get('GITHUB_TOKEN') or os.environ.get('GH_TOKEN')
    if not token:
        print("❌ Error: No GitHub token found in environment")
        print("   Set GITHUB_TOKEN or GH_TOKEN environment variable")
        return 1
    
    repo = "noahbutcher97/KatanaCombat_Demo"
    gap_tracker_path = "docs/plans/gap-tracker.md"
    
    # Parse gaps
    print(f"\n📖 Parsing gap tracker: {gap_tracker_path}")
    try:
        gaps = parse_gap_tracker(gap_tracker_path)
        print(f"✅ Found {len(gaps)} gaps to process")
    except Exception as e:
        print(f"❌ Failed to parse gap tracker: {e}")
        return 1
    
    # Filter to pending/partial only (skip done/deferred)
    active_gaps = [g for g in gaps if "Pending" in g.status or "Partial" in g.status]
    print(f"📊 Filtered to {len(active_gaps)} active gaps (Pending/Partial)")
    
    # Priority breakdown
    priority_counts = {}
    for gap in active_gaps:
        priority_counts[gap.priority] = priority_counts.get(gap.priority, 0) + 1
    
    print("\n📈 Priority Breakdown:")
    for priority in ["P0", "P1", "P2", "P3"]:
        count = priority_counts.get(priority, 0)
        if count > 0:
            print(f"   {priority}: {count} gaps")
    
    print(f"\n⚠️  About to create {len(active_gaps)} GitHub issues")
    print("   This will take approximately", len(active_gaps) * 2, "seconds (rate limiting)")
    print()
    
    # Create issues
    created = 0
    failed = 0
    issue_map = {}
    
    for i, gap in enumerate(active_gaps, 1):
        print(f"[{i}/{len(active_gaps)}] Creating: {gap.get_issue_title()[:60]}...", end=" ")
        
        issue_url = create_github_issue(gap, token, repo)
        
        if issue_url:
            print(f"✅")
            print(f"           URL: {issue_url}")
            created += 1
            issue_map[gap.gap_id] = issue_url
        else:
            print(f"❌ FAILED")
            failed += 1
        
        # Rate limiting: 2 seconds between requests
        if i < len(active_gaps):
            time.sleep(2)
    
    # Summary
    print("\n" + "=" * 70)
    print("📊 SUMMARY")
    print("=" * 70)
    print(f"✅ Successfully created: {created} issues")
    if failed > 0:
        print(f"❌ Failed to create: {failed} issues")
    print(f"📝 Total processed: {len(active_gaps)} gaps")
    print()
    
    # Save issue mapping
    if issue_map:
        mapping_file = "/tmp/gap_issue_mapping.json"
        with open(mapping_file, 'w') as f:
            json.dump(issue_map, f, indent=2)
        print(f"💾 Issue mapping saved to: {mapping_file}")
    
    return 0 if failed == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
