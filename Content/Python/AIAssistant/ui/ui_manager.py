"""
UI Manager for communication between Python and Editor Utility Widgets.
Handles response formatting and file-based state management.
"""
from typing import Optional

from ..core.utils import (
    Logger,
    get_ai_console_dir,
    log_conversation,
    read_file,
    write_file,
)


class UIManager:
    """Manages UI communication and response formatting."""

    def __init__(self):
        self.logger = Logger("UIManager", verbose=False)
        self.last_reply_path = (
            get_ai_console_dir() / "last_reply.txt"
        )

    def show_thinking(self) -> None:
        """Write 'thinking' state to UI."""
        msg = "⏳ Thinking..."
        write_file(self.last_reply_path, msg)
        self.logger.debug("Set UI to thinking state")

    def show_response(self, text: str) -> None:
        """Write AI response to UI file."""
        write_file(self.last_reply_path, text)
        self.logger.debug(f"Wrote response: {len(text)} chars")

    def get_last_response(self) -> Optional[str]:
        """Read the last response from UI file."""
        return read_file(self.last_reply_path)

    def log_exchange(self, user_msg: str, ai_msg: str) -> None:
        """Log a conversation exchange."""
        log_conversation("user", user_msg)
        log_conversation("assistant", ai_msg)
        self.logger.debug("Logged conversation exchange")

    def format_error(self, error: str) -> str:
        """Format an error message for UI display."""
        return f"❌ Error: {error}"

    def format_success(self, message: str) -> str:
        """Format a success message for UI display."""
        return f"✅ {message}"

    def format_info(self, message: str) -> str:
        """Format an info message for UI display."""
        return f"ℹ️ {message}"


# Global UI manager instance
_ui_manager: Optional[UIManager] = None


def get_ui_manager() -> UIManager:
    """Get or create the global UI manager."""
    global _ui_manager
    if _ui_manager is None:
        _ui_manager = UIManager()
    return _ui_manager
