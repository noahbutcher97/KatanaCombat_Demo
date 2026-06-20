# KatanaCombat Bug Evidence Lanes

## Build Failure

Start from the first compiler error. Inspect the named `.h`, `.cpp`, `.Build.cs`, or `.Target.cs` file before searching broadly. Build failures in editor-only code should not pull editor dependencies into `Source/KatanaCombat/`.

## Automation Failure

Use the test path to route:

- `KatanaCombat.CombatComponent.*`: state, input, phase/window, attack execution.
- `KatanaCombat.Targeting.*`: soft lock, filtering, direction transforms.
- `KatanaCombat.Weapon.*`: hit traces, socket config, equip state.
- `KatanaCombat.HitReaction.*` or `DeathSystem.*`: damage, death, hitstun, i-frames.
- `KatanaCombat.Integration.*`: cross-component wiring and team filtering.

## Runtime Or Editor Failure

Prefer the latest relevant log under `Saved/Logs/`. For PIE/editor behavior, use UEMCP if available; otherwise use an explicit Editor reproduction and record the map, actor, asset, command, and observed result.

## Asset Wiring Failure

Identify the exact asset path first. Do not infer asset state from C++ alone. Check related data-asset classes and defaults, then verify in Editor/UEMCP.

## Crash

Start from the stack frame nearest `Source/KatanaCombat` or `Source/KatanaCombatEditor`. Look for null object use, invalid array index, destroyed actor/component access, and BlueprintNativeEvent direct calls that should use `Execute_`.
