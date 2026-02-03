# KatanaCombat: Visual Implementation Roadmap
**Based on**: Comprehensive Audit 2026-02-03  
**Timeline**: 3-4 Weeks to Full Vision

---

## Current State: 65% Complete

```
┌─────────────────────────────────────────────────────────────┐
│                  KATANACOMBAT COMPLETION                    │
├─────────────────────────────────────────────────────────────┤
│ ████████████████████████████████████████░░░░░░░░░░░░░░░░  │
│                      65% Complete                            │
└─────────────────────────────────────────────────────────────┘

Core Combat:      ██████████████████████████████████████  95%
Input Buffering:  ██████████████████████████████████████ 100%
Finisher System:  ██████████████████████████████████████  95%
Parry System:     ████░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░  20%
Counter System:   ███░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░  15%
Flow State:       ██░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░  10%
VFX/SFX:          ████████░░░░░░░░░░░░░░░░░░░░░░░░░░░░░  40%
AI/Tokens:        ██████░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░  30%
```

---

## System Dependencies

```
                    ┌──────────────────┐
                    │  Core Combat     │
                    │  (COMPLETE 95%)  │
                    └────────┬─────────┘
                             │
              ┌──────────────┼──────────────┐
              │              │              │
    ┌─────────▼────────┐    │    ┌─────────▼─────────┐
    │  Parry System    │    │    │  Finisher System  │
    │  (20% MISSING)   │    │    │  (COMPLETE 95%)   │
    └─────────┬────────┘    │    └─────────┬─────────┘
              │             │              │
    ┌─────────▼────────┐   │    ┌─────────▼─────────┐
    │ Counter System   │   │    │   Flow State      │
    │ (15% MISSING)    │   │    │   (10% MISSING)   │
    └─────────┬────────┘   │    └───────────────────┘
              │            │
    ┌─────────▼────────────▼─────────┐
    │     AI Attack Tokens           │
    │     (30% IMPLEMENTED)          │
    └────────────────────────────────┘

BLOCKING RELATIONSHIPS:
→ Counter System BLOCKED BY Parry System
→ Flow State BLOCKED BY Finisher System (UNBLOCKED)
→ AI Tokens BLOCKED BY Parry + Counter Systems
```

---

## Critical Path Analysis

### Week 1: Foundations (P0 Issues)

```
Day 1        Day 2-3           Day 4-5
┌─────┐     ┌─────────┐       ┌─────────┐
│ Fix │  →  │  Parry  │   →   │ Counter │
│ I-  │     │ Detect  │       │ Window  │
│Face │     │ Logic   │       │ Tracking│
└──┬──┘     └────┬────┘       └────┬────┘
   │             │                  │
   └─────────────┴──────────────────┘
         BLOCKS ALL P1 WORK
```

**Must Complete Before Week 2**:
- ✅ Interface calls fixed (no crashes)
- ✅ Parry detection functional
- ✅ Counter window tracking implemented

---

### Week 2-3: Core Loop (P1 High Impact)

```
Week 2                      Week 3
┌──────────────┐           ┌──────────────┐
│ Day 1-2      │           │ Day 1-2      │
│ Counter      │           │ Flow State   │
│ Attack Exec  │           │ Management   │
├──────────────┤           ├──────────────┤
│ Day 3-4      │           │ Day 3-4      │
│ Attack Token │           │ VFX/SFX      │
│ Subsystem    │           │ Triggering   │
├──────────────┤           ├──────────────┤
│ Day 5        │           │ Day 5        │
│ Telegraph    │           │ Integration  │
│ Widget       │           │ Testing      │
└──────────────┘           └──────────────┘
```

---

## Combat Flow State Machine

### Current (Partial):

```
        ┌──────┐
        │ Idle │
        └───┬──┘
            │
    ┌───────▼──────────┐
    │   Attacking      │
    └───────┬──────────┘
            │
    ┌───────▼──────────┐
    │   Finishing      │  ← WORKS
    └───────┬──────────┘
            │
        ┌───▼───┐
        │ Dead  │
        └───────┘

MISSING: Blocking, Parrying, Counter Window, Flow State
```

