# Agent Router & Coordination System

**Purpose**: Intelligent task routing and multi-agent pipeline orchestration for automated, high-quality development workflows.

---

## Quick Start

### Use the Router (Recommended)
```
"I need to implement a dodge roll system"
→ Claude uses router agent → Selects ue-code-generator or pipeline-feature
```

### Direct Agent Usage
```
Task tool → subagent_type: "ue-code-generator"
Task tool → subagent_type: "design-compliance-auditor"
Task tool → subagent_type: "code-auditor"
```

### Pipeline Orchestration
```
Task tool → subagent_type: "pipeline-feature"
Task tool → subagent_type: "pipeline-bugfix"
```

---

## Agent Inventory (6 Total)

### 1. **router** 🎯 (Meta-Agent)
**Model**: Inherit | **Color**: Purple
**Purpose**: Intelligent task routing to appropriate specialists

**When to use**:
- Unsure which agent to delegate to
- Task could benefit from multiple agents
- Want automated pipeline selection

**What it does**:
- Analyzes task type, complexity, and context
- Selects best agent(s) for the job
- Launches agents with detailed prompts
- Coordinates multi-agent pipelines

**Decision logic**:
- NEW CODE → ue-code-generator
- VALIDATE DESIGN → design-compliance-auditor
- CODE REVIEW → code-auditor
- COMPLEX FEATURE → pipeline-feature
- UNCLEAR BUG → pipeline-bugfix

---

### 2. **ue-code-generator** 🟢 (Specialist)
**Model**: Opus | **Color**: Green
**Purpose**: Production-ready UE5.6 C++ code generation

**When to use**:
- Keywords: "implement", "add", "create", "build"
- New features or functionality needed
- Code modifications with compliance requirements

**What it does**:
- Generates complete, compilable implementations
- Enforces UE5.6 API conventions and project architecture
- Provides compliance scorecard (UE5.6, Architecture, Blueprint, Requirements)
- Asks clarifying questions proactively

**Strengths**:
- Full project context (phases/windows, input buffering, etc.)
- Blueprint-friendly code generation
- Before/after comparisons

**Example**: "Add a parry counter system with damage multiplier"

---

### 3. **design-compliance-auditor** 🔴 (Specialist)
**Model**: Opus | **Color**: Red
**Purpose**: Architecture validation against 6 design principles

**When to use**:
- Keywords: "validate", "check design", "architecture review"
- After implementing features (validation step)
- Debugging architectural deviations
- Before merging significant changes

**What it does**:
- Enforces critical principles (Phases vs Windows, Input Always Buffered, etc.)
- Categorizes violations (Critical/Major/Minor)
- Provides refactoring guidance with file:line references
- Explains design rationale

**Critical Principles**:
1. Phases exclusive, Windows overlap
2. Input always buffered
3. Parry = defender checks attacker
4. Hold = button state check
5. Delegates in CombatTypes.h
6. Pragmatic component consolidation

**Example**: "Check if my combo system follows project design"

---

### 4. **code-auditor** 🟠 (Specialist)
**Model**: Inherit | **Color**: Orange
**Purpose**: Code quality, best practices, optimization

**When to use**:
- Keywords: "review", "optimize", "best practices"
- After completing implementation
- Before committing significant changes
- Assessing architectural decisions

**What it does**:
- Checks project standard compliance
- Identifies scope/optimization opportunities
- Suggests intelligent alternatives
- Provides specific file:line fixes

**Output**:
- ✅ Compliant patterns
- ⚠️ Standards deviations
- 🔍 Scope & optimization opportunities
- 💡 Intelligent alternatives

**Example**: "Review my dodge roll implementation for optimization"

---

### 5. **pipeline-feature** 🔵 (Orchestrator)
**Model**: Opus | **Color**: Cyan
**Purpose**: Full feature implementation pipeline

**When to use**:
- Complex features (multi-file, multiple systems)
- Critical systems (core combat, input handling)
- Pre-release work (high quality bar)
- User explicitly requests comprehensive implementation

**Pipeline**:
1. **ue-code-generator** → Implement feature
2. **design-compliance-auditor** → Validate against design
3. **code-auditor** → Check best practices

**Pass criteria**:
- Phase 1: Code compiles, requirements met, compliance ≥95%
- Phase 2: 0 critical violations, ≤2 major violations
- Phase 3: No critical standards deviations

