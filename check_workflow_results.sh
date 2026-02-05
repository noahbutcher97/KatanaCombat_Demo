#!/bin/bash
# Workflow Results Verification Script
# Run this to check if the gap issues were created successfully

set -euo pipefail

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  KatanaCombat Gap Issues - Workflow Results Checker"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

REPO="noahbutcher97/KatanaCombat_Demo"

# Check if gh is authenticated
if ! gh auth status &> /dev/null; then
    echo "❌ Error: GitHub CLI not authenticated"
    echo "Please run: gh auth login"
    exit 1
fi

echo "✅ GitHub CLI authenticated"
echo ""

# Check workflow runs
echo "📊 Recent Workflow Runs:"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
gh run list --workflow=create-all-gap-issues.yml --limit 5 --repo "$REPO"
echo ""

# Get the latest run
echo "🔍 Latest Workflow Run Details:"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
LATEST_RUN=$(gh run list --workflow=create-all-gap-issues.yml --limit 1 --json databaseId,status,conclusion,createdAt --jq '.[0]' --repo "$REPO")

if [ -z "$LATEST_RUN" ] || [ "$LATEST_RUN" = "null" ]; then
    echo "⚠️  No workflow runs found"
    echo ""
    echo "Have you run the workflow yet?"
    echo "Go to: https://github.com/$REPO/actions/workflows/create-all-gap-issues.yml"
    exit 0
fi

RUN_ID=$(echo "$LATEST_RUN" | jq -r '.databaseId')
STATUS=$(echo "$LATEST_RUN" | jq -r '.status')
CONCLUSION=$(echo "$LATEST_RUN" | jq -r '.conclusion')
CREATED_AT=$(echo "$LATEST_RUN" | jq -r '.createdAt')

echo "Run ID: $RUN_ID"
echo "Status: $STATUS"
echo "Conclusion: $CONCLUSION"
echo "Created: $CREATED_AT"
echo ""

if [ "$STATUS" = "completed" ]; then
    if [ "$CONCLUSION" = "success" ]; then
        echo "✅ Workflow completed successfully!"
        echo ""
        
        # Count created issues
        echo "📈 Checking Created Issues:"
        echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
        
        TOTAL_GAPS=$(gh issue list --label "gap" --limit 1000 --json number --jq '. | length' --repo "$REPO")
        echo "Total gap issues: $TOTAL_GAPS"
        
        # Count by priority
        P0_COUNT=$(gh issue list --label "priority: p0" --limit 1000 --json number --jq '. | length' --repo "$REPO")
        P1_COUNT=$(gh issue list --label "priority: p1" --limit 1000 --json number --jq '. | length' --repo "$REPO")
        P2_COUNT=$(gh issue list --label "priority: p2" --limit 1000 --json number --jq '. | length' --repo "$REPO")
        P3_COUNT=$(gh issue list --label "priority: p3" --limit 1000 --json number --jq '. | length' --repo "$REPO")
        
        echo "  P0 (Critical): $P0_COUNT"
        echo "  P1 (High):     $P1_COUNT"
        echo "  P2 (Medium):   $P2_COUNT"
        echo "  P3 (Low):      $P3_COUNT"
        echo ""
        
        # Count by status
        PENDING_COUNT=$(gh issue list --label "status: pending" --state open --limit 1000 --json number --jq '. | length' --repo "$REPO")
        PARTIAL_COUNT=$(gh issue list --label "status: partial" --state open --limit 1000 --json number --jq '. | length' --repo "$REPO")
        
        echo "Status Breakdown:"
        echo "  Pending: $PENDING_COUNT"
        echo "  Partial: $PARTIAL_COUNT"
        echo ""
        
        # Show sample issues
        echo "📋 Sample Created Issues:"
        echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
        gh issue list --label "gap" --limit 5 --json number,title,labels --jq '.[] | "#\(.number): \(.title)"' --repo "$REPO"
        echo ""
        
        echo "🔗 View All Issues:"
        echo "https://github.com/$REPO/issues?q=is%3Aissue+is%3Aopen+label%3Agap"
        echo ""
        
        # View workflow logs
        echo "📝 Workflow Summary:"
        echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
        echo "View full logs at:"
        echo "https://github.com/$REPO/actions/runs/$RUN_ID"
        
    else
        echo "❌ Workflow failed with conclusion: $CONCLUSION"
        echo ""
        echo "View logs to see what went wrong:"
        echo "https://github.com/$REPO/actions/runs/$RUN_ID"
        echo ""
        echo "Common issues:"
        echo "  - Permission errors (check workflow permissions)"
        echo "  - Gap tracker parsing errors (check file format)"
        echo "  - Rate limiting (unlikely with 2s delays)"
    fi
else
    echo "⏳ Workflow is still running (Status: $STATUS)"
    echo ""
    echo "Monitor progress at:"
    echo "https://github.com/$REPO/actions/runs/$RUN_ID"
fi

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
