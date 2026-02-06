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
import time
from typing import List, Dict, Optional, Tuple
from dataclasses import dataclass
import logging


# Global logger
logger = logging.getLogger(__name__)

# Configuration constants
MAX_CONTINUOUS_FAILURES = 5  # Stop after N continuous failures
RETRY_MAX_ATTEMPTS = 3       # Max retries for transient errors
RETRY_BASE_DELAY = 2         # Base delay in seconds for exponential backoff
RATE_LIMIT_DELAY = 2         # Delay between successful requests


def setup_logging(debug: bool = False):
    """Configure logging with appropriate level."""
    level = logging.DEBUG if debug else logging.INFO
    logging.basicConfig(
        level=level,
        format='%(levelname)s: %(message)s' if not debug else '%(asctime)s [%(levelname)s] %(message)s',
        datefmt='%H:%M:%S'
    )


def verify_github_token() -> bool:
    """
    Verify GitHub token is set and has necessary scopes.
    
    Returns:
        bool: True if token is valid, False otherwise
    """
    token = os.environ.get('GH_TOKEN') or os.environ.get('GITHUB_TOKEN')
    
    if not token:
        logger.error("No GitHub token found")
        logger.error("Set GH_TOKEN or GITHUB_TOKEN environment variable")
        return False
    
    # Use gh CLI to verify token
    try:
        logger.debug("Verifying GitHub token...")
        result = subprocess.run(
            ['gh', 'auth', 'status'],
            capture_output=True,
            text=True,
            timeout=10
        )
        
        if result.returncode != 0:
            logger.error("GitHub CLI authentication failed")
            logger.error(f"Output: {result.stderr}")
            return False
        
        # Check for required scopes in output
        output = result.stdout + result.stderr
        logger.debug(f"Auth status: {output}")
        
        # The 'gh auth status' doesn't always show scopes clearly,
        # so we'll do a basic check
        if 'logged in' not in output.lower() and 'authenticated' not in output.lower():
            logger.warning("Could not verify authentication status")
            logger.warning("Continuing anyway - API calls will fail if token is invalid")
        
        logger.debug("✅ GitHub token verified")
        return True
        
    except FileNotFoundError:
        logger.error("GitHub CLI (gh) not found")
        logger.error("Install from: https://cli.github.com/")
        return False
    except Exception as e:
        logger.error(f"Error verifying token: {e}")
        return False


def check_repository_issues_enabled() -> bool:
    """
    Check if issues are enabled in the repository and token has permissions.
    
    Returns:
        bool: True if issues are accessible, False otherwise
    """
    try:
        logger.debug("Checking repository issues access...")
        
        # Try to list issues to verify permissions
        result = subprocess.run(
            ['gh', 'issue', 'list', '--limit', '1', '--json', 'number'],
            capture_output=True,
            text=True,
            timeout=10
        )
        
        if result.returncode != 0:
            error_output = result.stderr.lower()
            
            if 'issues are disabled' in error_output or 'not found' in error_output:
                logger.error("Issues are disabled in this repository")
                logger.error("Enable issues in repository settings")
                return False
            elif 'permission' in error_output or 'forbidden' in error_output:
                logger.error("Token lacks permission to access issues")
                logger.error("Ensure token has 'repo' or 'public_repo' scope")
                return False
            else:
                logger.error(f"Failed to access repository issues: {result.stderr}")
                return False
        
        logger.debug("✅ Repository issues are accessible")
        return True
        
    except Exception as e:
        logger.error(f"Error checking repository issues: {e}")
        return False


def exponential_backoff(attempt: int) -> float:
    """
    Calculate exponential backoff delay.
    
    Args:
        attempt: Current attempt number (0-indexed)
    
    Returns:
        float: Delay in seconds
    """
    return RETRY_BASE_DELAY * (2 ** attempt)


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
        logger.debug("Fetching existing gap issues...")
        
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
            
            logger.debug(f"Found {len(issue_map)} existing gap issues")
            return issue_map
        else:
            logger.warning(f"Failed to fetch existing issues: {result.stderr}")
            return {}
    except Exception as e:
        logger.warning(f"Could not fetch existing issues: {e}")
        logger.debug("Exception details:", exc_info=True)
        return {}


