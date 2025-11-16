# Agent Coordinator - Helper script for chaining agents
# Provides utilities for pipeline orchestration

param(
    [Parameter(Mandatory=$false)]
    [string]$Action = "status",

    [Parameter(Mandatory=$false)]
    [string]$PipelineType = "feature",

    [Parameter(Mandatory=$false)]
    [string[]]$AgentChain = @()
)

# Pipeline definitions
$Pipelines = @{
    "feature" = @{
        name = "Full Feature Pipeline"
        agents = @("ue-code-generator", "design-compliance-auditor", "code-auditor")
        orchestrator = "pipeline-feature"
        description = "Implement → Validate → Audit"
    }
    "bugfix" = @{
        name = "Bug Fix Pipeline"
        agents = @("design-compliance-auditor", "ue-code-generator", "code-auditor")
        orchestrator = "pipeline-bugfix"
        description = "Diagnose → Fix → Verify"
    }
    "validation" = @{
        name = "Validation Pipeline"
        agents = @("design-compliance-auditor", "code-auditor")
        orchestrator = $null
        description = "Architecture Check → Quality Check"
    }
    "refactoring" = @{
        name = "Refactoring Pipeline"
        agents = @("code-auditor", "ue-code-generator", "design-compliance-auditor")
        orchestrator = $null
        description = "Analyze → Refactor → Validate"
    }
}

# Available agents
$AvailableAgents = @{
    "router" = @{
        model = "inherit"
        color = "purple"
        specialty = "Task routing and agent selection"
        description = "Meta-agent that intelligently routes tasks to specialists"
    }
    "ue-code-generator" = @{
        model = "opus"
        color = "green"
        specialty = "Code generation and implementation"
        description = "Generates production-ready UE5.6 C++ code with compliance"
    }
    "design-compliance-auditor" = @{
        model = "opus"
        color = "red"
        specialty = "Architecture validation"
        description = "Enforces design principles and detects violations"
    }
    "code-auditor" = @{
        model = "inherit"
        color = "orange"
        specialty = "Code quality and best practices"
        description = "Reviews code for standards adherence and optimization"
    }
    "pipeline-feature" = @{
        model = "opus"
        color = "cyan"
        specialty = "Feature implementation orchestration"
        description = "Coordinates full feature delivery pipeline"
    }
    "pipeline-bugfix" = @{
        model = "opus"
        color = "red"
        specialty = "Bug fix orchestration"
        description = "Coordinates systematic bug diagnosis and resolution"
    }
}

function Show-Status {
    Write-Host ""
    Write-Host "🤖 Agent Coordination System - Status" -ForegroundColor Cyan
    Write-Host "=====================================" -ForegroundColor Cyan
    Write-Host ""

    Write-Host "📊 Available Agents ($($AvailableAgents.Count)):" -ForegroundColor Yellow
    Write-Host ""

    foreach ($agent in $AvailableAgents.GetEnumerator() | Sort-Object Name) {
        $name = $agent.Key
        $info = $agent.Value

        $colorName = switch ($info.color) {
            "purple" { "Magenta" }
            "green" { "Green" }
            "red" { "Red" }
            "orange" { "Yellow" }
            "cyan" { "Cyan" }
            default { "White" }
        }

        Write-Host "  • $name" -ForegroundColor $colorName -NoNewline
        Write-Host " [$($info.model)]" -ForegroundColor DarkGray
        Write-Host "    Specialty: $($info.specialty)" -ForegroundColor Gray
        Write-Host ""
    }

    Write-Host ""
    Write-Host "🔄 Predefined Pipelines ($($Pipelines.Count)):" -ForegroundColor Yellow
    Write-Host ""

    foreach ($pipeline in $Pipelines.GetEnumerator() | Sort-Object Name) {
        $name = $pipeline.Key
        $info = $pipeline.Value

        Write-Host "  • $name" -ForegroundColor Cyan -NoNewline
        Write-Host ": $($info.name)" -ForegroundColor White
        Write-Host "    $($info.description)" -ForegroundColor Gray

        if ($info.orchestrator) {
            Write-Host "    Orchestrator: $($info.orchestrator)" -ForegroundColor DarkGray
        } else {
            Write-Host "    Manual coordination required" -ForegroundColor DarkGray
        }

        Write-Host ""
    }

    Write-Host ""
    Write-Host "💡 Usage Examples:" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "  # Show this status"
    Write-Host "  powershell .claude/scripts/agent-coordinator.ps1 -Action status"
    Write-Host ""
    Write-Host "  # Get pipeline info"
    Write-Host "  powershell .claude/scripts/agent-coordinator.ps1 -Action info -PipelineType feature"
    Write-Host ""
    Write-Host "  # Validate agent chain"
    Write-Host "  powershell .claude/scripts/agent-coordinator.ps1 -Action validate ``"
    Write-Host "    -AgentChain ue-code-generator,design-compliance-auditor"
    Write-Host ""
}

