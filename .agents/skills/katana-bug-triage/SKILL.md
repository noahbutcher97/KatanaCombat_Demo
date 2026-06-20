---
name: katana-bug-triage
description: Diagnose KatanaCombat bugs from evidence before fixes. Use when the user reports crashes, failing tests, broken combat behavior, animation timing issues, parry/combo/hold regressions, asset wiring problems, or unclear Unreal Editor/runtime failures.
---

# Katana Bug Triage

## Evidence-First Workflow

1. Capture the exact symptom, expected behavior, and reproduction path.
2. Collect direct evidence: compiler output, automation log, crash stack, PIE/editor log, UEMCP response, asset name, or test failure.
3. Map the symptom to the smallest likely system using `references/evidence-lanes.md`.
4. Inspect the responsible files and nearby tests before proposing a fix.
5. State the hypothesis and the falsifying check.
6. Implement only after the evidence points to a concrete cause.
7. Verify with the narrowest regression test or runtime check, then run adjacent checks if the fix touches shared behavior.

## Common Design-Trap Checks

- Combo bugs: confirm input capture is not gated by combo windows.
- Parry bugs: confirm defender logic checks the attacker's parry window.
- Hold bugs: confirm button state is checked at the window boundary rather than tracked by duration.
- Phase/window bugs: confirm phases are mutually exclusive and windows can overlap.
- Delegate bugs: confirm cross-component delegates live in `CombatTypes.h`.

## Fix Discipline

Do not broaden from diagnosis into cleanup. Keep unrelated assets and generated files untouched. If the bug requires asset or Blueprint changes, require live Editor/UEMCP evidence before calling it fixed.

## Reporting

Report root cause, changed files, regression coverage, verification command results, and any unverified runtime surface.
