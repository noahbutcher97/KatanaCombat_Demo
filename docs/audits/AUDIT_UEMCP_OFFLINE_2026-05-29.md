# Asset, Config & Data-Integrity Audit (UEMCP Offline)

> **Date**: 2026-05-29
> **Scope**: Project configuration, content/asset integrity, data-driven combat config graph, naming conventions, repository/cook bloat — the *content & config* surface, complementary to the code-focused audits of 2026-02-03.
> **Method**: UEMCP **offline toolset** driven against the project on disk (no Unreal Editor, no live plugin). Binary `.uasset`/`.umap` header + property parsing, `AssetRegistry` bulk scan, and `.ini` config parsing.
> **Tooling**: [`uemcp`](https://github.com/noahbutcher97/uemcp) `server/offline-tools.mjs` → `executeOfflineTool(tool, params, UNREAL_PROJECT_ROOT)`. Tools used: `project_info`, `get_build_config`, `query_asset_registry`, `read_asset_properties`, `inspect_blueprint`, `list_level_actors`, `list_gameplay_tags`, `list_config_values`. See [Appendix A](#appendix-a--reproduction) to re-run.

---

## Executive Summary

| Metric | Value |
|--------|-------|
| **Engine** | UE 5.6 |
| **C++ modules** | 3 (`KatanaCombat` runtime, `KatanaCombatEditor`, `KatanaCombatTest`) |
| **Test files** | 24 automation-test `.cpp` files |
| **Total Content size** | ~2.2 GB |
| **Project-authored Content** | ~2.7 MB / 85 `.uasset` (under `Content/ProjectFiles/`) |
| **Marketplace/imported Content** | ~1.04 GB+ / ~1,967 assets + 32 demo maps + ~1,888 anim sequences |
| **Critical findings (P0)** | 2 (broken default map/gamemode config; dangling default-attack references) |
| **Medium findings (P1)** | 1 (repo/cook bloat) |
| **Low findings (P2)** | 5 (naming drift, stale redirectors, cross-pack mesh ref, unused C++ template residue) |

### Top actions

1. **Fix `DefaultEngine.ini`** — `Lvl_ThirdPerson1` / `GM_Samurai1` reference assets that don't exist (stray `1` suffix). A cooked build fails to load its startup map. *(P0-1, safe one-line fix.)*
2. **Repoint or recreate `LightAttack` / `HeavyAttack`** — `DA_Config_Katana`'s default attacks (and the combo chain's `NextComboAttack`) import packages that don't exist on disk. *(P0-2, needs editor.)*
3. **Prune unused marketplace bundles** — ~1 GB of anim packs + 32 demo maps ship in-tree and dwarf the 2.7 MB actual game. *(P1-1.)*

### Relationship to prior audits

The 2026-02-03 audits (`AUDIT_SYNTHESIS_2026-02-03.md` et al.) covered **C++ code health** (null-deref crash vectors, interface-call patterns, file decomposition). This audit covers the **content/config layer** those did not reach — it reads the actual serialized `.uasset` data, the `AssetRegistry`, and `Config/*.ini`. The two are non-overlapping; findings here are new.

---

## Part 1 — Project Shape (informational)

Confirmed via `project_info` + `get_build_config`:

- **Runtime module deps** (`KatanaCombat.Build.cs`): Core/CoreUObject/Engine, EnhancedInput, AIModule, **StateTree + GameplayStateTree**, GameplayTags, UMG/Slate, **MotionWarping**, Niagara.
- **Enabled plugins**: StateTree, GameplayStateTree, MotionWarping, AnimationWarping, AnimationLocomotionLibrary, MotionTrajectory, PoseSearch, ModelingToolsEditorMode, BlueprintFileUtils.
- **Architecture** (cross-checked against `Source/`): data-driven, component-based combat —
  - Components: `CombatComponent`, `WeaponComponent`, `TargetingComponent`, `HitReactionComponent`.
  - Data assets (`UPrimaryDataAsset`): `AttackData`, `AttackConfiguration`, `WeaponData`, `HitReactionData`/`HitReactionSettings`, `CombatSettings`, `TargetingSettings`, `MotionWarpingSettings`.
  - Anim-driven windows: `AnimNotifyState_{AttackPhase,ComboWindow,ParryWindow,CounterWindow,HoldWindow,...}` + `AnimNotify_{ToggleHitDetection,ActivateRagdoll,...}`.
  - AI: `CombatTokenSubsystem`, `EnemyCombatAIComponent`, BT tasks (`BTTask_{ApproachPlayer,CirclePlayer,ExecuteAttack,RequestAttackToken}`).
- **Gameplay tags** (`list_gameplay_tags`): 18 tags, all commented and coherent (`Attack.Capability.*`, `Attack.Type.*`, `Context.*`, `Style.*`).
- **The data-driven config graph decodes intact**: `DA_CombatSettings → {DA_Targeting, DA_MotionWarping, DA_Config_Katana, DA_HitReaction}`; `DA_Weapon_Katana → DA_Config_Katana`; `AttackData` chains via `NextComboAttack` / `HeavyComboAttack` / `DirectionalFollowUps[5]`; `HitReactionSettings → DirectionalReactions` by intensity/direction. The *design* is sound — the defects below are reference/naming hygiene, not structural.