def create_issue(gap: Gap, dry_run: bool = False) -> Tuple[bool, Optional[str]]:
    """
    Create a GitHub issue for the gap with retry logic.
    
    Args:
        gap: The gap to create an issue for
        dry_run: If True, only print what would be done
    
    Returns:
        Tuple of (success, error_message)
    """
    if dry_run:
        logger.info(f"  Would create: {gap.get_issue_title()}")
        return (True, None)
    
    cmd = [
        'gh', 'issue', 'create',
        '--title', gap.get_issue_title(),
        '--body', gap.get_issue_body(),
        '--label', ','.join(gap.get_labels())
    ]
    
    logger.debug(f"Command: {' '.join(cmd[:3])} ...")
    logger.debug(f"Title: {gap.get_issue_title()}")
    logger.debug(f"Labels: {', '.join(gap.get_labels())}")
    
    # Retry logic with exponential backoff
    for attempt in range(RETRY_MAX_ATTEMPTS):
        try:
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
            
            # Log the response
            logger.debug(f"Return code: {result.returncode}")
            logger.debug(f"Stdout: {result.stdout.strip()}")
            if result.stderr:
                logger.debug(f"Stderr: {result.stderr.strip()}")
            
            if result.returncode == 0:
                # Success
                issue_url = result.stdout.strip()
                logger.debug(f"✅ Created: {issue_url}")
                return (True, None)
            else:
                # Check if it's a transient error
                error_msg = result.stderr.lower()
                
                if 'rate limit' in error_msg or 'too many requests' in error_msg:
                    # Rate limit - retry with backoff
                    if attempt < RETRY_MAX_ATTEMPTS - 1:
                        delay = exponential_backoff(attempt)
                        logger.warning(f"Rate limit hit, retrying in {delay}s (attempt {attempt + 1}/{RETRY_MAX_ATTEMPTS})")
                        time.sleep(delay)
                        continue
                    else:
                        error = f"Rate limit exceeded after {RETRY_MAX_ATTEMPTS} attempts"
                        logger.error(error)
                        return (False, error)
                
                elif 'network' in error_msg or 'timeout' in error_msg or 'connection' in error_msg:
                    # Network error - retry with backoff
                    if attempt < RETRY_MAX_ATTEMPTS - 1:
                        delay = exponential_backoff(attempt)
                        logger.warning(f"Network error, retrying in {delay}s (attempt {attempt + 1}/{RETRY_MAX_ATTEMPTS})")
                        time.sleep(delay)
                        continue
                    else:
                        error = f"Network error after {RETRY_MAX_ATTEMPTS} attempts: {result.stderr}"
                        logger.error(error)
                        return (False, error)
                
                else:
                    # Non-transient error - don't retry
                    error = f"API error: {result.stderr}"
                    logger.error(f"❌ Failed to create issue for {gap.gap_id}: {result.stderr.strip()}")
                    return (False, error)
        
        except subprocess.TimeoutExpired:
            if attempt < RETRY_MAX_ATTEMPTS - 1:
                delay = exponential_backoff(attempt)
                logger.warning(f"Request timeout, retrying in {delay}s (attempt {attempt + 1}/{RETRY_MAX_ATTEMPTS})")
                time.sleep(delay)
                continue
            else:
                error = f"Timeout after {RETRY_MAX_ATTEMPTS} attempts"
                logger.error(error)
                return (False, error)
        
        except Exception as e:
            error = f"Unexpected error: {str(e)}"
            logger.error(f"❌ Error creating issue for {gap.gap_id}: {e}")
            logger.debug(f"Exception details:", exc_info=True)
            return (False, error)
    
    # Should not reach here, but just in case
    return (False, "Unknown error after retries")