### Target (Complete):

```
        ┌──────┐
        │ Idle │
        └───┬──┘
            │
    ┌───────┼──────────────────┐
    │       │                  │
┌───▼───┐  │  ┌────────────┐  │  ┌──────────┐
│Attack │  │  │  Blocking  │  └─▶│ Evading  │
└───┬───┘  │  └─────┬──────┘     └──────────┘
    │      │        │
    │  ┌───▼────────▼───┐
    │  │    Parrying    │  ← NEW
    │  └───────┬────────┘
    │          │
    │  ┌───────▼────────┐
    │  │ Counter Window │  ← NEW
    │  └───────┬────────┘
    │          │
┌───▼──────────▼─────┐
│     Finishing      │
└───────┬────────────┘
        │
┌───────▼────────┐
│  Flow State    │  ← NEW
│ (Auto-Finish)  │
└───────┬────────┘
        │
    ┌───▼───┐
    │ Dead  │
    └───────┘
```

---

## Feature Dependency Graph

```
                        ┌─────────────────┐
                        │  INPUT SYSTEM   │
                        │   (COMPLETE)    │
                        └────────┬────────┘
                                 │
                    ┌────────────┼────────────┐
                    │                         │
         ┌──────────▼─────────┐    ┌─────────▼────────┐
         │  COMBAT COMPONENT  │    │  TARGETING       │
         │    (COMPLETE)      │    │  (COMPLETE)      │
         └──────────┬─────────┘    └─────────┬────────┘
                    │                        │
    ┌───────────────┼───────────────┬────────┼────────┐
    │               │               │        │        │
┌───▼───┐  ┌────────▼────────┐  ┌──▼──┐  ┌──▼──┐  ┌─▼──┐
│ PARRY │  │  NORMAL ATTACK  │  │ HIT │  │DEATH│  │WARP│
│  20%  │  │   (COMPLETE)    │  │100% │  │100% │  │100%│
└───┬───┘  └─────────────────┘  └─────┘  └─────┘  └────┘
    │
┌───▼───────┐
│  COUNTER  │
│    15%    │
└───┬───────┘
    │
┌───▼───────┐       ┌──────────┐
│ FINISHER  │   →   │   FLOW   │
│   95%     │       │   10%    │
└───────────┘       └──────────┘
         │                 │
         └────────┬────────┘
                  │
         ┌────────▼────────┐
         │   VFX/SFX       │
         │     40%         │
         └─────────────────┘
```

**Legend**:
- Complete (>90%): Green boxes in actual implementation
- In Progress (40-90%): Yellow boxes
- Missing (<40%): Red boxes

---

## Implementation Priority Matrix

```
┌────────────────────────────────────────────────────────┐
│ HIGH IMPACT                                            │
│                                                        │
│  P0                     P1                             │
│  ┌──────────────┐      ┌──────────────┐              │
│  │ Interface    │      │ Flow State   │              │
│  │ Call Fix     │      │              │              │
│  │              │      │ Attack       │              │
│  │ Parry Logic  │      │ Tokens       │              │
│  │              │      │              │              │
│  │ Counter      │      │ Telegraph    │              │
│  │ Window       │      │              │              │
│  └──────────────┘      └──────────────┘              │
│   (Week 1)              (Week 2-3)                    │
├────────────────────────────────────────────────────────┤
│ LOW IMPACT                                             │
│                                                        │
│  P1                     P2                             │
│  ┌──────────────┐      ┌──────────────┐              │
│  │ VFX/SFX      │      │ Blueprint    │              │
│  │ Triggering   │      │ Cleanup      │              │
│  └──────────────┘      │              │              │
│                        │ AnimNotify   │              │
│                        │ Audit        │              │
│                        │              │              │
│                        │ IK/Proc      │              │
│                        └──────────────┘              │
│   (Week 3)              (Week 4+)                     │
└────────────────────────────────────────────────────────┘
      LOW COMPLEXITY          HIGH COMPLEXITY
```

