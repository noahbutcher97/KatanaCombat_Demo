#!/bin/bash
# Automated Gap Issue Creation - Requires valid GH_TOKEN
#
# This script creates GitHub issues from all active (Pending/Partial) gaps
# in the gap tracker using the GitHub CLI.
#
# Usage:
#   export GH_TOKEN="your_valid_token_here"
#   ./create_all_issues.sh

set -euo pipefail

REPO="noahbutcher97/KatanaCombat_Demo"
GAP_TRACKER="docs/plans/gap-tracker.md"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${BLUE}  KatanaCombat Gap Tracker → GitHub Issues Creator${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""

# Check if gh is installed
if ! command -v gh &> /dev/null; then
    echo -e "${RED}❌ Error: GitHub CLI (gh) is not installed${NC}"
    echo "Install from: https://cli.github.com/"
    exit 1
fi

# Check if authenticated
if ! gh auth status &> /dev/null; then
    echo -e "${RED}❌ Error: Not authenticated to GitHub${NC}"
    echo "Please set GH_TOKEN environment variable or run: gh auth login"
    exit 1
fi

echo -e "${GREEN}✅ GitHub CLI authenticated${NC}"
echo ""

# Count gaps in tracker
echo -e "${YELLOW}📊 Analyzing gap tracker...${NC}"
pending_count=$(grep -E "^\| [0-9]+\.[0-9]+ \|" "$GAP_TRACKER" | grep -i "pending" | wc -l)
partial_count=$(grep -E "^\| [0-9]+\.[0-9]+ \|" "$GAP_TRACKER" | grep -i "partial" | wc -l)
total_active=$((pending_count + partial_count))

echo "   Pending gaps: $pending_count"
echo "   Partial gaps: $partial_count"
echo "   Total to create: $total_active"
echo ""

if [ $total_active -eq 0 ]; then
    echo -e "${YELLOW}No active gaps to process${NC}"
    exit 0
fi

# Confirmation
echo -e "${YELLOW}⚠️  About to create $total_active GitHub issues${NC}"
echo -e "${YELLOW}   This will take approximately $((total_active * 2)) seconds${NC}"
echo ""
read -p "Continue? (yes/no): " confirm

if [ "$confirm" != "yes" ] && [ "$confirm" != "y" ]; then
    echo "Cancelled."
    exit 0
fi

echo ""
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${BLUE}  Creating Issues...${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""

# Create issues from prepared markdown files
created=0
failed=0

# Process critical (P0) issues
if [ -d ".github/gap-issues/critical" ]; then
    for file in .github/gap-issues/critical/*.md; do
        if [ -f "$file" ]; then
            filename=$(basename "$file")
            echo -n "Creating P0 issue: $filename... "
            
            if gh issue create -F "$file" --repo "$REPO" > /dev/null 2>&1; then
                echo -e "${GREEN}✅${NC}"
                ((created++))
            else
                echo -e "${RED}❌${NC}"
                ((failed++))
            fi
            sleep 2
        fi
    done
fi

# Process high priority (P1) issues
if [ -d ".github/gap-issues/high-priority" ]; then
    for file in .github/gap-issues/high-priority/*.md; do
        if [ -f "$file" ]; then
            filename=$(basename "$file")
            echo -n "Creating P1 issue: $filename... "
            
            if gh issue create -F "$file" --repo "$REPO" > /dev/null 2>&1; then
                echo -e "${GREEN}✅${NC}"
                ((created++))
            else
                echo -e "${RED}❌${NC}"
                ((failed++))
            fi
            sleep 2
        fi
    done
fi

# Process medium priority (P2) issues  
if [ -d ".github/gap-issues/medium-priority" ]; then
    echo ""
    echo -e "${YELLOW}📝 Note: Medium priority (P2) issues not yet created as markdown files${NC}"
    echo "   Create markdown files in .github/gap-issues/medium-priority/ to include them"
fi

# Process low priority (P3) issues
if [ -d ".github/gap-issues/low-priority" ]; then
    echo -e "${YELLOW}📝 Note: Low priority (P3) issues not yet created as markdown files${NC}"
    echo "   Create markdown files in .github/gap-issues/low-priority/ to include them"
fi

echo ""
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${BLUE}  Summary${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""
echo -e "${GREEN}✅ Successfully created: $created issues${NC}"
if [ $failed -gt 0 ]; then
    echo -e "${RED}❌ Failed to create: $failed issues${NC}"
fi
echo ""
echo -e "${YELLOW}📋 Next steps:${NC}"
echo "   1. Review created issues in the repository"
echo "   2. Create more issue markdown files for remaining gaps"
echo "   3. Run this script again to create additional issues"
echo "   4. Update gap tracker with issue numbers"
echo ""

exit 0