function Show-PipelineInfo {
    param([string]$Type)

    if (-not $Pipelines.ContainsKey($Type)) {
        Write-Host "❌ Unknown pipeline type: $Type" -ForegroundColor Red
        Write-Host "Available: $($Pipelines.Keys -join ', ')" -ForegroundColor Yellow
        return
    }

    $pipeline = $Pipelines[$Type]

    Write-Host ""
    Write-Host "🔄 Pipeline: $($pipeline.name)" -ForegroundColor Cyan
    Write-Host "=====================================" -ForegroundColor Cyan
    Write-Host ""

    Write-Host "Description: $($pipeline.description)" -ForegroundColor White
    Write-Host ""

    Write-Host "Agent Sequence:" -ForegroundColor Yellow
    for ($i = 0; $i -lt $pipeline.agents.Count; $i++) {
        $agentName = $pipeline.agents[$i]
        $agentInfo = $AvailableAgents[$agentName]

        Write-Host "  $($i + 1). $agentName" -ForegroundColor Green
        Write-Host "     $($agentInfo.specialty)" -ForegroundColor Gray
        if ($i -lt $pipeline.agents.Count - 1) {
            Write-Host "     ↓" -ForegroundColor DarkGray
        }
    }

    Write-Host ""

    if ($pipeline.orchestrator) {
        Write-Host "Orchestrator: $($pipeline.orchestrator)" -ForegroundColor Yellow
        Write-Host "Use via: Launch $($pipeline.orchestrator) agent" -ForegroundColor Gray
    } else {
        Write-Host "Orchestrator: Manual coordination" -ForegroundColor Yellow
        Write-Host "Launch agents sequentially via Task tool" -ForegroundColor Gray
    }

    Write-Host ""
}

function Validate-AgentChain {
    param([string[]]$Chain)

    Write-Host ""
    Write-Host "✅ Validating Agent Chain" -ForegroundColor Cyan
    Write-Host "=====================================" -ForegroundColor Cyan
    Write-Host ""

    $valid = $true

    Write-Host "Agents to chain: $($Chain.Count)" -ForegroundColor Yellow
    Write-Host ""

    foreach ($agentName in $Chain) {
        if ($AvailableAgents.ContainsKey($agentName)) {
            $agent = $AvailableAgents[$agentName]
            Write-Host "  ✅ $agentName" -ForegroundColor Green
            Write-Host "     $($agent.specialty)" -ForegroundColor Gray
        } else {
            Write-Host "  ❌ $agentName - NOT FOUND" -ForegroundColor Red
            $valid = $false
        }
    }

    Write-Host ""

    if ($valid) {
        Write-Host "✅ Chain is valid" -ForegroundColor Green

        # Check if matches known pipeline
        foreach ($pipeline in $Pipelines.GetEnumerator()) {
            $pipelineAgents = $pipeline.Value.agents -join ","
            $chainStr = $Chain -join ","

            if ($pipelineAgents -eq $chainStr) {
                Write-Host ""
                Write-Host "💡 This matches the '$($pipeline.Key)' pipeline" -ForegroundColor Yellow
                if ($pipeline.Value.orchestrator) {
                    Write-Host "   Use orchestrator: $($pipeline.Value.orchestrator)" -ForegroundColor Cyan
                }
            }
        }
    } else {
        Write-Host "❌ Chain has invalid agents" -ForegroundColor Red
        Write-Host ""
        Write-Host "Available agents: $($AvailableAgents.Keys -join ', ')" -ForegroundColor Yellow
    }

    Write-Host ""
}

# Main execution
switch ($Action.ToLower()) {
    "status" {
        Show-Status
    }
    "info" {
        Show-PipelineInfo -Type $PipelineType
    }
    "validate" {
        if ($AgentChain.Count -eq 0) {
            Write-Host "❌ No agents specified for validation" -ForegroundColor Red
            Write-Host "Usage: -Action validate -AgentChain agent1,agent2,agent3" -ForegroundColor Yellow
        } else {
            Validate-AgentChain -Chain $AgentChain
        }
    }
    default {
        Write-Host "❌ Unknown action: $Action" -ForegroundColor Red
        Write-Host "Available actions: status, info, validate" -ForegroundColor Yellow
    }
}