---

## Testing Strategy Visualization

```
┌─────────────────────────────────────────────────────────┐
│                   TESTING PYRAMID                       │
├─────────────────────────────────────────────────────────┤
│                                                         │
│                     ▲ Manual                            │
│                    ╱ ╲                                  │
│                   ╱   ╲  VFX/SFX                        │
│                  ╱     ╲ Playtest                       │
│                 ╱───────╲                               │
│                ╱         ╲                              │
│               ╱  ▲ E2E   ╲ Integration                  │
│              ╱  ╱ ╲      ╲ Tests                        │
│             ╱  ╱   ╲     ╲                              │
│            ╱  ╱     ╲    ╲ Full Combat                  │
│           ╱  ╱───────╲   ╲ Loop                         │
│          ╱  ╱         ╲  ╲                              │
│         ╱  ╱  ▲ Unit  ╲  ╲                              │
│        ╱  ╱  ╱ ╲      ╲  ╲ Component                    │
│       ╱  ╱  ╱   ╲     ╲  ╲ Tests                        │
│      ╱  ╱  ╱     ╲    ╲  ╲                              │
│     ╱  ╱  ╱───────╲   ╲  ╲ Parry, Counter,              │
│    ╱  ╱  ╱         ╲  ╲  ╲ Flow State                   │
│   ╱  ╱  ╱           ╲ ╲  ╲                              │
│  ╱  ╱  ╱─────────────╲╲  ╲                              │
│ ╱__╱__╱_______________╲╲__╲                             │
│                                                         │
│ Current: 126 tests (Unit + E2E)                         │
│ Target:  180+ tests (Add Parry/Counter/Flow)           │
└─────────────────────────────────────────────────────────┘
```

---

## Risk Heat Map

```
┌────────────────────────────────────────────────────────┐
│  RISK ASSESSMENT: Likelihood vs Impact                 │
├────────────────────────────────────────────────────────┤
│                                                        │
│ HIGH     🔴 Interface    🟡 Flow State                 │
│ IMPACT      Crashes         Desync                     │
│                                                        │
│         🟡 Counter      🟢 VFX/SFX                     │
│ MEDIUM     Race Cond       Leaks                       │
│                                                        │
│         🟢 Token        🟢 Telegraph                   │
│ LOW        Deadlock        Too Easy                    │
│                                                        │
└────────────────────────────────────────────────────────┘
     HIGH           MEDIUM          LOW
          LIKELIHOOD OF OCCURRENCE

🔴 = Critical - Address Immediately
🟡 = High - Address Soon
🟢 = Medium - Monitor
```

---

## Milestone Timeline

```
Week 1          Week 2          Week 3          Week 4
┌────────────┐  ┌────────────┐  ┌────────────┐  ┌────────────┐
│ FOUNDATION │  │ CORE LOOP  │  │ FLOW + FX  │  │   POLISH   │
├────────────┤  ├────────────┤  ├────────────┤  ├────────────┤
│            │  │            │  │            │  │            │
│ ✓ Fix I-   │  │ • Counter  │  │ • Flow     │  │ • Blueprint│
│   Calls    │  │   Attacks  │  │   State    │  │   Cleanup  │
│            │  │            │  │            │  │            │
│ ✓ Parry    │  │ • Tokens   │  │ • VFX/SFX  │  │ • AnimNot. │
│   Detect   │  │            │  │            │  │   Audit    │
│            │  │ • Telegraph│  │ • Integrate│  │            │
│ ✓ Counter  │  │            │  │            │  │ • Profile  │
│   Window   │  │            │  │            │  │            │
└────────────┘  └────────────┘  └────────────┘  └────────────┘
    ↓               ↓               ↓               ↓
┌────────────┐  ┌────────────┐  ┌────────────┐  ┌────────────┐
│ MILESTONE  │  │ MILESTONE  │  │ MILESTONE  │  │ MILESTONE  │
│     1      │  │     2      │  │     3      │  │     4      │
├────────────┤  ├────────────┤  ├────────────┤  ├────────────┤
│ Can parry  │  │ Can counter│  │ Can chain  │  │ Code clean │
│ enemy      │  │ + AI won't │  │ finishers  │  │ + optimized│
│ attacks    │  │ spam       │  │ in flow    │  │            │
└────────────┘  └────────────┘  └────────────┘  └────────────┘

           DEMO READY ─────────────────────▶
```