**Retry logic**: If Phase 2 finds critical violations, returns to Phase 1 for fixes

**Example**: "Implement a full dodge system with i-frames and directional control"

---

### 6. **pipeline-bugfix** 🔴 (Orchestrator)
**Model**: Opus | **Color**: Red
**Purpose**: Systematic bug diagnosis and resolution

**When to use**:
- Bug cause unclear (needs diagnosis)
- Might be design violation (parry, combo, hold issues)
- Critical bugs affecting core gameplay
- Recurring bugs

**Pipeline**:
1. **design-compliance-auditor** → Diagnose root cause
2. **ue-code-generator** → Implement fix
3. **code-auditor** → Verify no regressions

**Common patterns**:
- "Parry not working" → ParryWindow on wrong animation
- "Combo breaks" → Input gating with combo window
- "Hold doesn't trigger" → Duration tracking instead of button state

**Confidence levels**: High/Medium/Low for diagnosis

**Example**: "Parry detection isn't working consistently"

---

## How the Router Works

### Step 1: Task Analysis

**Identifies keywords**:
- Implementation: implement, add, create, build
- Validation: validate, check, design compliance
- Quality: review, optimize, refactor
- Debugging: fix, bug, issue, not working

**Assesses scope**:
- Single file → code-auditor or direct
- Multi-file/systems → ue-code-generator or pipeline
- Architectural → design-compliance-auditor

**Checks context**:
- Active context mode (animation, combat-logic, etc.)
- Recent conversation history
- Urgency (critical bug vs exploration)

---

### Step 2: Decision Making

**Single Agent** (simple, well-defined):
```
User: "Add stamina cost to dodge roll"
Router: → ue-code-generator (single feature addition)
```

**Pipeline** (complex, quality-critical):
```
User: "Implement full dodge system with i-frames"
Router: → pipeline-feature (complex feature)
```

**Direct Handling** (no agent needed):
```
User: "Explain the phase system"
Router: → Answers directly (documentation query)
```

---

### Step 3: Execution

**For single agent**:
- Provides detailed prompt with context
- Launches agent
- Reports results

**For pipeline**:
- Shows execution plan (Phase 1 → 2 → 3)
- Launches agents sequentially
- Passes results between phases
- Consolidates final report

---

## Pipeline Workflows

### Feature Implementation Pipeline

**Trigger**: Complex new feature

**Flow**:
```
User Request
    ↓
[Phase 1: Implementation]
ue-code-generator
    ↓ (files created)
[Phase 2: Validation]
design-compliance-auditor
    ↓ (compliance report)
    ├─ Critical violations? → Return to Phase 1
    └─ Pass → Continue
[Phase 3: Quality Check]
code-auditor
    ↓ (quality report)
Consolidated Report
```

**Metrics**:
- Total duration
- Iterations required
- Quality score (calculated from all phases)

---

### Bug Fix Pipeline

**Trigger**: Bug with unclear root cause

**Flow**:
```
Bug Report
    ↓
[Phase 1: Diagnosis]
design-compliance-auditor
    ↓ (root cause analysis)
[Phase 2: Fix]
ue-code-generator
    ↓ (fix implementation)
[Phase 3: Verification]
code-auditor
    ↓ (regression check)
    ├─ Regressions found? → Return to Phase 2
    └─ Pass → Continue
Consolidated Report
```

**Confidence levels**: Diagnosis confidence (High/Medium/Low)

---

## Usage Examples

### Example 1: Simple Feature (Direct Agent)
**Request**: "Add a stagger duration property to AttackData"

**Router Decision**: ue-code-generator (single agent)
**Reasoning**: Simple addition, one file, low complexity

**Result**: Property added with UPROPERTY metadata, default value, validation

---

### Example 2: Complex Feature (Pipeline)
**Request**: "Implement a full dodge system with i-frames and directional control"

**Router Decision**: pipeline-feature
**Reasoning**: Multi-system (component, data, animation), high complexity

**Execution**:
- Phase 1: DodgeComponent created, input handling, i-frame logic
- Phase 2: 1 major violation found (i-frame timing), fixed
- Phase 3: Compliant, suggestions for stamina integration

**Result**: Production-ready dodge system

---

### Example 3: Architecture Validation (Direct Agent)
**Request**: "I modified combo to gate input by window. Is this correct?"

