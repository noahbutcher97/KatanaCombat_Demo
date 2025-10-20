"""
Blueprint Integration Helpers for UE5 AI Assistant
==================================================

This module contains helper functions for Blueprint communication using
file-based I/O. All functions write their output to files that Blueprint
can read using "Load File to String".

Output files are written to: [YourProject]/Saved/AIConsole/

Usage in Blueprint:
1. Execute Python Script with the function code
2. Add Delay node (0.1 seconds) to ensure file is written
3. Use "Load File to String" to read the output file
4. Parse the result string as needed
"""

from pathlib import Path
from typing import TYPE_CHECKING

from AIAssistant import config

if TYPE_CHECKING:
    import unreal  # type: ignore
    HAS_UNREAL = True
else:
    try:
        import unreal  # type: ignore
        HAS_UNREAL = True
    except ImportError:
        unreal = None  # type: ignore
        HAS_UNREAL = False


def get_output_path(filename: str) -> Path:
    """Get the output file path for Blueprint communication."""
    if HAS_UNREAL and unreal is not None:
        saved_dir = Path(unreal.Paths.project_saved_dir()  # type: ignore
                         )
    else:
        saved_dir = Path.cwd() / "Saved"

    output_path = saved_dir / "AIConsole" / filename
    output_path.parent.mkdir(parents=True, exist_ok=True)
    return output_path


# ============================================================================
# SERVER SELECTION FUNCTIONS
# ============================================================================


def get_active_server() -> None:
    """
    Get the currently active server name and write to file.
    
    Output file: server_status.txt
    Content: "production" | "dev" | "localhost"
    
    Blueprint usage:
        Execute Python Script: get_active_server()
        Delay: 0.1 seconds
        Load File to String: "[Project]/Saved/AIConsole/server_status.txt"
    """
    cfg = config.get_config()
    active_server = cfg.get_active_server()

    output_path = get_output_path("server_status.txt")
    output_path.write_text(active_server, encoding='utf-8')
    print(f"[Blueprint Helper] Active server: {active_server}")


def switch_server(server_name: str) -> None:
    """
    Switch to a different server and write result to file.
    
    Args:
        server_name: "production", "dev", or "localhost"
    
    Output file: server_switch_result.txt
    Content: "success|[url]" or "error|[message]"
    
    Blueprint usage:
        Execute Python Script: switch_server('{0}')
        (Use String Format to inject server name)
        Delay: 0.1 seconds
        Load File to String: 
            "[Project]/Saved/AIConsole/server_switch_result.txt"
        Split String by "|" to get [status, message]
    """
    cfg = config.get_config()
    success = cfg.switch_server(server_name)

    output_path = get_output_path("server_switch_result.txt")

    if success:
        result = f"success|{cfg.api_url}"
    else:
        result = f"error|Invalid server: {server_name}"

    output_path.write_text(result, encoding='utf-8')
    print(f"[Blueprint Helper] Server switch: {result}")


def get_server_display() -> None:
    """
    Get server display string for UI and write to file.
    
    Output file: server_display.txt
    Content: "[server_name] → [url]"
    
    Blueprint usage:
        Execute Python Script: get_server_display()
        Delay: 0.1 seconds
        Load File to String: "[Project]/Saved/AIConsole/server_display.txt"
    """
    cfg = config.get_config()
    active = cfg.get_active_server()
    url = cfg.api_url
    status = f"{active} → {url}"

    output_path = get_output_path("server_display.txt")
    output_path.write_text(status, encoding='utf-8')
    print(f"[Blueprint Helper] Display: {status}")


def list_servers() -> None:
    """
    Get list of all available servers and write to file.
    
    Output file: server_list.txt
    Content: "production,dev,localhost"
    
    Blueprint usage:
        Execute Python Script: list_servers()
        Delay: 0.1 seconds
        Load File to String: "[Project]/Saved/AIConsole/server_list.txt"
        Split String by "," to get array of server names
    """
    cfg = config.get_config()
    servers = ",".join(cfg.SERVER_PRESETS.keys())

    output_path = get_output_path("server_list.txt")
    output_path.write_text(servers, encoding='utf-8')
    print(f"[Blueprint Helper] Available servers: {servers}")


# ============================================================================
# INLINE SCRIPTS FOR BLUEPRINT (COPY-PASTE READY)
# ============================================================================
"""
SCRIPT 1: Get Active Server
----------------------------
from AIAssistant import blueprint_helpers
blueprint_helpers.get_active_server()

Read from: [Project]/Saved/AIConsole/server_status.txt


SCRIPT 2: Switch Server
-----------------------
from AIAssistant import blueprint_helpers
blueprint_helpers.switch_server('{0}')

(Use String Format node to inject server name into {0})
Read from: [Project]/Saved/AIConsole/server_switch_result.txt
Parse: Split by "|" to get [status, message]


SCRIPT 3: Get Server Display
----------------------------
from AIAssistant import blueprint_helpers
blueprint_helpers.get_server_display()

Read from: [Project]/Saved/AIConsole/server_display.txt


SCRIPT 4: List All Servers
--------------------------
from AIAssistant import blueprint_helpers
blueprint_helpers.list_servers()

Read from: [Project]/Saved/AIConsole/server_list.txt
Parse: Split by "," to get array


ALTERNATIVE: Direct Inline Scripts (without importing)
------------------------------------------------------

GET ACTIVE SERVER:
from AIAssistant import config
import unreal
from pathlib import Path

cfg = config.get_config()
active = cfg.get_active_server()

saved_dir = Path(unreal.Paths.project_saved_dir())
output_path = saved_dir / "AIConsole" / "server_status.txt"
output_path.parent.mkdir(parents=True, exist_ok=True)
output_path.write_text(active, encoding='utf-8')


SWITCH SERVER:
from AIAssistant import config
import unreal
from pathlib import Path

selected = '{0}'  # Inject via String Format
cfg = config.get_config()
success = cfg.switch_server(selected)

saved_dir = Path(unreal.Paths.project_saved_dir())
output_path = saved_dir / "AIConsole" / "server_switch_result.txt"
output_path.parent.mkdir(parents=True, exist_ok=True)

if success:
    result = f"success|{cfg.api_url}"
else:
    result = f"error|Invalid server: {selected}"

output_path.write_text(result, encoding='utf-8')


GET SERVER DISPLAY:
from AIAssistant import config
import unreal
from pathlib import Path

cfg = config.get_config()
active = cfg.get_active_server()
url = cfg.api_url
status = f"{active} → {url}"

saved_dir = Path(unreal.Paths.project_saved_dir())
output_path = saved_dir / "AIConsole" / "server_display.txt"
output_path.parent.mkdir(parents=True, exist_ok=True)
output_path.write_text(status, encoding='utf-8')
"""
