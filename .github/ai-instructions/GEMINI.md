# KatanaCombat Project Context for Gemini

**Project Name:** KatanaCombat
**Engine:** Unreal Engine 5.6
**Language:** C++ / Blueprint Hybrid
**System:** Windows (win32)

---

## 1. Project Identity & Core Philosophy

KatanaCombat is a high-fidelity melee combat system inspired by *Ghost of Tsushima* and *Sekiro*. It prioritizes **functional consolidation** over fragmented architecture.

### 🧠 Critical Design Mandates
*   **Consolidation:** The `UCombatComponent` is the brain. It is intentionally dense (~1000 lines). Do *not* fragment its logic into tiny sub-components unless absolutely necessary.
*   **Hybrid Combos:** The system supports two paths simultaneously:
    *   **Responsive:** Input buffered → Wait for recovery → Execute.
    *   **Snappy:** Input buffered → Combo Window Open → Cancel Recovery → Execute immediately.
*   **Defensive Depth:** Defense is based on **Posture**, not just health.
*   **Data-Driven:** All attack properties (timing, damage, warping) live in `UAttackData` assets. Do *not* hardcode values in C++.

### ⚠️ Architectural "Laws" (Do Not Break)
1.  **Phases vs. Windows:**
    *   **Phases** (`Windup`, `Active`, `Recovery`) are **Exclusive**. Only one is active at a time.
    *   **Windows** (`Parry`, `Combo`, `Hold`, `Cancel`) are **Independent**. They are boolean flags that can overlap with phases.
    *   *Anti-Pattern:* Treating a "Hold Window" as a separate "Hold Phase".
2.  **Input Buffering:**
    *   Input is **ALWAYS** buffered during an attack.
    *   The `ComboWindow` determines **WHEN** it executes, not **WHETHER** it buffers.
    *   *Anti-Pattern:* Gating input registration behind `bCanCombo`.
3.  **Parry Logic:**
    *   Parry is a **Defender-Side** check.
    *   The Defender checks `Enemy->IsInParryWindow()`.
    *   The `ParryWindow` is defined on the **Attacker's** montage.
    *   *Anti-Pattern:* Putting the Parry Window on the defender's block animation.

---

## 2. Codebase Map

### Core Systems (`Source/KatanaCombat/`)
*   **`Core/CombatComponent.h`**: The central brain. State machine, input buffering, combo logic.
*   **`Core/TargetingComponent.h`**: Directional targeting, cone filtering, motion warp setup.
*   **`Core/WeaponComponent.h`**: Socket-based swept sphere tracing, hit detection.
*   **`Core/HitReactionComponent.h`**: Damage processing, hitstun, reaction montages.
*   **`Data/AttackData.h`**: Primary Data Asset defining attacks.
*   **`Interfaces/CombatInterface.h`**: Communication interface between actors.

### Configuration (`Config/`)
*   **`DefaultInput.ini`**: Enhanced Input mappings (`IA_LightAttack`, `IA_Block`, etc.).
*   **`DefaultEngine.ini`**: Collision profiles and rendering settings.

### Testing (`Source/KatanaCombatTest/`)
*   **`Private/`**: Contains unit tests (`StateTransitionTests.cpp`, `InputBufferingTests.cpp`).
*   **`CombatTestHelpers.h`**: Utilities for spawning test worlds and characters.

### AI Context (`.claude/`)
This directory contains the "Meta" of the project.
*   **`context-modes/`**: Definitions for filtered views (Animation, Logic, UI).
*   **`agents/`**: Personas for code generation (`ue-code-generator`) and auditing (`code-auditor`).
*   **`diagnostics/`**: Configuration for linter warnings and false positives.

---

## 3. Operational Context Modes

To work efficiently, adopt one of the following "Modes" based on the task.

