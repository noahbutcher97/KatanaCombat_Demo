# UE5 AI Assistant - Python Client

Complete Python client for integrating AI-powered viewport descriptions into Unreal Engine 5.6+

## 📁 Files Overview

| File | Purpose |
|------|---------|
| `main.py` | Main orchestrator - entry point for running the AI assistant |
| `config.py` | Configuration management with server selection |
| `api_client.py` | HTTP client for backend communication |
| `context_collector.py` | Collects viewport data (camera, actors, lighting, etc.) |
| `action_executor.py` | Executes AI commands and processes responses |
| `ui_manager.py` | Manages UI display and notifications |
| `blueprint_helpers.py` | ⭐ **NEW** - Blueprint integration with file-based I/O |

## 🚀 Quick Start

### 1. Copy to UE5 Project
```bash
# Copy entire AIAssistant directory to:
[YourProject]/Content/Python/AIAssistant/
```

### 2. Run in UE5 Python Console
```python
from AIAssistant import main
main.run_assistant()
```

### 3. Switch Servers (Optional)
```python
from AIAssistant import config
cfg = config.get_config()

# List available servers
cfg.list_servers()

# Switch to dev server
cfg.switch_server("dev")

# Switch to production
cfg.switch_server("production")
```

## 🎛️ Server Selection

The system supports 3 server presets:

| Server | URL | When to Use |
|--------|-----|-------------|
| **production** | `https://ue5-assistant-noahbutcher97.replit.app` | Stable deployment |
| **dev** | `https://...-janeway.replit.dev` | Test new features |
| **localhost** | `http://localhost:5000` | Local development |

**Default:** `production`

## 📘 Blueprint Integration

See **`BLUEPRINT_INTEGRATION.md`** for complete guide on adding server selection to Editor Utility Widgets.

### Quick Example - Get Active Server
```python
# In Blueprint "Execute Python Script" node:
from AIAssistant import blueprint_helpers
blueprint_helpers.get_active_server()

# Then use "Load File to String":
# [Project]/Saved/AIConsole/server_status.txt
```

## 📂 Output Files

All Blueprint communication files are written to:
```
[YourProject]/Saved/AIConsole/
├── config.json               # Persistent configuration
├── server_status.txt         # Current active server
├── server_switch_result.txt  # Server switch result
├── server_display.txt        # Display string for UI
└── server_list.txt          # Available servers list
```

## 🔧 Configuration

Edit settings programmatically:
```python
from AIAssistant import config
cfg = config.get_config()

# Change AI model
cfg.set("model", "gpt-4o")

# Change response style
cfg.set("response_style", "technical")

# Enable verbose logging
cfg.set("verbose_logging", True)
```

Or edit directly: `[Project]/Saved/AIConsole/config.json`

## 📚 Documentation

- **`Documentation/`** - Complete technical documentation
  - **`BLUEPRINT_INTEGRATION.md`** - Server selection guide for Blueprint
  - Plus 7 other technical guides
- **`tests/DEBUG_TEST.py`** - Test utilities and examples
- **Backend Docs**: See `/app/Documentation/` for API reference

## 🛠️ Troubleshooting

**Can't import AIAssistant?**
- Ensure directory is in UE5 Python path: `Content/Python/AIAssistant/`

**Server connection fails?**
- Check server URL with `cfg.list_servers()`
- Verify internet connection
- Try switching servers: `cfg.switch_server("production")`

**Blueprint can't read files?**
- Always add 0.1s Delay between Python execution and file reading
- Check file paths use forward slashes
- Verify files exist in `[Project]/Saved/AIConsole/`

## 🔄 Version

**v3.0** - Modular architecture with server selection and Blueprint integration

---

**Author:** Noah Butcher  
**Compatible:** Unreal Engine 5.6+ (Python 3.11.8)