---

## Success Metrics Dashboard

```
┌─────────────────────────────────────────────────────────┐
│              COMPLETION METRICS (Target)                │
├─────────────────────────────────────────────────────────┤
│                                                         │
│ Parry Success Rate:       [████████████] >95%          │
│ Counter Window Timing:    [████████████] 2.0s ±0.1s    │
│ Flow Chain Length:        [████████████] 3-5 finishers │
│ Attack Token Limit:       [████████████] ≤3 enemies    │
│ Telegraph Visibility:     [████████████] >80%          │
│ Test Coverage:            [██████████  ] >80%          │
│ Frame Time:               [████████████] <1ms/char     │
│ Crash Rate:               [████████████] 0             │
│                                                         │
├─────────────────────────────────────────────────────────┤
│              PLAYER EXPERIENCE (Qualitative)            │
├─────────────────────────────────────────────────────────┤
│                                                         │
│ [ ] Parry timing feels fair and rewarding              │
│ [ ] Counter attacks feel powerful (1.5x)               │
│ [ ] Flow state creates "power fantasy"                 │
│ [ ] Enemy attacks are readable                         │
│ [ ] Combat flow feels like Batman Arkham               │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

---

## Quick Status Reference

| System | Week 1 | Week 2 | Week 3 | Week 4 | Final |
|--------|--------|--------|--------|--------|-------|
| **Interface Calls** | 🔴→🟢 | ✅ | ✅ | ✅ | ✅ |
| **Parry Detection** | 🔴→🟢 | ✅ | ✅ | ✅ | ✅ |
| **Counter Window** | 🔴→🟢 | ✅ | ✅ | ✅ | ✅ |
| **Counter Attacks** | 🔴 | 🔴→🟢 | ✅ | ✅ | ✅ |
| **Attack Tokens** | 🔴 | 🔴→🟢 | ✅ | ✅ | ✅ |
| **Telegraph** | 🔴 | 🔴→🟢 | ✅ | ✅ | ✅ |
| **Flow State** | 🔴 | 🔴 | 🔴→🟢 | ✅ | ✅ |
| **VFX/SFX** | 🟡 | 🟡 | 🟡→🟢 | ✅ | ✅ |
| **Polish** | 🟡 | 🟡 | 🟡 | 🟡→🟢 | ✅ |

**Legend**: 🔴 Blocked | 🟡 In Progress | 🟢 Complete | ✅ Verified

---

## Next Steps

### Today (Day 1)
1. ✅ Review audit documents
2. ⬜ Begin interface call pattern fix
3. ⬜ Create branch: `feature/parry-counter-flow`
4. ⬜ Setup test infrastructure for new systems

### This Week (Days 2-5)
1. ⬜ Complete interface call fix
2. ⬜ Implement parry detection logic
3. ⬜ Implement counter window tracking
4. ⬜ Write unit tests

### Next Week (Week 2)
1. ⬜ Counter attack execution
2. ⬜ Attack token subsystem
3. ⬜ Telegraph widget
4. ⬜ Integration testing

---

**For detailed implementation instructions, see**:
- `docs/AUDIT_ACTION_CHECKLIST.md` - Step-by-step tasks
- `docs/COMPREHENSIVE_AUDIT_2026-02-03.md` - Full analysis
- `docs/AUDIT_EXECUTIVE_SUMMARY.md` - Quick overview

**Last Updated**: 2026-02-03