| Mode | Focus Area | Relevant Files | Key Mental Model |
| :--- | :--- | :--- | :--- |
| **`combat-logic`** | Core Mechanics | `CombatComponent`, `CombatTypes`, `Interfaces` | Focus on State Machine, Input Buffering, and C++ logic flow. |
| **`animation`** | Visuals & Timing | `AnimInstance`, `AnimNotify`, `Montages` | Focus on `AnimNotify_AttackPhaseTransition` checkpoints (implicit phase inference), blending, and slot management. |
| **`data-assets`** | Content Design | `AttackData`, `CombatSettings` | Focus on properties, UPROPERTY settings, and data validation. |
| **`testing`** | QA & Stability | `KatanaCombatTest`, `Automation` | Focus on writing assertions, ensuring coverage, and regression testing. |
| **`full`** | Architecture | All Files | Use for cross-cutting refactors or understanding system dependencies. |

---

## 4. Development Workflows

### A. Implementing a New Feature (e.g., "Dodge Roll")
1.  **Clarify:** Define the mechanic. Is it a State? An Action? Does it need Data?
2.  **Context:** Switch to `combat-logic` mode.
3.  **Data:** Create necessary properties in `UCombatSettings` (e.g., `EvadeDistance`).
4.  **Input:** Add `IA_Evade` to `DefaultInput.ini` and bind in Character.
5.  **Logic:** Implement `OnEvadePressed` in `CombatComponent`. Update State Machine to allow transition to `Evading`.
6.  **Animation:** Create `AM_Evade` and add Notifies (e.g., `AnimNotify_FinishEvade`).
7.  **Verify:** Run `StateTransitionTests` to ensure Evade doesn't break existing flows.

### B. Creating a New Attack
1.  **Context:** Switch to `data-assets` and `animation` mode.
2.  **Asset:** Create `UAttackData` (e.g., `DA_Heavy_Overhead`).
3.  **Montage:** Setup sections and `AnimNotify_AttackPhaseTransition` checkpoints (phases inferred implicitly between checkpoints).
4.  **Link:** Assign to `CombatComponent` or chain from an existing attack.

### C. Fixing a Bug (e.g., "Parry not triggering")
1.  **Diagnose:** Check the *Defender's* logic in `TryParry()`.
2.  **Check Data:** Does the *Attacker's* montage have a `ParryWindow` notify?
3.  **Check Timing:** Is the window active when the hit occurs?
4.  **Test:** Create a reproduction test case in `ParryDetectionTests.cpp`.

---

## 5. Automated Validation

Before marking a task as complete, perform these checks:

### 1. Run Diagnostics
Check for critical warnings:
```bash
/check-warnings
```
*Note: Ignore "Blueprint-exposed" unused variable warnings in `CombatComponent`.*

### 2. Run Automation Tests
Execute the project's test suite:
```bash
UnrealEditor.exe "KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat"
```

### 3. Architecture Audit
Ask yourself:
*   Did I add a hardcoded magic number? (Move to `CombatSettings`)
*   Did I add logic to `Character` that belongs in `CombatComponent`?
*   Did I mix up a Phase and a Window?

---

## 6. Troubleshooting & Common Issues

*   **"Input ignored during attack":** Check `InputBufferingTests`. Ensure you aren't checking `bCanCombo` before setting the `bBuffered` flag. Input must *always* buffer.
*   **"Animation stuck":** Check `AnimNotify_AttackPhaseEnd`. If the montage is interrupted or blends out early, the notify might be skipped. Ensure `OnMontageEnded` handles cleanup.
*   **"Hit not registering":** Check `WeaponComponent`. Is `EnableHitDetection` called? Is the trace channel correct (`ECC_Pawn`)?
*   **"Crash on Combo":** Check `AttackData` references. Ensure `NextComboAttack` is not pointing to a deleted asset or creating a circular dependency (though `AttackData` validation logic should catch cycles).

---

**You are now initialized with the full context of the KatanaCombat project.**
Use the **Modes** to focus your attention.
Respect the **Architectural Laws**.
Validate with **Tests**.