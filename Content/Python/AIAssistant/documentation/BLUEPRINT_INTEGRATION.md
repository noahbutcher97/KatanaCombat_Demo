# Blueprint Integration Guide - Complete Reference

> **Updated for v3.1** - Comprehensive node-by-node examples with real paths for easy copy-paste

## 📋 Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [File Communication Protocol](#file-communication-protocol)
3. [Server Management](#server-management)
4. [AI Commands & Viewport Analysis](#ai-commands--viewport-analysis)
5. [File System Operations](#file-system-operations)
6. [Project Intelligence](#project-intelligence)
7. [Blueprint Screenshot Capture](#blueprint-screenshot-capture)
8. [Complete Blueprint Examples](#complete-blueprint-examples)

---

## Architecture Overview

### System Components

```
Your UE5 Project (D:\UnrealProjects\5.6\UE5_Assistant\)
├── Content/
│   └── Python/
│       └── AIAssistant/              # Python modules
│           ├── main.py
│           ├── config.py
│           ├── blueprint_helpers.py  # Blueprint communication
│           └── ...
├── Saved/
│   └── AIConsole/                    # File-based I/O
│       ├── server_status.txt
│       ├── command_result.txt
│       └── ...
└── Config/
    └── AIAssistant_config.json       # Persistent settings

Backend: https://ue5-assistant-noahbutcher97.replit.app
```

### How It Works

1. **Blueprint** → Execute Python Script node
2. **Python** → Processes request, writes result to `Saved/AIConsole/*.txt`
3. **Blueprint** → Add 0.1s Delay, then Load File to String
4. **Blueprint** → Parse result and update UI

---

## File Communication Protocol

All Python operations write results to standardized files in:
```
D:\UnrealProjects\5.6\UE5_Assistant\Saved\AIConsole\
```

### File Path Convention

**In Blueprint "Load File to String" nodes, use:**
```
D:/UnrealProjects/5.6/UE5_Assistant/Saved/AIConsole/[filename].txt
```
⚠️ **Use forward slashes** `/` not backslashes `\`

### Standard Output Files

| Operation | Output File | Format |
|-----------|-------------|--------|
| Server status | `server_status.txt` | `"production"` |
| Server switch | `server_switch_result.txt` | `"success\|URL"` |
| AI command | `command_result.txt` | `"success\|response"` |
| File list | `file_list_result.txt` | `"success\|file1,file2"` |
| File read | `file_read_result.txt` | `"success\|content"` |
| Project info | `project_info_result.txt` | `"success\|JSON"` |
| Blueprint capture | `blueprint_capture_result.txt` | `"success\|capture_id"` |

---

## Server Management

### 1. Get Current Server

**Blueprint Flow:**
```
Event Construct
  ↓
[Execute Python Script]
  └─ Python Command: (see below)
  ↓
[Delay] Duration: 0.1
  ↓
[Load File to String]
  └─ File Name: D:/UnrealProjects/5.6/UE5_Assistant/Saved/AIConsole/server_status.txt
  ↓
[Print String] or [Set Text (TextBlock)]
```

**Python Command (copy-paste into Execute Python Script):**
```python
from AIAssistant.tools import blueprint_helpers
blueprint_helpers.get_active_server()
```

**Output:** `production` or `dev` or `localhost`

---

### 2. Switch Server

**Blueprint Flow:**
```
[Button Clicked] or [ComboBox Selection Changed]
  ↓
[Get Selected Option] (from ComboBox) → Selected Server
  ↓
[Format String] 
  └─ Format: "{0}"
  └─ String 0: [Selected Server]
  ↓
[Execute Python Script]
  └─ Python Command: (see below with {0})
  ↓
[Delay] Duration: 0.1
  ↓
[Load File to String]
  └─ File Name: D:/UnrealProjects/5.6/UE5_Assistant/Saved/AIConsole/server_switch_result.txt
  ↓
[Split String]
  └─ String: [File Content]
  └─ Delimiter: "|"
  └─ Result: [Status, URL]
  ↓
[Branch] Condition: Status == "success"
  TRUE → [Set Text] "✅ Connected to {URL}"
  FALSE → [Set Text] "❌ Error: {URL}"
```

**Python Command (copy-paste into Execute Python Script):**
```python
from AIAssistant.tools import blueprint_helpers
blueprint_helpers.switch_server('{0}')
```

**Available Servers:**
- `production` → https://ue5-assistant-noahbutcher97.replit.app
- `dev` → Development server URL
- `localhost` → http://localhost:5000

---

### 3. Get Server Display (with URL)

**Python Command:**
```python
from AIAssistant.tools import blueprint_helpers
blueprint_helpers.get_server_display()
```

**File to Read:** `D:/UnrealProjects/5.6/UE5_Assistant/Saved/AIConsole/server_display.txt`

**Output:** `production → https://ue5-assistant-noahbutcher97.replit.app`

---

## AI Commands & Viewport Analysis

### 1. Send AI Command (Text Only)

**Blueprint Flow:**
```
[Text Input Box] → User Input
  ↓
[Format String]
  └─ Format: "{0}"
  └─ String 0: [User Input]
  ↓
[Execute Python Script]
  └─ Python Command: (see below)
  ↓
[Delay] Duration: 0.5
  ↓
[Load File to String]
  └─ File Name: D:/UnrealProjects/5.6/UE5_Assistant/Saved/AIConsole/command_result.txt
  ↓
[Split String]
  └─ Delimiter: "|"
  └─ Result: [Status, Response]
  ↓
[Branch] Status == "success"
  TRUE → [Set Text (Response Box)] to [Response]
  FALSE → [Set Text (Response Box)] to "❌ {Response}"
```

**Python Command:**
```python
from AIAssistant.tools import blueprint_helpers
blueprint_helpers.execute_ai_command('{0}')
```

---

### 2. Viewport Description with Context

**Python Command (with viewport context):**
```python
from AIAssistant.tools import blueprint_helpers
blueprint_helpers.describe_viewport_with_context('{0}')
```

**Output File:** `D:/UnrealProjects/5.6/UE5_Assistant/Saved/AIConsole/viewport_result.txt`

**Format:** `success|[AI-generated viewport description]`

---

## File System Operations

### 1. List Files in Directory

**Blueprint Flow:**
```
[Text Input] → Directory Path (e.g., "Content/Blueprints")
  ↓
[Format String] "{0}"
  ↓
[Execute Python Script]
  └─ Python Command: (see below)
  ↓
[Delay] 0.2
  ↓
[Load File to String]
  └─ File: D:/UnrealProjects/5.6/UE5_Assistant/Saved/AIConsole/file_list_result.txt
  ↓
[Split String] Delimiter: "|"
  └─ [Status, FileList]
  ↓
[Branch] Status == "success"
  TRUE → [Split String] FileList by ","
         ↓
         [ForEachLoop] → Add each file to List View
  FALSE → [Print String] "Error: {FileList}"
```

**Python Command:**
```python
from AIAssistant.tools import blueprint_helpers
blueprint_helpers.list_files('{0}')
```

**Example Paths:**
- `Content/Blueprints`
- `Source/UE5_Assistant`
- `Content/Maps`

---

### 2. Read File Contents

**Blueprint Flow:**
```
[Text Input] → File Path (e.g., "Content/Blueprints/MyBP.uasset")
  ↓
[Format String] "{0}"
  ↓
[Execute Python Script]
  └─ Python Command: (see below)
  ↓
[Delay] 0.2
  ↓
[Load File to String]
  └─ File: D:/UnrealProjects/5.6/UE5_Assistant/Saved/AIConsole/file_read_result.txt
  ↓
[Split String] Delimiter: "|"
  └─ [Status, Content]
  ↓
[Branch] Status == "success"
  TRUE → [Set Text (Multiline)] to [Content]
  FALSE → [Set Text] "❌ {Content}"
```

**Python Command:**
```python
from AIAssistant.tools import blueprint_helpers
blueprint_helpers.read_file('{0}')
```

---

### 3. Search Files by Pattern

**Blueprint Flow:**
```
[Text Input] → Search Pattern (e.g., "*.cpp")
  ↓
[Format String] "{0}"
  ↓
[Execute Python Script]
  └─ Python Command: (see below)
  ↓
[Delay] 0.3
  ↓
[Load File to String]
  └─ File: D:/UnrealProjects/5.6/UE5_Assistant/Saved/AIConsole/file_search_result.txt
  ↓
[Split String] "|" → [Status, Results]
  ↓
[Split String] "," → [File Array]
  ↓
[ForEachLoop] → Display results
```

**Python Command:**
```python
from AIAssistant.tools import blueprint_helpers
blueprint_helpers.search_files('{0}')
```

**Example Patterns:**
- `*.cpp` - All C++ files
- `*.uasset` - All asset files
- `BP_*.uasset` - Blueprints starting with BP_

---

## Project Intelligence

### 1. Get Project Metadata

**Blueprint Flow:**
```
[Button - "Show Project Info"]
  ↓
[Execute Python Script]
  └─ Python Command: (see below)
  ↓
[Delay] 0.5
  ↓
[Load File to String]
  └─ File: D:/UnrealProjects/5.6/UE5_Assistant/Saved/AIConsole/project_info_result.txt
  ↓
[Split String] "|" → [Status, JSON]
  ↓
[Branch] Status == "success"
  TRUE → [Parse JSON] → Extract fields
         ↓
         [Set Text] "Project: {project_name}"
         [Set Text] "Engine: {engine_version}"
         [Set Text] "Modules: {module_count}"
  FALSE → [Print String] "Error"
```

**Python Command:**
```python
from AIAssistant.tools import blueprint_helpers
blueprint_helpers.get_project_info()
```

**Output Contains:**
- Project name
- Engine version
- Module list
- Plugin list
- Asset counts
- Blueprint statistics

---

### 2. Request Implementation Guidance

**Blueprint Flow:**
```
[Text Input] → Feature Request (e.g., "How do I add a jump mechanic?")
  ↓
[Format String] "{0}"
  ↓
[Execute Python Script]
  └─ Python Command: (see below)
  ↓
[Delay] 1.0
  ↓
[Load File to String]
  └─ File: D:/UnrealProjects/5.6/UE5_Assistant/Saved/AIConsole/guidance_result.txt
  ↓
[Split String] "|" → [Status, Guidance]
  ↓
[Set Text (Multiline Rich Text)]
```

**Python Command:**
```python
from AIAssistant.tools import blueprint_helpers
blueprint_helpers.request_guidance('{0}')
```

**Example Requests:**
- "How do I implement a health system?"
- "Best way to add AI pathfinding?"
- "Setup multiplayer replication for this project"

---

## Blueprint Screenshot Capture

### 1. Capture Active Blueprint Editor

**Blueprint Flow:**
```
[Button - "Capture Blueprint"]
  ↓
[Execute Python Script]
  └─ Python Command: (see below)
  ↓
[Delay] 1.0
  ↓
[Load File to String]
  └─ File: D:/UnrealProjects/5.6/UE5_Assistant/Saved/AIConsole/blueprint_capture_result.txt
  ↓
[Split String] "|" → [Status, CaptureID]
  ↓
[Branch] Status == "success"
  TRUE → [Set Text] "✅ Captured! ID: {CaptureID}"
  FALSE → [Set Text] "❌ {CaptureID}"
```

**Python Command:**
```python
from AIAssistant.tools import blueprint_helpers
blueprint_helpers.capture_blueprint()
```

**Output:** `success|bp_capture_20251011_143022`

**Screenshot saved to:**
```
D:/UnrealProjects/5.6/UE5_Assistant/Saved/Screenshots/AIAssistant/
```

---

### 2. List All Blueprint Captures

**Python Command:**
```python
from AIAssistant.tools import blueprint_helpers
blueprint_helpers.list_blueprints()
```

**Output File:** `D:/UnrealProjects/5.6/UE5_Assistant/Saved/AIConsole/blueprint_list_result.txt`

**Format:** `success|BP_Character,BP_GameMode,BP_PlayerController`

---

## Complete Blueprint Examples

### Example 1: Full AI Assistant Panel

**Widget Hierarchy:**
```
Canvas Panel
├── Vertical Box (Main Container)
│   ├── ComboBox (Server Selector) - Name: ServerCombo
│   ├── Text (Server Status) - Name: ServerStatusText
│   ├── Horizontal Box (Input Row)
│   │   ├── Editable Text (User Input) - Name: InputField
│   │   └── Button (Send) - Name: SendButton
│   └── Editable Text (Multiline) (Response) - Name: ResponseBox
```

**Event Graph Nodes:**

**1. Event Construct:**
```
Event Construct
  ↓
[Clear Options] → ServerCombo
  ↓
[Add Option] → ServerCombo, "production"
  ↓
[Add Option] → ServerCombo, "dev"
  ↓
[Add Option] → ServerCombo, "localhost"
  ↓
[Execute Python Script]
────────────────────────────────────────────────
from AIAssistant.tools import blueprint_helpers
blueprint_helpers.get_active_server()
────────────────────────────────────────────────
  ↓
[Delay] 0.1
  ↓
[Load File to String]
  └─ File: D:/UnrealProjects/5.6/UE5_Assistant/Saved/AIConsole/server_status.txt
  ↓
[Set Selected Option] → ServerCombo, [File Content]
```

**2. Server Selection Changed:**
```
[On Selection Changed] → ServerCombo
  ↓
[Get Selected Option] → SelectedServer
  ↓
[Format String] "{0}" with [SelectedServer]
  ↓
[Execute Python Script]
────────────────────────────────────────────────
from AIAssistant.tools import blueprint_helpers
blueprint_helpers.switch_server('{0}')
────────────────────────────────────────────────
  ↓
[Delay] 0.1
  ↓
[Load File to String]
  └─ File: D:/UnrealProjects/5.6/UE5_Assistant/Saved/AIConsole/server_switch_result.txt
  ↓
[Split String] "|" → [Status, Message]
  ↓
[Branch] Status == "success"
  TRUE → [Append] "✅ Connected to " + Message
         ↓
         [Set Text] → ServerStatusText
  FALSE → [Set Text] → ServerStatusText, "❌ Error: " + Message
```

**3. Send Button Clicked:**
```
[On Clicked] → SendButton
  ↓
[Get Text] → InputField → UserInput
  ↓
[Format String] "{0}" with [UserInput]
  ↓
[Execute Python Script]
────────────────────────────────────────────────
from AIAssistant.tools import blueprint_helpers
blueprint_helpers.execute_ai_command('{0}')
────────────────────────────────────────────────
  ↓
[Delay] 0.5
  ↓
[Load File to String]
  └─ File: D:/UnrealProjects/5.6/UE5_Assistant/Saved/AIConsole/command_result.txt
  ↓
[Split String] "|" → [Status, Response]
  ↓
[Branch] Status == "success"
  TRUE → [Set Text] → ResponseBox, [Response]
  FALSE → [Set Text] → ResponseBox, "❌ Error: " + [Response]
```

---

### Example 2: File Browser Widget

**Widget Elements:**
- `Text Input` (Directory) - Name: DirInput
- `Button` (Browse) - Name: BrowseButton
- `List View` (Files) - Name: FileListView

**Browse Button Clicked:**
```
[On Clicked] → BrowseButton
  ↓
[Get Text] → DirInput → DirectoryPath
  ↓
[Format String] "{0}" with [DirectoryPath]
  ↓
[Execute Python Script]
────────────────────────────────────────────────
from AIAssistant.tools import blueprint_helpers
blueprint_helpers.list_files('{0}')
────────────────────────────────────────────────
  ↓
[Delay] 0.2
  ↓
[Load File to String]
  └─ File: D:/UnrealProjects/5.6/UE5_Assistant/Saved/AIConsole/file_list_result.txt
  ↓
[Split String] "|" → [Status, FileList]
  ↓
[Branch] Status == "success"
  TRUE → [Split String] FileList by ","
         ↓
         [Clear List Items] → FileListView
         ↓
         [ForEachLoop] Array: [Files]
           ↓
           [Add Item] → FileListView, [File]
  FALSE → [Print String] "Error: " + FileList
```

---

### Example 3: Blueprint Capture Button

**Single Node Setup:**
```
[Button Clicked] → CaptureButton
  ↓
[Execute Python Script]
────────────────────────────────────────────────
from AIAssistant.tools import blueprint_helpers
blueprint_helpers.capture_blueprint()
────────────────────────────────────────────────
  ↓
[Delay] 1.0
  ↓
[Load File to String]
  └─ File: D:/UnrealProjects/5.6/UE5_Assistant/Saved/AIConsole/blueprint_capture_result.txt
  ↓
[Split String] "|" → [Status, ID]
  ↓
[Branch] Status == "success"
  TRUE → [Print String] "Screenshot saved! ID: " + ID
  FALSE → [Print String] "Capture failed: " + ID
```

---

## 🔧 Configuration

### Enable/Disable Features

Edit: `D:\UnrealProjects\5.6\UE5_Assistant\Config\AIAssistant_config.json`

```json
{
  "feature_flags": {
    "allow_file_read": true,
    "allow_file_list": true,
    "allow_file_search": true,
    "allow_guidance": true,
    "allow_blueprint_capture": true,
    "allow_project_info": true,
    "allow_viewport_context": true
  }
}
```

---

## 🐛 Troubleshooting

### File Not Found Errors

**Problem:** Load File to String returns error  
**Solution:** Check path uses **forward slashes**:
```
✅ D:/UnrealProjects/5.6/UE5_Assistant/Saved/AIConsole/file.txt
❌ D:\UnrealProjects\5.6\UE5_Assistant\Saved\AIConsole\file.txt
```

### Empty Results

**Problem:** File loads but contains no data  
**Solution:** 
1. Increase Delay node duration (0.1 → 0.3 seconds)
2. Check Python Output Log for errors
3. Verify feature flags are enabled in config

### Server Connection Fails

**Problem:** Server switch returns error  
**Solution:**
1. Check internet connection
2. Verify server URL in config.py
3. Test backend at: https://ue5-assistant-noahbutcher97.replit.app/health

### Python Import Errors

**Problem:** "No module named AIAssistant"  
**Solution:**
1. Verify AIAssistant folder is at: `Content/Python/AIAssistant/`
2. Restart Unreal Editor
3. Check Python paths in Project Settings

---

## 📝 Quick Reference

### All Python Commands

```python
# Server Management
from AIAssistant.tools import blueprint_helpers
blueprint_helpers.get_active_server()
blueprint_helpers.switch_server('{0}')
blueprint_helpers.get_server_display()

# AI Commands
blueprint_helpers.execute_ai_command('{0}')
blueprint_helpers.describe_viewport_with_context('{0}')
blueprint_helpers.request_guidance('{0}')

# File Operations
blueprint_helpers.list_files('{0}')
blueprint_helpers.read_file('{0}')
blueprint_helpers.search_files('{0}')

# Project Intelligence
blueprint_helpers.get_project_info()

# Blueprint Operations
blueprint_helpers.capture_blueprint()
blueprint_helpers.list_blueprints()
```

### All Output Files

Base Path: `D:/UnrealProjects/5.6/UE5_Assistant/Saved/AIConsole/`

- `server_status.txt`
- `server_switch_result.txt`
- `server_display.txt`
- `command_result.txt`
- `viewport_result.txt`
- `guidance_result.txt`
- `file_list_result.txt`
- `file_read_result.txt`
- `file_search_result.txt`
- `project_info_result.txt`
- `blueprint_capture_result.txt`
- `blueprint_list_result.txt`

---

**Last Updated:** October 2025 | **Version:** 3.1