---

## Part 2 — Findings

### P0 — CRITICAL

#### P0-1 — Default map and game mode point to non-existent assets

`Config/DefaultEngine.ini` references packages with a stray `1` suffix that do not exist on disk:

```ini
GameDefaultMap=/Game/ProjectFiles/Levels/Lvl_ThirdPerson1.Lvl_ThirdPerson1
EditorStartupMap=/Game/ProjectFiles/Levels/Lvl_ThirdPerson1.Lvl_ThirdPerson1
GlobalDefaultGameMode=/Game/ProjectFiles/Core/GameModes/GM_Samurai1.GM_Samurai1_C
```

**Evidence**
- On disk: `Content/ProjectFiles/Levels/` contains `Lvl_ThirdPerson.umap` and `Lvl_Arena.umap` — there is **no** `Lvl_ThirdPerson1`.
- On disk: `Content/ProjectFiles/Core/GameModes/` contains `GM_Samurai.uasset` — there is **no** `GM_Samurai1`.
- Filesystem search for `Lvl_ThirdPerson1` / `GM_Samurai1`: **0 files**.
- The existing redirector for the moved game mode resolves to `/Game/ProjectFiles/Core/GameModes/GM_Samurai` (no `1`), confirming the intended name.

**Impact**: a cooked/packaged build cannot resolve its startup map or global game mode; the editor's startup map fails to open. High severity, definite.

**Fix** (safe, surgical): drop the `1` from all three lines:
```ini
GameDefaultMap=/Game/ProjectFiles/Levels/Lvl_ThirdPerson.Lvl_ThirdPerson
EditorStartupMap=/Game/ProjectFiles/Levels/Lvl_ThirdPerson.Lvl_ThirdPerson
GlobalDefaultGameMode=/Game/ProjectFiles/Core/GameModes/GM_Samurai.GM_Samurai_C
```
*(Confirm the intended default level is `Lvl_ThirdPerson` vs `Lvl_Arena` before committing.)*

---

#### P0-2 — Katana default attacks are dangling references

`DA_Config_Katana` (class `AttackConfiguration`) imports default-attack packages that do not exist:

| Property | Imported package | Exists on disk? |
|----------|------------------|-----------------|
| `DefaultLightAttack` | `/Game/ProjectFiles/Data/PDA/Attack/AttackData/Light/LightAttack` | ❌ no |
| `DefaultHeavyAttack` | `/Game/ProjectFiles/Data/PDA/Attack/AttackData/Heavy/HeavyAttack` | ❌ no |

The combo chain re-references the same missing base (`LightAttack_2.NextComboAttack → LightAttack.LightAttack`).

**Evidence**
- `AssetRegistry` scan of `/Game/ProjectFiles/Data/PDA/Attack` lists `LightAttack_1…11` and `HeavyAttack_1…4` — **no** un-suffixed `LightAttack` / `HeavyAttack` package.
- Filesystem search for `Light/LightAttack.uasset` and `Heavy/HeavyAttack.uasset`: **0 files**.
- No `ObjectRedirector` exists for either name (project has only 4 redirectors; none for these).
- These are entries in `DA_Config_Katana`'s **import table** (hard references), read via `read_asset_properties`.

**Likely cause**: the base attack assets were renamed to `LightAttack_1` / `HeavyAttack_1` (or deleted) without fixing up references in `DA_Config_Katana` and the combo chain. Note also the serialized object name inside each `LightAttack_N.uasset` reads as `LightAttack` while its `AssetRegistry` name is `LightAttack_N` — a classic duplicate-then-rename artifact.

**Impact**: the weapon's configured default light/heavy attack won't resolve at runtime. Confidence high from disk + registry; **verify in-editor** before repointing (open `DA_Config_Katana`, set `DefaultLightAttack`/`DefaultHeavyAttack` to the intended `LightAttack_1`/`HeavyAttack_1`, then re-save and Fix Up Redirectors).

---

### P1 — MEDIUM

#### P1-1 — Repository & cook bloat: ~1 GB of unused marketplace content

| Area | Size | Assets |
|------|------|--------|
| `Content/Assets/Animations/CombatMasterAnimBundle` | 576 MB | 1,244 |
| `Content/Assets/Animations/ARPG_Samurai` | 323 MB | 723 |
| `Content/Assets/Animations/DynamicKatana` | 141 MB | — |
| Demo `.umap`s inside those bundles | — | 32 |
| **Actual game** (`Content/ProjectFiles/`) | **2.7 MB** | **85** |
| **Total `Content/`** | **~2.2 GB** | — |

The imported anim packs (and their 32 demo maps) ship in-tree and overwhelmingly outweigh the project's own content. They inflate clone size, cook time, and package size.

