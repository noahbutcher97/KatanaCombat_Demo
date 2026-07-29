#!/usr/bin/env bash
# SessionStart hook (cloud/Linux only): materialize this project's LFS binaries
# so UEMCP offline tools parse real .uasset/.umap bytes instead of ~130 B
# pointer stubs (which fail with "bad magic 0x73726576").
#
# CLOUD-ONLY by three layered guards (any one suffices to no-op locally):
#   1. non-Linux uname (Windows/git-bash/macOS) -> exit
#   2. no Claude-cloud container marker in env  -> exit
#   3. binaries already real (local clones, resumed sessions) -> exit
# On local machines the working copy already has real binaries, so even if the
# env markers are ever renamed upstream, guards 1/3 still keep this inert.
#
# Scope: UEMCP_LFS_SCOPE if set (project | anims | marketplace | all), else
# project (~2 MB). Expand mid-session with:
#   bash .claude/scripts/uemcp-lfs-pull.sh <scope|glob> [--dry-run]
set -u

case "$(uname -s 2>/dev/null || echo unknown)" in
  Linux) : ;;
  *) exit 0 ;;
esac

# Cloud/remote container markers (Claude Code on the web / mobile sessions).
# If neither is present, this is a local Linux machine — stay out of its way.
if [ -z "${CLAUDE_CODE_CONTAINER_ID:-}" ] && [ "${CCR_AGENT_PROXY_ENABLED:-0}" != "1" ]; then
  exit 0
fi

DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"

# Fast exit if binaries are already materialized (hook also fires on resume).
sample="$(find "$DIR/../../Content/ProjectFiles" -name '*.uasset' -size +1k -print 2>/dev/null | head -1)"
[ -n "$sample" ] && exit 0

bash "$DIR/uemcp-lfs-pull.sh" "${UEMCP_LFS_SCOPE:-project}" || true
exit 0