**Router Decision**: design-compliance-auditor (single agent)
**Reasoning**: Likely design violation, needs immediate check

**Result**: ❌ Critical violation found - input must always be buffered. Refactoring guidance provided.

---

### Example 4: Bug Fix (Pipeline)
**Request**: "Parry detection isn't working consistently"

**Router Decision**: pipeline-bugfix
**Reasoning**: Bug with unclear cause, likely design issue

**Execution**:
- Phase 1: Diagnosis - ParryWindow on defender's animation (should be on attacker)
- Phase 2: Moved ParryWindow, updated defender logic
- Phase 3: No regressions, safe to commit

**Result**: ✅ Parry now works consistently

---

### Example 5: Code Review (Direct Agent)
**Request**: "Review my dodge roll implementation"

**Router Decision**: code-auditor (single agent)
**Reasoning**: Quality check, not architecture validation

**Result**: Compliant patterns identified, one optimization suggestion (consider stamina cost)

---

## Agent Coordinator Script

Helper tool for managing agents:

```powershell
# Show all agents and pipelines
powershell .claude/scripts/agent-coordinator.ps1 -Action status

# Get pipeline details
powershell .claude/scripts/agent-coordinator.ps1 -Action info -PipelineType feature

# Validate custom agent chain
powershell .claude/scripts/agent-coordinator.ps1 -Action validate `
  -AgentChain ue-code-generator,design-compliance-auditor,code-auditor
```

**Output**: Agent info, pipeline details, chain validation

---

## Integration with Other Systems

### Context Modes
- Router respects active context
- Prioritizes agents based on context
- Animation context + "add notify" → ue-code-generator

### Hooks
- **agent-reminder hook**: Suggests router for complex tasks
- **Complexity threshold**: ≥3 points triggers agent suggestion

### Diagnostics
- code-auditor uses diagnostics data for reviews
- Filters false positives (Blueprint-exposed, etc.)

---

## Best Practices

### 1. **Use Router for Ambiguity**
When unsure which agent:
```
"Use router to implement dodge system"
```

### 2. **Direct Agent for Known Tasks**
When certain:
```
Task tool → ue-code-generator for implementation
Task tool → design-compliance-auditor for validation
```

### 3. **Pipeline for Quality-Critical**
Major features or pre-release:
```
Task tool → pipeline-feature for comprehensive delivery
```

### 4. **Trust the Router**
It has full project context and decision logic

---

## Performance Metrics

| Agent/Pipeline | Duration | When to Use |
|----------------|----------|-------------|
| router | ~10-20s | Task routing (always fast) |
| ue-code-generator | ~30-90s | Single feature implementation |
| design-compliance-auditor | ~20-60s | Architecture validation |
| code-auditor | ~20-40s | Code review |
| pipeline-feature | ~2-4min | Full feature delivery |
| pipeline-bugfix | ~1-3min | Bug diagnosis + fix |

---

## Troubleshooting

### "Router selected wrong agent"
**Cause**: Task description ambiguous
**Fix**: Be more specific in request, or override with direct agent selection

### "Pipeline taking too long"
**Cause**: Phase 2 validation loop (fix → re-validate)
**Fix**: Check for critical violations early, address before Phase 2

### "Agent doesn't have enough context"
**Cause**: Insufficient information in request
**Fix**: Provide files, systems affected, reproduction steps

---

## Future Enhancements

- [ ] Agent performance analytics
- [ ] Custom pipeline builder
- [ ] Agent result caching
- [ ] Parallel agent execution (where safe)
- [ ] Visual pipeline progress indicator

---

## Files Created

```
.claude/agents/
├── router.md                  ← Meta-agent for task routing
├── ue-code-generator.md       ← Code generation specialist (existing)
├── design-compliance-auditor.md ← Architecture validation (existing)
├── code-auditor.md            ← Quality review specialist (existing)
├── pipeline-feature.md        ← Feature pipeline orchestrator
├── pipeline-bugfix.md         ← Bug fix pipeline orchestrator
└── README.md                  ← This guide

.claude/scripts/
└── agent-coordinator.ps1      ← Agent management helper

.claude/hooks/
└── agent-reminder.ps1         ← Updated with router integration
```

---

**Ready to use!** The router agent will intelligently delegate tasks to the right specialists, and pipelines will orchestrate multi-agent workflows for comprehensive quality assurance.