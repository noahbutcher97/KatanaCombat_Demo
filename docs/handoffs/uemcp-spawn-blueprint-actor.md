# Handoff: `spawn_blueprint_actor` fails on non-`/Game/Blueprints/` content layouts

**To:** uemcp main orchestration agent
**From:** Claude Code session, KatanaCombat repro (2026-04-28)
**Severity:** Blocking — every project that doesn't keep its Blueprints under `/Game/Blueprints/` cannot use `spawn_blueprint_actor` or any tool in the `blueprints-write` toolset.
**Plugin tree under test:** `Plugins/UEMCP/` (vendored locally in `D:/UnrealProjects/5.6/KatanaCombat/`).

---

## 1. Reproduction (this session, against editor on TCP:55558)

Asset under test:

- **Path:** `/Game/ProjectFiles/Core/Actors/Character/BP_EnemyCharacter`
- **Class:** `BP_EnemyCharacter_C` (parent: `APlayerCharacter`-line C++)
- **In live level:** `/Game/ProjectFiles/Levels/Lvl_ThirdPerson1`, 4 OFPA-stored instances at `Z=298`.

Call (every parameter form fails the same way):

```jsonc
spawn_blueprint_actor({
  "blueprint_name": "BP_EnemyCharacter",
  "actor_name":     "BP_EnemyCharacter_New",
  "location":       [197.38, 150, 298],
  "rotation":       [0, 180, 0]
})
```

Response:

```
Error in spawn_blueprint_actor: tcp-55558:
  Blueprint 'BP_EnemyCharacter' not found under /Game/Blueprints/
```

The asset is loadable by every other handler I tried in this session — `inspect_blueprint`, `bp_list_graphs`, `bp_find_in_graph`, `bp_show_node`, `bp_neighbors`, `bp_trace_exec`, `find_actors`, `get_actors`. The failure is specific to handlers that hard-code the `/Game/Blueprints/` prefix.

---

## 2. Root cause

`Plugins/UEMCP/Source/UEMCP/Private/ActorHandlers.cpp:630-637`:

```cpp
const FString AssetPath = FString::Printf(TEXT("/Game/Blueprints/%s"), *BlueprintName);
if (!FPackageName::DoesPackageExist(AssetPath))
{
    BuildErrorResponse(OutResponse,
        FString::Printf(TEXT("Blueprint '%s' not found under /Game/Blueprints/"), *BlueprintName),
        TEXT("BLUEPRINT_NOT_FOUND"));
    return;
}
```

Three problems:

1. **Hardcoded root.** `/Game/Blueprints/` is the only path searched.
2. **Filesystem probe, not registry lookup.** `FPackageName::DoesPackageExist` only checks the literal disk path. Even though `FAssetRegistryModule` knows where `BP_EnemyCharacter` actually lives, it's never consulted.
3. **No support for fully-qualified `/Game/...` paths.** Passing the full path makes things worse — the printf produces `/Game/Blueprints//Game/ProjectFiles/Core/Actors/Character/BP_EnemyCharacter`, which is also a "not found" miss.

The same `/Game/Blueprints/` assumption is documented in the user-facing tip text for the `actors` and `blueprints-write` toolsets:

> *"spawn_blueprint_actor looks up blueprints under /Game/Blueprints/ only — pass just the asset name, not a full path."*
> *"All blueprint commands use name-only lookup under /Game/Blueprints/ — pass 'MyBP', not '/Game/Blueprints/MyBP'."*

So this isn't a one-handler oversight — it's a plugin-wide convention.

---

## 3. Blast radius

Every BP-addressing handler in `blueprints-write` and `actors` is presumed affected. Recommend the receiving agent run:

```
grep -rn '/Game/Blueprints/' Plugins/UEMCP/Source/UEMCP/Private/
```

Tools likely sharing the bug (from `find_tools` output and the toolset tip):

