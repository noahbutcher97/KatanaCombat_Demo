# Intelligent agent delegation reminder hook
# Only triggers for complex tasks to avoid over-delegation

$toolName = $env:TOOL_NAME
$toolArgs = $env:TOOL_ARGS

# Skip if no tool context
if (-not $toolName) { exit 0 }

# Check for user override
# New standard: CLAUDE_SKIP_AGENT_REMINDERS
# Old (backward compat): CLAUDE_NO_AGENT_REMINDERS
if ($env:CLAUDE_SKIP_AGENT_REMINDERS -eq "1" -or $env:CLAUDE_NO_AGENT_REMINDERS -eq "1") { exit 0 }

# Track complexity indicators
$complexityScore = 0

# Check for multi-file edits
if ($toolArgs -match '\.cpp.*\.h' -or $toolArgs -match '\.h.*\.cpp') {
    $complexityScore += 2
}

# Check for component/system files
if ($toolArgs -match 'CombatComponent|TargetingComponent|WeaponComponent|AttackData') {
    $complexityScore += 1
}

# Check for new file creation (higher complexity)
if ($toolName -eq 'Write' -and $toolArgs -notmatch 'README|\.md') {
    $complexityScore += 2
}

# Check for large edits (proxy: long old_string in Edit tool)
if ($toolName -eq 'Edit' -and $toolArgs.Length -gt 500) {
    $complexityScore += 1
}

# Only remind if complexity threshold met
if ($complexityScore -ge 3) {
    Write-Output @"
<system-reminder>This appears to be a complex, multi-file task. Consider delegating to specialized agents:

**Quick Routing**:
- router: Auto-select appropriate agent or pipeline based on task

**Specialist Agents**:
- ue-code-generator: New UE5.6 feature implementation with compliance scoring
- design-compliance-auditor: Validate against architectural patterns
- code-auditor: Review for best practices and optimization opportunities

**Pipelines** (for major work):
- pipeline-feature: Full implementation → validation → audit workflow
- pipeline-bugfix: Diagnosis → fix → verification workflow

💡 Tip: Use 'router' agent if unsure which specialist to choose.

Only use agents for substantial changes (multi-file, new systems, refactoring). Skip for simple edits.</system-reminder>
"@
}