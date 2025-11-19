# Post-Commit Hook
# Runs after successful commits

param()

# Record commit in context history
$commitHash = git rev-parse HEAD
$commitMsg = git log -1 --pretty=%B

Write-Host "Post-commit: $commitHash" -ForegroundColor Gray
Write-Host "   Message: $commitMsg" -ForegroundColor Gray

# TODO: Add post-commit actions here
# - Update context history
# - Clean temporary files
# - Notify external systems