- `actors`: `spawn_blueprint_actor` (confirmed)
- `blueprints-write`: `create_blueprint`, `add_component`, `set_component_property`, `set_blueprint_property`, `compile_blueprint`, `add_function_node`, `add_event_node`, `connect_nodes`, `add_variable`, `add_self_reference`, `add_component_reference`, `find_nodes`, `set_pawn_props`, `set_physics_props`, `set_static_mesh_props`

NOT affected (different code paths, take asset paths or full UObject paths directly):

- All `offline` toolset tools (read .uasset directly from disk)
- `remote-control` toolset (takes object paths)
- `actors` toolset's `spawn_actor` (primitive — by class name, not asset)
- `find_actors`, `get_actors`, `set_actor_transform`, `delete_actor` (work on already-spawned instances)

---

## 4. Proposed fix

Centralize the resolution into one helper, called by every affected handler. Suggested location: `Plugins/UEMCP/Source/UEMCP/Private/ActorLookupHelper.cpp` (or new `BlueprintLookupHelper.cpp` next to it — the existing helper already covers actor lookups, so colocating BP lookups there is a natural fit).

```cpp
// Resolves a Blueprint asset by either:
//   1. fully-qualified /Game/... path (preferred — unambiguous)
//   2. legacy bare-name lookup at /Game/Blueprints/<Name> (back-compat)
//   3. AssetRegistry fallback by ObjectName, project-wide
//
// On ambiguity (case 3 with multiple hits), returns an explicit error
// with all matches listed — never silently picks one.
static bool ResolveBlueprintAssetPath(
    const FString& Input,
    FString& OutPackagePath,
    FString& OutError)
{
    // Case 1: caller passed a fully-qualified package path
    if (Input.StartsWith(TEXT("/Game/")))
    {
        if (FPackageName::DoesPackageExist(Input))
        {
            OutPackagePath = Input;
            return true;
        }
        OutError = FString::Printf(TEXT("Blueprint package not found: %s"), *Input);
        return false;
    }

    // Case 2: bare name — back-compat /Game/Blueprints/<Name>
    const FString LegacyPath = FString::Printf(TEXT("/Game/Blueprints/%s"), *Input);
    if (FPackageName::DoesPackageExist(LegacyPath))
    {
        OutPackagePath = LegacyPath;
        return true;
    }

    // Case 3: AssetRegistry fallback by ObjectName, anywhere under /Game/
    FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    IAssetRegistry& AR = ARM.Get();

    TArray<FAssetData> Hits;
    AR.GetAssetsByClass(UBlueprint::StaticClass()->GetClassPathName(), Hits, /*bSearchSubClasses=*/ true);

    TArray<FString> Matches;
    for (const FAssetData& Hit : Hits)
    {
        if (Hit.AssetName.ToString() == Input)
        {
            Matches.Add(Hit.GetSoftObjectPath().GetLongPackageName());
        }
    }

    if (Matches.Num() == 1)
    {
        OutPackagePath = Matches[0];
        return true;
    }
    if (Matches.Num() > 1)
    {
        OutError = FString::Printf(
            TEXT("Ambiguous Blueprint name '%s' (%d matches: %s) — pass a fully-qualified /Game/... path to disambiguate"),
            *Input, Matches.Num(), *FString::Join(Matches, TEXT(", ")));
        return false;
    }

    OutError = FString::Printf(
        TEXT("Blueprint '%s' not found (checked %s, then AssetRegistry project-wide)"),
        *Input, *LegacyPath);
    return false;
}
```

Then `HandleSpawnBlueprintActor` (and every sibling handler) becomes:

```cpp
FString AssetPath, ResolveError;
if (!ResolveBlueprintAssetPath(BlueprintName, AssetPath, ResolveError))
{
    BuildErrorResponse(OutResponse, ResolveError, TEXT("BLUEPRINT_NOT_FOUND"));
    return;
}
UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
// ... existing logic unchanged from line 638 onward
```