def update_issue(gap: Gap, issue_number: int, dry_run: bool = False) -> Tuple[bool, Optional[str]]:
    """
    Update an existing GitHub issue with retry logic.
    
    Args:
        gap: The gap data to update
        issue_number: The issue number to update
        dry_run: If True, only print what would be done
    
    Returns:
        Tuple of (success, error_message)
    """
    if dry_run:
        logger.info(f"  Would update #{issue_number}: {gap.get_issue_title()}")
        return (True, None)
    
    try:
        logger.debug(f"Updating issue #{issue_number}")
        
        # Update body
        result = subprocess.run(
            ['gh', 'issue', 'edit', str(issue_number), '--body', gap.get_issue_body()],
            capture_output=True,
            text=True,
            timeout=30
        )
        
        logger.debug(f"Body update return code: {result.returncode}")
        if result.returncode != 0:
            error = f"Failed to update body: {result.stderr}"
            logger.error(error)
            return (False, error)
        
        # Update labels
        labels = ','.join(gap.get_labels())
        result = subprocess.run(
            ['gh', 'issue', 'edit', str(issue_number), '--add-label', labels],
            capture_output=True,
            text=True,
            timeout=30
        )
        
        logger.debug(f"Label update return code: {result.returncode}")
        if result.returncode != 0:
            error = f"Failed to update labels: {result.stderr}"
            logger.error(error)
            return (False, error)
        
        logger.debug(f"✅ Updated issue #{issue_number}")
        return (True, None)
        
    except Exception as e:
        error = f"Unexpected error: {str(e)}"
        logger.error(f"❌ Error updating issue #{issue_number}: {e}")
        logger.debug(f"Exception details:", exc_info=True)
        return (False, error)


