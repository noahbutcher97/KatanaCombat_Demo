"""
UE5 AI Assistant - Modular Architecture
Author: Noah Butcher
Version: 3.0 (October 2025)

A modular AI assistant system for Unreal Engine 5.6+ that provides:
- Smart viewport and scene analysis
- Natural language command execution
- Async API communication
- Extensible action system

Quick Start:
    from AIAssistant import send_command
    response = send_command("what do I see?")
"""

__version__ = "3.0.0"
__author__ = "Noah Butcher"

# Core components (new folder structure)
from .collection import context_collector
from .core import config, main, utils
from .core.config import get_config

# Main entry points for convenience
from .core.main import get_assistant, send_command
from .execution import action_executor
from .execution.action_executor import get_executor
from .network import api_client, async_client
from .ui import ui_manager

__all__ = [
    # Main functions
    "send_command",
    "get_assistant",
    "get_config",
    "get_executor",
    # Modules
    "config",
    "utils",
    "api_client",
    "async_client",
    "context_collector",
    "action_executor",
    "ui_manager",
    "main",
]

# AUTO-INITIALIZE: Connect to backend when module is imported
# This is CRITICAL - without this, the client never connects!
try:
    import unreal
    # Trigger auto-initialization from main.py
    from .core.main import _auto_init
    _auto_init()
except ImportError:
    # Not in UE5 environment - skip auto-init
    pass
except Exception as e:
    try:
        import unreal
        unreal.log_error(f"❌ Failed to auto-initialize AI Assistant: {e}")
    except:
        print(f"❌ Failed to auto-initialize AI Assistant: {e}")
