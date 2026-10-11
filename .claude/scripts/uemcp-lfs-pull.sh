#!/usr/bin/env bash
# uemcp-lfs-pull.sh — materialize this project's Git LFS binaries so the UEMCP
# offline tools have real .uasset/.umap bytes to parse.
#
# Cloud clones bring LFS content down as ~130 B pointer stubs. This pulls the
# real objects for a chosen SCOPE. Safe to run standalone at any time to EXPAND
# what's materialized in a running session — LFS objects already cached are not
# re-downloaded.
#
# Usage:
#   bash .claude/scripts/uemcp-lfs-pull.sh [scope|glob ...] [--dry-run]
#
#   scope keyword (default: project):
#     project       Content/ProjectFiles/**                     (~2 MB)   the game's own content
#     anims         + Content/Assets/Animations/**              (~2.9 GB) project + marketplace anim packs
#     marketplace   + Content/Assets/**                         (~4.3 GB) project + all imported assets
#     all           everything LFS-tracked                      (~4.3 GB)
#
#   explicit globs (contain a "/"): pulled verbatim, e.g.
#     bash .claude/scripts/uemcp-lfs-pull.sh 'Content/Assets/Animations/ARPG_Samurai/**'
#
#   --dry-run   resolve the scope and print the on-disk size, pull nothing.
#
# Env: UEMCP_LFS_SCOPE / UEMCP_LFS_INCLUDE mirror the positional args (args win).
set -euf

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
PROJECT_REPO="$(git -C "$SCRIPT_DIR" rev-parse --show-toplevel 2>/dev/null || echo '')"

log() { printf '[uemcp-lfs] %s\n' "$*" >&2; }

[ -n "$PROJECT_REPO" ] || { log "not inside a git repo"; exit 1; }

# ── Parse args ────────────────────────────────────────────────────────────
DRYRUN=0
POS=""
for a in "$@"; do
  case "$a" in
    --dry-run|-n) DRYRUN=1 ;;
    *) POS="$POS $a" ;;
  esac
done
POS="${POS# }"
[ -n "$POS" ] || POS="${UEMCP_LFS_INCLUDE:-${UEMCP_LFS_SCOPE:-project}}"

# ── Resolve scope keyword -> include globs (empty INCLUDE => everything) ────
# First token decides: a known keyword expands to a preset; anything with "/"
# means the caller passed explicit globs (use POS verbatim).
first="${POS%% *}"
case "$first" in
  project)     INCLUDE="Content/ProjectFiles/**" ;;
  anims)       INCLUDE="Content/ProjectFiles/** Content/Assets/Animations/**" ;;
  marketplace) INCLUDE="Content/ProjectFiles/** Content/Assets/**" ;;
  all)         INCLUDE="" ;;
  *)           case "$first" in */*) INCLUDE="$POS" ;; *)
                 log "unknown scope '$first' (use: project | anims | marketplace | all | <glob>)"; exit 2 ;;
               esac ;;
esac

# ── Size estimate for the resolved scope ──────────────────────────────────
size_of() {  # args: include globs (none => all)
  if [ "$#" -eq 0 ]; then
    git -C "$PROJECT_REPO" lfs ls-files -s 2>/dev/null
  else
    inc=""; for g in "$@"; do inc="$inc -I $g"; done
    # shellcheck disable=SC2086
    git -C "$PROJECT_REPO" lfs ls-files -s $inc 2>/dev/null
  fi | awk '
    { i=match($0,/\([0-9.]+ [KMG]B\)$/); if(!i)next; s=substr($0,i+1); gsub(/[()]/,"",s);
      split(s,su," "); v=su[1]; u=su[2];
      if(u=="KB")v*=1024; else if(u=="MB")v*=1048576; else if(u=="GB")v*=1073741824; t+=v; c++ }
    END{ printf "%.1f MB across %d files", t/1048576, c }'
}

# shellcheck disable=SC2086
if [ -z "$INCLUDE" ]; then EST="$(size_of)"; else EST="$(size_of $INCLUDE)"; fi
log "scope: ${first} -> ${INCLUDE:-<everything>}"
log "estimated target: ${EST}"

if [ "$DRYRUN" = "1" ]; then log "dry-run: nothing pulled"; exit 0; fi

# ── Ensure git-lfs ─────────────────────────────────────────────────────────
if ! command -v git-lfs >/dev/null 2>&1; then
  log "installing git-lfs"
  ( apt-get install -y git-lfs >/dev/null 2>&1 || sudo apt-get install -y git-lfs >/dev/null 2>&1 ) \
    || { log "could not install git-lfs"; exit 1; }
fi
git -C "$PROJECT_REPO" lfs install --skip-smudge >/dev/null 2>&1 || true

# ── Pull + materialize ────────────────────────────────────────────────────
if [ -z "$INCLUDE" ]; then
  log "pulling ALL LFS objects (this is large)"
  git -C "$PROJECT_REPO" lfs pull >/dev/null 2>&1 || log "lfs pull error (network policy?)"
  git -C "$PROJECT_REPO" lfs checkout >/dev/null 2>&1 || true
else
  inc_args=""; for g in $INCLUDE; do inc_args="$inc_args --include=$g"; done
  # shellcheck disable=SC2086
  git -C "$PROJECT_REPO" lfs pull $inc_args >/dev/null 2>&1 || log "lfs pull error (network policy?); trying checkout from cache"
  for g in $INCLUDE; do git -C "$PROJECT_REPO" lfs checkout "${g%/**}" >/dev/null 2>&1 || true; done
fi

# ── Verify by count of materialized (>1 KB) assets ────────────────────────
real_ct=0
if [ -z "$INCLUDE" ]; then roots="Content"; else roots=""; for g in $INCLUDE; do roots="$roots ${g%/**}"; done; fi
for r in $roots; do
  d="$PROJECT_REPO/$r"; [ -d "$d" ] || continue
  n=$(find "$d" \( -name '*.uasset' -o -name '*.umap' \) -size +1k 2>/dev/null | wc -l)
  real_ct=$((real_ct + n))
done
if [ "$real_ct" -gt 0 ]; then
  log "OK — $real_ct binaries materialized for offline introspection"
else
  log "WARN — no binaries materialized; offline tools limited to text-derived data"
  exit 1
fi