**Recommendation**: identify the specific animations the game actually references (editor *Reference Viewer* / *Size Map*, or a packaged-build reference scan), then move the rest under a `Developers/` or otherwise cook-excluded path, or delete. **Offline tooling cannot prove zero references** — confirm before culling.

---

### P2 — LOW (hygiene)

| ID | Finding | Evidence | Fix |
|----|---------|----------|-----|
| **P2-1** | `DirectionalAttack_R` missing the `DA_` prefix its 4 siblings use (`DA_DirectionalAttack_{L,F,B,None}`) — and it's a *live* reference inside `LightAttack_*.DirectionalFollowUps`. | `AssetRegistry` path scan | Rename to `DA_DirectionalAttack_R`, fix up redirectors. |
| **P2-2** | `AttackData` instances (`LightAttack_1…11`, `HeavyAttack_1…4`, `Finisher_*`) don't follow the `DA_` convention used by sibling PrimaryDataAssets; serialized object names (`LightAttack`) also don't match registry names (`LightAttack_2`). | registry vs `read_asset_properties` | Decide one convention; rename consistently. Resolves the confusion underlying P0-2. |
| **P2-3** | 4 unresolved `ObjectRedirector`s (`GM_Samurai`, `Lvl_ThirdPerson`, 2× `SK_Mannequin`). | `query_asset_registry class_name=ObjectRedirector` | Run *Fix Up Redirectors in Folder*. |
| **P2-4** | `DA_Weapon_Katana.WeaponMesh` sourced from an unrelated pack folder: `/Game/Assets/Characters/CyberpunkRunner/Meshes/SKM_Katana_5`. | `read_asset_properties` | Move the katana mesh under a weapon/project folder. Organizational only. |
| **P2-5** | Unused C++ template residue: full Epic `Variant_Combat`/`Variant_Platforming`/`Variant_SideScrolling` trees present; `GM_Samurai` derives from `GameModeBase`, not the C++ `KatanaCombatGameMode`/`CombatGameMode`. | `inspect_blueprint`, `Source/` tree | Confirm unused, then remove to reduce surface area. |

---

## Part 3 — Notes & caveats

- **Levels are NOT empty.** `list_level_actors` reports only 2 placed actors in `Lvl_ThirdPerson.umap` because the project uses **One File Per Actor (OFPA)** — actors live in `Content/__ExternalActors__/` (70 for `Lvl_ThirdPerson`, 54 for `Lvl_Arena`). The offline `.umap` parse does not resolve OFPA external actors; use the editor for placed-actor inventory.
- **BP ↔ C++ wiring verified** where checked: `BP_EnemyCharacter` → C++ `EnemyCharacter`; `BP_SamuraiCharacter` is the player BP.
- Offline tooling reads **on-disk serialized state**. It cannot evaluate runtime/PIE behavior, compiled Blueprint bytecode semantics, or live reflection. Pair with the editor (or UEMCP's TCP:55558 / HTTP:30010 live layers) for those.

---

## Appendix A — Reproduction

This audit is fully reproducible offline, no editor required.

```bash
# 1. Clone both repos side by side
git clone https://github.com/noahbutcher97/uemcp.git
git clone https://github.com/noahbutcher97/katanacombat_demo.git

# 2. Install the UEMCP server deps
cd uemcp/server && npm install

# 3. Drive the offline tools against the project root
export UNREAL_PROJECT_ROOT=/abs/path/to/katanacombat_demo
node -e '
  import("./offline-tools.mjs").then(async ({ executeOfflineTool }) => {
    const ROOT = process.env.UNREAL_PROJECT_ROOT;
    const call = (t, p={}) => executeOfflineTool(t, p, ROOT);
    console.log(JSON.stringify(await call("project_info"), null, 2));
    console.log(JSON.stringify(await call("query_asset_registry",
      { path_prefix: "/Game/ProjectFiles/Data/PDA/Attack", limit: 200 }), null, 2));
    console.log(JSON.stringify(await call("read_asset_properties",
      { asset_path: "Content/ProjectFiles/Data/PDA/Attack/AttackConfigurations/DA_Config_Katana.uasset" }), null, 2));
  });
'
```

Key offline tools and what they surfaced:

| Tool | Used for |
|------|----------|
| `project_info` | modules, engine version, enabled plugins |
| `get_build_config` | `*.Build.cs` / `*.Target.cs` module deps |
| `query_asset_registry` | class histogram, path scans, redirector enumeration |
| `read_asset_properties` | decoded CDO values of DataAssets (the config graph + P0-2) |
| `inspect_blueprint` | BP parent-class / generated-class wiring |
| `list_level_actors` | level export tables (+ OFPA caveat) |
| `list_gameplay_tags` | tag hierarchy + comments |
| `list_config_values` / `Config/*.ini` | default map / game mode (P0-1) |

Config references (P0-1) were cross-checked directly against `Config/DefaultEngine.ini` and the filesystem; dangling attack imports (P0-2) against `AssetRegistry` + filesystem + redirector enumeration.
