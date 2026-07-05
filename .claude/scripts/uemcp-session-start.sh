#!/usr/bin/env bash
# SessionStart hook (cloud/Linux only): materialize this project's LFS binaries
# so UEMCP offline tools parse real .uasset/.umap bytes instead of ~130 B
# pointer stubs (which fail with "bad magic 0x73726576").
#
# No-op everywhere else: on Windows/local machines the working copy already has
# real binaries, and on non-Linux uname this exits immediately.
#
# Scope: UEMCP_LFS_SCOPE if set (project | anims | marketplace | all), else
# project (~2 MB). Expand mid-session with:
#   bash .claude/scripts/uemcp-lfs-pull.sh <scope|glob> [--dry-run]
set -u

case "$(uname -s 2>/dev/null || echo unknown)" in
  Linux) : ;;
  *) exit 0 ;;
esac

DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"

# Fast exit if binaries are already materialized (hook also fires on resume).
sample="$(find "$DIR/../../Content/ProjectFiles" -name '*.uasset' -size +1k -print 2>/dev/null | head -1)"
[ -n "$sample" ] && exit 0

bash "$DIR/uemcp-lfs-pull.sh" "${UEMCP_LFS_SCOPE:-project}" || true
exit 0
