"""
Utility functions for UE5 AI Assistant.
Logging, file operations, and common helpers.
"""
import datetime
import json
from pathlib import Path
from typing import Optional

try:
    import unreal  # type: ignore
    HAS_UNREAL = True
except ImportError:
    HAS_UNREAL = False
    unreal = None  # type: ignore  # Only available in UE environment


class Logger:
    """Unified logging system for the AI Assistant."""

    def __init__(self, name: str, verbose: bool = False):
        self.name = name
        self.verbose = verbose

    def _log(self, level: str, msg: str, icon: str = "") -> None:
        """Internal logging method."""
        prefix = f"[{self.name}]"
        full_msg = f"{prefix} {icon} {msg}".strip()

        if HAS_UNREAL and unreal is not None:
            # unreal is available (UE environment only)
            if level == "error":
                unreal.log_error(full_msg)  # type: ignore[union-attr]
            elif level == "warn":
                unreal.log_warning(full_msg)  # type: ignore[union-attr]
            else:
                unreal.log(full_msg)  # type: ignore[union-attr]

        print(full_msg)

    def info(self, msg: str) -> None:
        """Log info message."""
        self._log("info", msg, "ℹ️")

    def success(self, msg: str) -> None:
        """Log success message."""
        self._log("info", msg, "✅")

    def warn(self, msg: str) -> None:
        """Log warning message."""
        self._log("warn", msg, "⚠️")

    def error(self, msg: str) -> None:
        """Log error message."""
        self._log("error", msg, "❌")

    def debug(self, msg: str) -> None:
        """Log debug message (only if verbose)."""
        if self.verbose:
            self._log("info", msg, "🔍")


def get_project_saved_dir() -> Path:
    """Get the project's Saved directory."""
    if HAS_UNREAL and unreal is not None:
        # unreal is available (UE environment only)
        return Path(
            unreal.Paths.project_saved_dir()  # type: ignore[union-attr]
        )
    return Path(".")


def get_ai_console_dir() -> Path:
    """Get the AIConsole directory."""
    ai_dir = get_project_saved_dir() / "AIConsole"
    ai_dir.mkdir(parents=True, exist_ok=True)
    return ai_dir


def write_file(path: Path, content: str) -> None:
    """Write content to a file."""
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        f.write(content)


def append_file(path: Path, content: str) -> None:
    """Append content to a file."""
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "a", encoding="utf-8") as f:
        f.write(content)


def read_file(path: Path) -> Optional[str]:
    """Read content from a file."""
    try:
        if path.exists():
            with open(path, "r", encoding="utf-8") as f:
                return f.read()
    except Exception:
        pass
    return None


def timestamp() -> str:
    """Get current timestamp as string."""
    return datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")


def log_conversation(role: str, text: str) -> None:
    """Log a conversation exchange to file."""
    log_path = get_ai_console_dir() / "conversation_log.txt"
    entry = {"ts": timestamp(), "role": role, "text": text}
    append_file(
        log_path, json.dumps(entry, ensure_ascii=False) + "\n"
    )
