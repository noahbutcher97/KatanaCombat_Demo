# KatanaCombat Documentation

**Ghost of Tsushima-inspired melee combat system for Unreal Engine 5.6**

This is the documentation hub for KatanaCombat. All docs are organized into subdirectories by purpose.

---

## Directory Structure

```
docs/
├── README.md                    <- This file (navigation hub)
├── architecture/                <- System design & API
│   ├── ARCHITECTURE.md          <- Complete technical deep dive
│   ├── ARCHITECTURE_QUICK.md    <- Quick reference (start here)
│   └── API_REFERENCE.md         <- Component API documentation
├── guides/                      <- How-to guides
│   ├── GETTING_STARTED.md       <- Setup & installation
│   ├── ATTACK_CREATION.md       <- Attack authoring workflow
│   ├── TROUBLESHOOTING.md       <- Common issues & solutions
│   └── FINISHER_TESTING.md      <- Finisher test procedures
├── specs/                       <- Technical specifications
│   └── PAIRED_ANIMATION_SPEC.md <- Paired animation system spec
├── audits/                      <- Codebase audits
│   ├── AUDIT_SYNTHESIS_2026-02-03.md  <- Unified synthesis (start here)
│   ├── AUDIT_CLAUDE_2026-02-03.md     <- Claude 5-agent analytical audit
│   ├── AUDIT_COPILOT_2026-02-03.md    <- Copilot system-level audit
│   ├── AUDIT_EXECUTIVE_SUMMARY.md     <- Copilot quick reference
│   ├── AUDIT_ACTION_CHECKLIST.md      <- Implementation checklist
│   ├── AUDIT_VISUAL_ROADMAP.md        <- Visual roadmap & diagrams
│   └── archive/                       <- Historical audits
├── plans/                       <- Implementation plans
│   ├── README.md                <- Plan index & lifecycle
│   ├── combat-polish-plan.md    <- Normal attack effects (ACTIVE)
│   ├── gap-tracker.md           <- 149 gaps tracked (REFERENCE)
│   ├── preview-tool-refactor-plan.md  <- PT-11 refactor (COMPLETE)
│   └── archive/                 <- Completed/superseded plans
├── reference/                   <- Reference materials
│   ├── CHANGELOG.md             <- Version history & bug fixes
│   ├── ROADMAP.md               <- Planned features & status
│   ├── PAIRED_ANIMATION_RESEARCH.md <- Research notes
│   ├── SYSTEM_PROMPT.md         <- System prompt reference
│   └── LOG_ANALYSIS_2026-01-30.md   <- Test log analysis
└── archive/                     <- Historical documents
    ├── 2025-11-notes/           <- Early design notes
    ├── 2026-01-31-audits/       <- Previous audit cycle
    ├── ci-cd/                   <- CI/CD documentation
    └── validation/              <- Validation guides
```

---

## Quick Navigation

### By Task

| I want to... | Start here |
|--------------|------------|
| Understand the combat system quickly | [architecture/ARCHITECTURE_QUICK.md](architecture/ARCHITECTURE_QUICK.md) |
| Deep dive into component design | [architecture/ARCHITECTURE.md](architecture/ARCHITECTURE.md) |
| Look up a component API | [architecture/API_REFERENCE.md](architecture/API_REFERENCE.md) |
| Set up the project | [guides/GETTING_STARTED.md](guides/GETTING_STARTED.md) |
| Create a new attack | [guides/ATTACK_CREATION.md](guides/ATTACK_CREATION.md) |
| Debug a combat issue | [guides/TROUBLESHOOTING.md](guides/TROUBLESHOOTING.md) |
| Understand paired animations | [specs/PAIRED_ANIMATION_SPEC.md](specs/PAIRED_ANIMATION_SPEC.md) |
| See current audit findings | [audits/AUDIT_SYNTHESIS_2026-02-03.md](audits/AUDIT_SYNTHESIS_2026-02-03.md) |
| Check implementation gaps | [plans/gap-tracker.md](plans/gap-tracker.md) |
| See active development plans | [plans/README.md](plans/README.md) |
| View change history | [reference/CHANGELOG.md](reference/CHANGELOG.md) |
| See planned features | [reference/ROADMAP.md](reference/ROADMAP.md) |

### By Role

| Role | Documents |
|------|-----------|
| **New Developer** | [GETTING_STARTED](guides/GETTING_STARTED.md) -> [ARCHITECTURE_QUICK](architecture/ARCHITECTURE_QUICK.md) -> [ATTACK_CREATION](guides/ATTACK_CREATION.md) |
| **Combat Designer** | [ATTACK_CREATION](guides/ATTACK_CREATION.md) -> [PAIRED_ANIMATION_SPEC](specs/PAIRED_ANIMATION_SPEC.md) -> [TROUBLESHOOTING](guides/TROUBLESHOOTING.md) |
| **Continuing Development** | [AUDIT_SYNTHESIS](audits/AUDIT_SYNTHESIS_2026-02-03.md) -> [Gap Tracker](plans/gap-tracker.md) -> [Combat Polish Plan](plans/combat-polish-plan.md) |
| **AI Assistant (Claude)** | `CLAUDE.md` (auto-loaded) -> [ARCHITECTURE_QUICK](architecture/ARCHITECTURE_QUICK.md) -> [Plans README](plans/README.md) |

---

## Documentation Hierarchy

This project uses a four-level hierarchy optimized for Claude CLI:

| Level | Purpose | Location |
|-------|---------|----------|
| **L1: Working Memory** | Essential rules, patterns, quick refs | `CLAUDE.md` (auto-loaded) |
| **L2: Specifications** | Detailed system specs | `docs/specs/` |
| **L3: Architecture** | Deep dives, API details | `docs/architecture/` |
| **L4: Plans** | Active development, gap tracking | `docs/plans/` |

---

## Current Project Status

**Overall**: ~50% complete toward Batman Arkham/AC3 combat vision

| System | Status | Audit Rating |
|--------|--------|--------------|
| Core Combat (4-component) | Stable | 95% feature complete |
| Input Buffering | Stable | Working (LIFO queue bug pending fix) |
| Finisher System | Stable | 95% feature complete |
| Parry System | Not wired | 20% (stubs only) |
| Counter System | Not wired | 15% (stubs only) |
| Flow State | Not started | 10% |
| VFX/SFX | Scaffolded | 40% (properties exist, no triggers) |
| AI Coordination | Not started | 30% |
| Architecture Health | 6.5/10 | CombatComponent.cpp needs decomposition |

**Latest Audit**: [Unified Synthesis (2026-02-03)](audits/AUDIT_SYNTHESIS_2026-02-03.md) -- cross-references two parallel audits with 38 unified action items

**Active Plans**: [Plans Index](plans/README.md)

---

## Test Suite

**14 suites, 126 tests** -- see [KatanaCombatTest README](../Source/KatanaCombatTest/README.md)

Run in editor: `Window > Developer Tools > Session Frontend > Automation > Filter: "KatanaCombat"`

---

## System Requirements

- **Engine**: Unreal Engine 5.6
- **Language**: C++20
- **Plugins**: Motion Warping, State Tree, Gameplay State Tree
