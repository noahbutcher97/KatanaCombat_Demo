o# /agent - Agent Coordination System

Display agent coordination status, available specialist agents, and predefined pipelines.

**Action**: Run agent-coordinator.ps1 script to display agent system status.

**Usage**:
```
/agent
/agent status
/agent info [pipeline-type]
/agent validate [agent1,agent2,...]
```

**Examples**:
- `/agent` or `/agent status` - Show all available agents and pipelines
- `/agent info feature` - Show feature pipeline details
- `/agent validate ue-code-generator,design-compliance-auditor` - Validate agent chain

---

## What This Shows

### Agent Status Display

Available specialist agents and their capabilities, predefined pipelines for common workflows, and usage guidelines for when to delegate to agents vs. handle tasks directly.

---

## Pipeline Details

### Feature Pipeline
- **ue-code-generator** → **design-compliance-auditor** → **code-auditor**
- Orchestrator: pipeline-feature
- Use for: Full feature implementation with validation and quality review

### Bugfix Pipeline
- **design-compliance-auditor** → **ue-code-generator** → **code-auditor**
- Orchestrator: pipeline-bugfix
- Use for: Systematic bug diagnosis and resolution

---

## Integration with Context Modes

The agent system integrates with context modes to provide mode-specific recommendations:

**Animation Mode**:
- Recommended: ue-code-generator, design-compliance-auditor
- Use for: AnimNotify implementation, phase system validation

**Combat Logic Mode**:
- Recommended: design-compliance-auditor, pipeline-feature
- Critical: Combat logic changes require strict validation

**Data Assets Mode**:
- Recommended: ue-code-generator, code-auditor
- Use for: Data structure design, property validation

**Testing Mode**:
- Recommended: ue-code-generator, code-auditor
- Use for: Test coverage, quality validation

---

## How to Launch Agents

Use the Task tool with subagent_type parameter to launch specialist agents for complex tasks.

**Implementation**: Call `.claude/scripts/agent-coordinator.ps1` with appropriate action parameter.