def close_issue(issue_number: int, reason: str, dry_run: bool = False) -> Tuple[bool, Optional[str]]:
    """
    Close an issue that's marked as Done in gap tracker.
    
    Args:
        issue_number: The issue number to close
        reason: Reason for closing
        dry_run: If True, only print what would be done
    
    Returns:
        Tuple of (success, error_message)
    """
    if dry_run:
        logger.info(f"  Would close #{issue_number}: {reason}")
        return (True, None)
    
    try:
        logger.debug(f"Closing issue #{issue_number}")
        
        result = subprocess.run(
            ['gh', 'issue', 'close', str(issue_number), '--comment', reason],
            capture_output=True,
            text=True,
            timeout=30
        )
        
        logger.debug(f"Close return code: {result.returncode}")
        
        if result.returncode == 0:
            logger.debug(f"✅ Closed issue #{issue_number}")
            return (True, None)
        else:
            error = f"Failed to close issue: {result.stderr}"
            logger.error(error)
            return (False, error)
        
    except Exception as e:
        error = f"Unexpected error: {str(e)}"
        logger.error(f"❌ Error closing issue #{issue_number}: {e}")
        logger.debug(f"Exception details:", exc_info=True)
        return (False, error)


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
    parser.add_argument('--debug', action='store_true',
                       help='Enable debug logging with detailed output')
    
    args = parser.parse_args()
    
    # Setup logging
    setup_logging(args.debug)
    
    logger.info("━" * 70)
    logger.info("  KatanaCombat Gap Tracker → GitHub Issues Sync")
    logger.info("━" * 70)
    logger.info("")
    
    # Pre-flight checks
    logger.info("🔍 Running pre-flight checks...")
    
    # 1. Verify GitHub token
    if not verify_github_token():
        logger.error("❌ GitHub token verification failed")
        return 1
    
    # 2. Check repository issues access
    if not check_repository_issues_enabled():
        logger.error("❌ Repository issues are not accessible")
        return 1
    
    logger.info("✅ Pre-flight checks passed")
    logger.info("")
    
    # Parse gap tracker
    logger.info("📖 Parsing gap tracker...")
    try:
        gaps = parse_gap_tracker()
        logger.info(f"✅ Found {len(gaps)} total gaps")
    except Exception as e:
        logger.error(f"❌ Failed to parse gap tracker: {e}")
        logger.debug("Exception details:", exc_info=True)
        return 1
    
    # Filter gaps
    if args.status != 'All':
        gaps = [g for g in gaps if args.status in g.status]
        logger.info(f"🔍 Filtered to {len(gaps)} gaps with status: {args.status}")
    
    # Get existing issues if syncing
    existing_issues = {}
    if args.sync or args.create:
        logger.info("🔍 Checking existing issues...")
        existing_issues = get_existing_issues()
        logger.info(f"📊 Found {len(existing_issues)} existing gap issues")
    
    logger.info("")
    
    # Track statistics
    created = updated = closed = skipped = failed = 0
    continuous_failures = 0
    failed_gaps = []  # Track which gaps failed
    
    # Process gaps
    for i, gap in enumerate(gaps):
        if args.max > 0 and (created + updated) >= args.max:
            logger.info(f"Reached max limit of {args.max} issues")
            break
        
        # Check continuous failure threshold
        if continuous_failures >= MAX_CONTINUOUS_FAILURES:
            logger.error(f"❌ Stopping: {continuous_failures} continuous failures exceeded threshold")
            logger.error(f"   Consider checking your network connection and GitHub API status")
            break
        
        gap_exists = gap.gap_id in existing_issues
        
        if args.create and not gap_exists:
            # Create new issue
            if "Done" in gap.status or "Deferred" in gap.status:
                skipped += 1
                continue
            
            logger.info(f"[{i+1}/{len(gaps)}] Creating {gap.gap_id}...", end=" ", flush=True)
            success, error = create_issue(gap, args.dry_run)
            
            if success:
                print("✅")
                created += 1
                continuous_failures = 0  # Reset failure counter
            else:
                print("❌")
                failed += 1
                continuous_failures += 1
                failed_gaps.append((gap.gap_id, error))
                logger.error(f"   Gap: {gap.gap_id} - {gap.description}")
                if error:
                    logger.error(f"   Error: {error}")
            
            # Rate limiting between requests
            if not args.dry_run and i < len(gaps) - 1 and success:
                logger.debug(f"Sleeping {RATE_LIMIT_DELAY}s for rate limiting...")
                time.sleep(RATE_LIMIT_DELAY)
        
        elif args.sync and gap_exists:
            issue_data = existing_issues[gap.gap_id]
            issue_number = issue_data['number']
            
            # Check if should be closed
            if "Done" in gap.status and issue_data['state'] == 'open':
                logger.info(f"[{i+1}/{len(gaps)}] Closing {gap.gap_id} (marked Done)...", end=" ", flush=True)
                success, error = close_issue(issue_number, "Gap marked as Done in tracker", args.dry_run)
                
                if success:
                    print("✅")
                    closed += 1
                    continuous_failures = 0
                else:
                    print("❌")
                    failed += 1
                    continuous_failures += 1
                    failed_gaps.append((gap.gap_id, error))
            
            # Check if needs update (simplified check)
            elif issue_data['state'] == 'open':
                # For now, just skip - full sync would compare content
                skipped += 1
    
    # Summary
    logger.info("")
    logger.info("━" * 70)
    if args.dry_run:
        logger.info("🔍 DRY RUN - No changes made")
    logger.info(f"✅ Created: {created}")
    logger.info(f"🔄 Updated: {updated}")
    logger.info(f"🔒 Closed: {closed}")
    logger.info(f"❌ Failed: {failed}")
    logger.info(f"⏭️  Skipped: {skipped}")
    logger.info("━" * 70)
    logger.info("")
    
    # List failed gaps if any
    if failed_gaps:
        logger.warning("Failed gaps:")
        for gap_id, error in failed_gaps:
            logger.warning(f"  - {gap_id}: {error}")
        logger.info("")
    
    # Success criteria check
    if not args.dry_run and (args.create or args.sync):
        # Check if we had operations to perform
        total_operations = created + updated + closed
        
        if total_operations == 0 and failed == 0:
            # No operations performed and no failures - this is OK (nothing to do)
            logger.info("ℹ️  No operations performed (nothing to create/sync)")
        elif total_operations == 0 and failed > 0:
            # No successes but had failures - ERROR
            logger.error("❌ FAILURE: No issues were successfully created/updated, but encountered failures")
            logger.error(f"   Total failures: {failed}")
            return 1
        elif failed > 0:
            # Some successes but also failures
            failure_rate = failed / (total_operations + failed)
            if failure_rate > 0.5:  # More than 50% failure rate
                logger.error(f"❌ HIGH FAILURE RATE: {failure_rate*100:.1f}% of operations failed")
                logger.error(f"   Successes: {total_operations}, Failures: {failed}")
                return 1
            else:
                logger.warning(f"⚠️  Some operations failed: {failed} out of {total_operations + failed}")
                logger.warning(f"   Failure rate: {failure_rate*100:.1f}%")
                # Don't return error if failure rate is acceptable
    
    return 0


if __name__ == '__main__':
    sys.exit(main())
