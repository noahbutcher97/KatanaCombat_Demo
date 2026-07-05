# UEMCP in cloud (Claude Code on the web) sessions

This project uses [UEMCP](https://github.com/noahbutcher97/uemcp) — Noah's Unreal
Engine MCP bridge. UEMCP has **two layers**, and only one can run in a cloud
session:

| Layer | Transport | Needs a running editor? | Works in cloud? |
|-------|-----------|-------------------------|-----------------|
| **Offline** | reads `.uasset`/`.umap`/`.ini` + AssetRegistry from disk | No | ✅ **Yes** |
| Live | TCP `55558` (C++ plugin), TCP `55557`, HTTP `30010` (Remote Control) | Yes | ❌ No editor in cloud |

So in a web session you get the **offline toolset** (~18 read-only introspection
tools: `project_info`, `query_asset_registry`, `read_asset_properties`,
`inspect_blueprint`, `list_level_actors`, `list_gameplay_tags`, the `bp_*`
graph-trace tools, etc.). Anything that mutates the editor or runs PIE requires
your local machine.

## Where the offline server lives

**`$HOME/uemcp`** (i.e. `/home/user/uemcp` in a web container) — a sibling of the
repo checkout, cloned fresh from the public repo each time the environment is
provisioned. The consumed entrypoint is `$HOME/uemcp/server/server.mjs`.

The container is ephemeral: the clone does **not** persist between sessions, which
is why it is re-provisioned by a startup script rather than vendored into this
repo.

## How to make it import immediately (one-time setup)

Add this line to your **environment setup script** (Claude Code on the web →
environment settings → setup script). It runs when the container is provisioned,
before any session starts, so the server file exists by the time MCP servers load:

```bash
bash .claude/scripts/uemcp-cloud-bootstrap.sh
```

That script (idempotent, safe to re-run):
1. clones/updates the public `uemcp` repo into `$HOME/uemcp`,
2. `npm install`s the server deps (skipped when the lockfile is unchanged),
3. smoke-checks that the server boots headless,
4. **materializes this project's binary assets from Git LFS** (see next section).

### Why step 4 matters — LFS binaries

The offline tools parse **binary** `.uasset`/`.umap`. But cloud clones come down
with LFS content as ~130-byte **pointer stubs** (git-lfs isn't smudged on clone),
so without a pull the tools have nothing real to read — they'd only see stubs.

The bootstrap installs `git-lfs` (via apt) and pulls a chosen **scope**,
defaulting to just the project-authored content. Without this, the binary-decode
tools (`read_asset_properties`, `query_asset_registry`, `inspect_blueprint`, the
`bp_*` graph tools) are inert; with it they decode real serialized asset data —
the same capability that produced
[`docs/audits/AUDIT_UEMCP_OFFLINE_2026-05-29.md`](../docs/audits/AUDIT_UEMCP_OFFLINE_2026-05-29.md).

#### Scope presets

| Scope | Globs | Size | Use when |
|-------|-------|------|----------|
| `project` *(default)* | `Content/ProjectFiles/**` | ~2 MB / 105 | asset/config/blueprint integrity — the game's own data |
| `anims` | + `Content/Assets/Animations/**` | ~2.85 GB | you need to introspect the marketplace anim packs |
| `marketplace` | + `Content/Assets/**` | ~4.27 GB | all imported assets (anims, characters, VFX, SFX) |
| `all` | everything LFS-tracked | ~4.28 GB | full parity with a local checkout |

Set the **startup** scope by exporting one of these in the environment setup
script *before* the bootstrap call:

```bash
export UEMCP_LFS_SCOPE=anims          # or: project | marketplace | all
bash .claude/scripts/uemcp-cloud-bootstrap.sh
```

Other knobs:
- `UEMCP_LFS_INCLUDE` — explicit space-separated globs (overrides the preset)
- `UEMCP_SKIP_LFS=1` — skip the pull entirely

#### Expanding scope on demand (mid-session)

You don't have to decide up front. Start lean (`project`) and pull more whenever
a task needs it — objects already cached are not re-downloaded:

```bash
# Check cost first (pulls nothing):
bash .claude/scripts/uemcp-lfs-pull.sh anims --dry-run

# Then pull a preset...
bash .claude/scripts/uemcp-lfs-pull.sh anims

# ...or just one pack by explicit glob:
bash .claude/scripts/uemcp-lfs-pull.sh 'Content/Assets/Animations/ARPG_Samurai/**'
```

`uemcp-lfs-pull.sh` is the single source of truth for scope resolution, sizing,
and pulling; the bootstrap delegates its step 4 to it.

Once provisioned, the **`uemcp-offline`** server in [`.mcp.json`](../.mcp.json)
loads native `mcp__uemcp-offline__*` tools automatically. It auto-attaches to this
project via the workspace root the harness advertises — no manual `attach_project`
needed. The offline toolset must be enabled once per session via the
`enable_toolset` management tool (or `find_tools`), then its tools appear.

### Overrides

The bootstrap and wrapper honor:

- `UEMCP_HOME` — install location (default `$HOME/uemcp`)
- `UEMCP_REPO` — clone URL (default the public repo)
- `UEMCP_REF`  — branch/tag/commit to check out (default: repo default branch)

If you point `UEMCP_HOME` somewhere other than `$HOME/uemcp`, update the
`uemcp-offline` `args` path in `.mcp.json` to match.

## Guaranteed CLI path (no MCP handshake)

For scripts, CI, or when you just want raw output, drive the offline tools
directly — this bypasses MCP entirely and is the most reliable path:

```bash
node .claude/scripts/uemcp-offline.mjs                 # list offline tools
node .claude/scripts/uemcp-offline.mjs project_info
node .claude/scripts/uemcp-offline.mjs query_asset_registry \
  '{"path_prefix":"/Game/ProjectFiles/Data/PDA/Attack","limit":200}'
node .claude/scripts/uemcp-offline.mjs read_asset_properties \
  '{"asset_path":"Content/ProjectFiles/Data/PDA/Attack/AttackConfigurations/DA_Config_Katana.uasset"}'
```

Defaults `UNREAL_PROJECT_ROOT` to this repo's root. This is the same tooling that
produced [`docs/audits/AUDIT_UEMCP_OFFLINE_2026-05-29.md`](../docs/audits/AUDIT_UEMCP_OFFLINE_2026-05-29.md).

## Windows / local machines

Untouched. The native `uemcp` entry in `.mcp.json` (pointing at
`D:/DevTools/UEMCP`) still drives the full live + offline surface locally. The
`uemcp-offline` entry resolves to a `${HOME}/uemcp` path that doesn't exist on
Windows, so it stays inert there.

## Known limitation (cloud native-MCP attach)

Native-MCP offline attach relies on the client advertising a workspace root that
contains the `.uproject` — which the Claude Code harness does. Launching the
server over a raw stdio pipe with **no** roots advertised (and no
`UEMCP_PROJECT_ATTACH_MODE=env`) leaves the offline layer reporting
`PROJECT_NOT_ATTACHED`. If native tools ever show unavailable, fall back to the
CLI wrapper above, which does not depend on attach state.
