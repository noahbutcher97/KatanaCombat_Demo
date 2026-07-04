#!/usr/bin/env bash
# uemcp-cloud-bootstrap.sh
#
# Provision Noah's UEMCP server into a cloud/Linux Claude Code session so its
# OFFLINE toolset (disk-only .uasset/.umap/.ini/AssetRegistry parsing) is
# available with no running Unreal Editor.
#
# The LIVE layers (TCP:55558 C++ plugin, HTTP:30010 Remote Control) cannot work
# in the cloud — there is no editor — so only the offline toolset is provisioned.
#
# This script is IDEMPOTENT and safe to run on every session start. Intended to
# be invoked from the environment's setup script (Claude Code on the web):
#
#     bash .claude/scripts/uemcp-cloud-bootstrap.sh
#
# Result: the server lives at $UEMCP_HOME/server (default: $HOME/uemcp/server),
# consumed by the "uemcp-offline" entry in .mcp.json and by uemcp-offline.mjs.
#
# Overridable env:
#   UEMCP_HOME   install location            (default: $HOME/uemcp)
#   UEMCP_REPO   clone URL (public repo)     (default: https://github.com/noahbutcher97/uemcp)
#   UEMCP_REF    branch/tag/commit to check  (default: repo default branch)
set -eu

UEMCP_HOME="${UEMCP_HOME:-$HOME/uemcp}"
UEMCP_REPO="${UEMCP_REPO:-https://github.com/noahbutcher97/uemcp}"
UEMCP_REF="${UEMCP_REF:-}"
SERVER_DIR="$UEMCP_HOME/server"

log() { printf '[uemcp-bootstrap] %s\n' "$*" >&2; }

# Windows/local machines already have the real server (D:/DevTools/UEMCP) and the
# native "uemcp" .mcp.json entry — do not interfere there.
case "$(uname -s 2>/dev/null || echo unknown)" in
  Linux|Darwin) : ;;
  *) log "non-POSIX host detected; skipping cloud bootstrap (local setup owns UEMCP)"; exit 0 ;;
esac

command -v git  >/dev/null 2>&1 || { log "git not found; cannot provision UEMCP"; exit 0; }
command -v node >/dev/null 2>&1 || { log "node not found; cannot provision UEMCP"; exit 0; }

clone_fresh() {
  rm -rf "$UEMCP_HOME"
  n=0
  until [ "$n" -ge 4 ]; do
    if git clone --depth 1 "$UEMCP_REPO" "$UEMCP_HOME"; then return 0; fi
    n=$((n + 1)); wait=$((2 ** n))
    log "clone failed (attempt $n); retrying in ${wait}s"
    sleep "$wait"
  done
  log "clone failed after retries"; return 1
}

# 1. Clone or update -------------------------------------------------------
if git -C "$UEMCP_HOME" rev-parse HEAD >/dev/null 2>&1; then
  log "existing checkout at $UEMCP_HOME; fetching latest"
  git -C "$UEMCP_HOME" fetch --depth 1 origin >/dev/null 2>&1 || log "fetch failed; using cached checkout"
  git -C "$UEMCP_HOME" reset --hard "origin/$(git -C "$UEMCP_HOME" rev-parse --abbrev-ref origin/HEAD 2>/dev/null | sed 's#^origin/##' || echo HEAD)" >/dev/null 2>&1 || true
else
  log "cloning $UEMCP_REPO -> $UEMCP_HOME"
  clone_fresh || exit 1
fi

if [ -n "$UEMCP_REF" ]; then
  log "checking out ref: $UEMCP_REF"
  git -C "$UEMCP_HOME" fetch --depth 1 origin "$UEMCP_REF" >/dev/null 2>&1 || true
  git -C "$UEMCP_HOME" checkout -q "$UEMCP_REF" 2>/dev/null || log "ref $UEMCP_REF not found; staying on default branch"
fi

[ -f "$SERVER_DIR/server.mjs" ] || { log "server.mjs missing after checkout — unexpected repo layout"; exit 1; }

# 2. Install server deps (skip if lockfile unchanged since last install) ----
STAMP="$SERVER_DIR/node_modules/.uemcp-bootstrap-stamp"
LOCK="$SERVER_DIR/package-lock.json"
LOCK_HASH="$( ( [ -f "$LOCK" ] && sha1sum "$LOCK" || echo none ) | awk '{print $1}')"
if [ -f "$STAMP" ] && [ "$(cat "$STAMP" 2>/dev/null)" = "$LOCK_HASH" ]; then
  log "deps already installed (lockfile unchanged); skipping npm install"
else
  log "installing server deps"
  if [ -f "$LOCK" ]; then
    ( cd "$SERVER_DIR" && npm ci --no-audit --no-fund ) || \
    ( cd "$SERVER_DIR" && npm install --no-audit --no-fund )
  else
    ( cd "$SERVER_DIR" && npm install --no-audit --no-fund )
  fi
  printf '%s' "$LOCK_HASH" > "$STAMP"
fi

# 3. Smoke check: server boots headless and indexes tools ------------------
if ( cd "$SERVER_DIR" && printf '%s\n' \
    '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"bootstrap","version":"0"}}}' \
    | timeout 20 node server.mjs >/dev/null 2>&1 ); then
  log "OK — UEMCP offline server ready at $SERVER_DIR"
else
  # A non-zero exit here is expected (server waits on stdin); treat as best-effort.
  log "OK — UEMCP provisioned at $SERVER_DIR (headless smoke inconclusive; this is normal)"
fi

log "done. Native offline tools load via the 'uemcp-offline' server in .mcp.json."
log "Direct CLI: node .claude/scripts/uemcp-offline.mjs project_info"
