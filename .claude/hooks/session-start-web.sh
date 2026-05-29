#!/bin/bash
# SessionStart hook (Claude Code on the web): make `git push` work by installing
# the GitHub CLI and wiring git auth from a token provided as an environment secret.
#
# Design goals:
#   - Web-only: no-op in local/desktop sessions.
#   - Idempotent: safe to run every session; skips work already done.
#   - Graceful: never blocks the session. No token, no network, or a restrictive
#     network policy all degrade quietly (always exits 0).
#   - No secret on disk: prefers `gh auth setup-git` (gh supplies the token from
#     the environment at push time) over writing the token into ~/.gitconfig.
#
# Provide the token by setting GH_TOKEN (or GITHUB_TOKEN) as an environment
# variable/secret on the Claude Code web environment. Use a fine-grained PAT
# scoped to just the repos you push to, with Contents: read/write.

set -uo pipefail   # intentionally no `-e`: a failed step must not abort the session

log() { printf '[session-start] %s\n' "$*" >&2; }

# 1. Web-only guard ----------------------------------------------------------
if [ "${CLAUDE_CODE_REMOTE:-}" != "true" ]; then
  log "not a remote session; skipping gh/git auth setup."
  exit 0
fi

TOKEN="${GH_TOKEN:-${GITHUB_TOKEN:-}}"

# 2. Install gh if missing (best-effort; failure is non-fatal) ----------------
install_gh() {
  command -v gh >/dev/null 2>&1 && return 0

  # Method A: apt (Debian/Ubuntu) when we have root + the package toolchain.
  if [ "$(id -u)" = "0" ] && command -v apt-get >/dev/null 2>&1; then
    if curl -fsSL https://cli.github.com/packages/githubcli-archive-keyring.gpg \
         -o /usr/share/keyrings/githubcli-archive-keyring.gpg 2>/dev/null; then
      echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/githubcli-archive-keyring.gpg] https://cli.github.com/packages stable main" \
        > /etc/apt/sources.list.d/github-cli.list 2>/dev/null
      if apt-get update -y >/dev/null 2>&1 && apt-get install -y gh >/dev/null 2>&1; then
        return 0
      fi
    fi
  fi

  # Method B: download the release tarball into ~/.local/bin (no root needed).
  local arch ver url dest tmp
  case "$(uname -m)" in
    x86_64)        arch=amd64 ;;
    aarch64|arm64) arch=arm64 ;;
    *) log "unsupported arch $(uname -m); cannot fetch gh binary."; return 1 ;;
  esac
  ver=$(curl -fsSL https://api.github.com/repos/cli/cli/releases/latest 2>/dev/null \
        | sed -n 's/.*"tag_name":[[:space:]]*"v\([^"]*\)".*/\1/p' | head -1)
  if [ -z "$ver" ]; then
    log "could not resolve latest gh version (no network / restricted policy)."
    return 1
  fi
  dest="$HOME/.local/bin"; tmp="$(mktemp -d)"
  url="https://github.com/cli/cli/releases/download/v${ver}/gh_${ver}_linux_${arch}.tar.gz"
  mkdir -p "$dest"
  if curl -fsSL "$url" 2>/dev/null | tar -xz -C "$tmp" 2>/dev/null \
       && cp "$tmp/gh_${ver}_linux_${arch}/bin/gh" "$dest/gh" 2>/dev/null; then
    chmod +x "$dest/gh"
    export PATH="$dest:$PATH"
    # Persist PATH for the rest of the session.
    if [ -n "${CLAUDE_ENV_FILE:-}" ]; then
      echo "export PATH=\"$dest:\$PATH\"" >> "$CLAUDE_ENV_FILE"
    fi
    rm -rf "$tmp"
    return 0
  fi
  rm -rf "$tmp"
  log "gh download failed (no network / restricted policy)."
  return 1
}

if install_gh; then
  log "gh available: $(command -v gh)"
else
  log "gh not installed; continuing (git auth may fall back to token helper)."
fi

# 3. Wire git auth -----------------------------------------------------------
if [ -z "$TOKEN" ]; then
  log "no GH_TOKEN/GITHUB_TOKEN set; skipping git auth. (Set one as an env secret to enable git push.)"
  exit 0
fi

if command -v gh >/dev/null 2>&1; then
  export GH_TOKEN="$TOKEN"               # gh reads the token from the environment
  if gh auth setup-git >/dev/null 2>&1; then
    log "configured git to authenticate via gh (no token written to disk)."
    exit 0
  fi
  log "gh auth setup-git failed; falling back to token credential helper."
fi

# Fallback: only when gh is unavailable/failed. Writes a credential helper that
# emits the token; this stores the token in ~/.gitconfig (acceptable in an
# ephemeral container, but gh setup-git above is preferred when it works).
git config --global --replace-all credential."https://github.com".helper \
  "!f() { echo username=x-access-token; echo \"password=${TOKEN}\"; }; f" 2>/dev/null \
  && log "configured token credential helper for github.com." \
  || log "could not configure git credential helper."

exit 0