---

## 5. Compatibility

| Caller form | Before fix | After fix |
|---|---|---|
| `MyBP` (asset is at `/Game/Blueprints/MyBP`) | ✅ works | ✅ works (Case 2) |
| `MyBP` (asset is at `/Game/ProjectFiles/.../MyBP`) | ❌ fails | ✅ works (Case 3 via AR) |
| `/Game/ProjectFiles/.../MyBP` (full path) | ❌ fails (path mangling) | ✅ works (Case 1) |
| `MyBP` exists in two folders | n/a | ❌ explicit ambiguity error listing both — by design |
| `BP_NonExistent` (typo) | ❌ "/Game/Blueprints/ only" message | ❌ "checked X, then AR project-wide" — clearer |

No back-compat break. Existing test suites should pass unchanged.

---

## 6. Tip-text updates after fix lands

Search and remove `/Game/Blueprints/`-only language:

- `MCPCommandRegistry.cpp` — wherever `actors` toolset tip is registered.
- Same for `blueprints-write` toolset tip.

Replace with something like:

> *"spawn_blueprint_actor accepts a fully-qualified `/Game/...` path (preferred) or a bare asset name (resolved via AssetRegistry; ambiguous bare names error explicitly)."*

---

## 7. Validation plan

After patch, run on KatanaCombat (which is set up to repro both layouts):

```
# Should now succeed — bare name, found via AR fallback
spawn_blueprint_actor(blueprint_name="BP_EnemyCharacter", actor_name="V1", location=[0,0,300])

# Should now succeed — full path, no AR involvement
spawn_blueprint_actor(blueprint_name="/Game/ProjectFiles/Core/Actors/Character/BP_EnemyCharacter",
                     actor_name="V2", location=[100,0,300])

# Should still succeed — back-compat path
# (Project would need a BP at /Game/Blueprints/Foo first; copy any small BP there to test.)

# Should error cleanly — typo
spawn_blueprint_actor(blueprint_name="BP_DoesNotExist", actor_name="V3", location=[200,0,300])
# Expect BLUEPRINT_NOT_FOUND with message naming both /Game/Blueprints/BP_DoesNotExist
# and the AssetRegistry search.

# Should error with ambiguity message
# Setup: duplicate any BP into /Game/Blueprints/SameName AND /Game/Other/SameName
spawn_blueprint_actor(blueprint_name="SameName", actor_name="V4", location=[300,0,300])
# Expect explicit error listing both package paths.
```

Then run the existing automation suite at `Plugins/UEMCP/Source/UEMCP/Private/Tests/UEMCPTests.cpp` to confirm no regressions.

---

## 8. Out of scope (don't expand the fix here)

- Removing the legacy `/Game/Blueprints/` prefix entirely. Some Epic-template-derived projects still use it; keep Case 2 for back-compat.
- Adding plugin-settings-configurable lookup roots. Case 3's AR fallback already solves the layout problem; settings would be over-engineered.
- Auto-moving user BPs into `/Game/Blueprints/`. Don't.
- Hardening other paths in the plugin against unrelated assumptions.

---

## 9. References from this session

- Failing handler: `Plugins/UEMCP/Source/UEMCP/Private/ActorHandlers.cpp:611-675` (function `HandleSpawnBlueprintActor`).
- Offending lines: `:630` (printf with hardcoded prefix), `:631` (filesystem probe), `:634` (error message that exposes the assumption).
- Sibling handler convention documented in `find_tools` output for the `blueprints-write` toolset (delivered as a `tips` field on the search response).
- Test asset for repro: `/Game/ProjectFiles/Core/Actors/Character/BP_EnemyCharacter`, instances at `[197.38, 6.57, 298]`, `[-291.96, -30.99, 298]`, `[-52.08, 194.89, 298]`, `[-42.50, -219.30, 298]` in `Lvl_ThirdPerson1`.
