# AIAssistant v2.0 - Installation & Deployment Guide

## 📦 What's New

You now have a **complete modular AI assistant system** with:

- ✅ **Enhanced Context Collection**: Automatically captures lighting, materials, environment, selection data
- ✅ **Modular Architecture**: Clean separation of concerns, easy to extend
- ✅ **Async Support**: Non-blocking API calls keep editor responsive
- ✅ **Thread-Safe**: File-based communication works perfectly with Blueprint
- ✅ **Extensible**: Register custom actions from Blueprint or Python
- ✅ **Robust Error Handling**: Retry logic, timeouts, graceful degradation

## 🚀 Quick Installation (3 Steps)

### Step 1: Copy the Folder

Copy the entire `AIAssistant` folder to your UE5 project:

```
UE5_Assistant/Content/Python/AIAssistant/
```

Your folder structure should be:
```
Content/Python/AIAssistant/
├── __init__.py
├── core/              # Central entry points
│   ├── main.py
│   ├── config.py
│   └── utils.py
├── network/           # Communication modules
│   ├── http_polling_client.py
│   ├── websocket_client.py
│   ├── api_client.py
│   └── async_client.py
├── execution/         # Action system
│   ├── action_executor.py
│   ├── action_queue.py
│   └── action_executor_extensions.py
├── ui/                # Editor UI extensions
│   ├── toolbar_menu.py
│   └── ui_manager.py
├── collection/        # Metadata collectors
│   ├── context_collector.py
│   ├── file_collector.py
│   └── project_metadata_collector.py
├── system/            # System management
│   ├── auto_update.py
│   ├── startup.py
│   └── project_registration.py
├── tools/             # AI-assisted tools
│   ├── scene_orchestrator.py
│   ├── viewport_controller.py
│   ├── actor_manipulator.py
│   └── blueprint_capture.py
├── troubleshoot/      # Diagnostics tools
│   └── troubleshooter.py
└── documentation/     # Guides and docs
    ├── README.md
    ├── INSTALLATION.md (this file)
    └── ...
```

### Step 2: Update Your Blueprint

In your Editor Utility Widget Blueprint, find the **Format Text** node and change it:

**OLD:**
```python
import ai_command_console; ai_command_console.send_and_store('{0}')
```

**NEW:**
```python
import AIAssistant; AIAssistant.send_command('{0}')
```

That's the only Blueprint change needed! Everything else stays the same.

### Step 3: Test It

1. Open your Editor Utility Widget
2. Type: "what do I see?"
3. Click "Send Prompt To AI"
4. **Wait 15-20 seconds** (normal - API call in progress)
5. Response appears with full scene details

✅ Done!

**Note:** The brief wait is normal and ensures thread-safe operation. All UE API calls execute correctly on the main thread.

## 📋 Verification Checklist

- [ ] `AIAssistant` folder copied to `Content/Python/`
- [ ] Blueprint Format Text node updated
- [ ] Test command works ("what do I see?")
- [ ] Response appears in text box
- [ ] Response file created at `Saved/AIConsole/last_reply.txt`

## ⚙️ Configuration (Optional)

The system creates a config file at: `Saved/AIConsole/config.json`

Default settings:
```json
{
  "api_base_url": "https://ue5-assistant-noahbutcher97.replit.app",
  "model": "gpt-4o-mini",
  "timeout": 25,
  "max_retries": 3,
  "retry_delay": 2.5,
  "verbose_logging": false,
  "max_context_turns": 6
}
```

To enable debug logging:
```python
import AIAssistant; AIAssistant.get_config().set('verbose_logging', True)
```

## 🧠 Enhanced Features (Automatic)

These features work immediately, no setup needed:

### Smart Scene Analysis
- Camera position and rotation
- All actors with types and classes
- Lighting setup (directional, point, spot, sky)
- Material assignments
- Environment settings (fog, post-process)
- Selection details

### Quick Commands
- "what do I see?" → Full viewport description
- "describe scene" → Detailed scene analysis
- "list actors" → Actor inventory
- "selected" → Selection details

### Conversation Memory
The system remembers context across commands:
```
User: "what lights are in the scene?"
AI: "There are 3 directional lights..."

User: "make them brighter"
AI: [Remembers we're talking about lights]
```

## 🔧 Customization Examples

### Add Custom Actions from Blueprint

Execute this Python command in your Blueprint:
```python
import AIAssistant; exec('''
def my_custom_action():
    import unreal
    unreal.log("Custom action executed!")
    return "Custom action complete!"

AIAssistant.get_executor().register("my_custom_action", my_custom_action)
''')
```

### Adjust Timeout

If commands timeout, increase the limit:
```python
import AIAssistant; AIAssistant.get_config().set('timeout', 60)
```

## 🐛 Troubleshooting

### "Module 'AIAssistant' not found"

**Solution:** Verify folder is at correct location:
```
Content/Python/AIAssistant/  (not Content/Python/attached_assets/AIAssistant/)
```

### "requests library not available"

**Solution:** Install requests in UE Python:
```python
import subprocess, sys
subprocess.run([sys.executable, "-m", "pip", "install", "requests"])
```

### Response not appearing

**Solution:** Check these:
1. Look at UE Output Log for Python errors
2. Verify API server is running: https://ue5-assistant-noahbutcher97.replit.app
3. Check file exists: `Saved/AIConsole/last_reply.txt`
4. Enable verbose logging to see details

### Editor freezing

**Solution:** 
- Default async mode should prevent this
- If using `use_async=False`, expect 15-20 second wait
- Switch back to async: `AIAssistant.send_command(text)`

## 📚 Documentation Files

- **README.md** - Complete feature documentation
- **BLUEPRINT_INTEGRATION.md** - Detailed Blueprint guide with examples
- **INSTALLATION.md** - This file (quick start)

## 🎯 Next Steps

1. **Test the enhanced context**: Try "what do I see?" in a complex scene
2. **Experiment with commands**: Ask about lighting, materials, actors
3. **Add custom actions**: Extend the system for your workflow
4. **Configure settings**: Adjust timeout, logging, model as needed

## 📝 Migration Notes

If you were using the old v1.0 system:

### What Changed
- ✅ One line Blueprint update (import statement)
- ✅ File paths remain the same
- ✅ Much smarter context collection
- ✅ Extensible action system
- ✅ Better error handling

### What Stayed the Same
- ✅ Response file location: `Saved/AIConsole/last_reply.txt`
- ✅ Conversation log: `Saved/AIConsole/conversation_log.txt`
- ✅ Blueprint file reading logic
- ✅ Widget UI and layout

## ✅ Success Indicators

You'll know it's working when:

1. ✅ Command executes without Python errors
2. ✅ Response file updates with AI text
3. ✅ Blueprint displays response in text box
4. ✅ Descriptions include lighting, materials, actors
5. ✅ Editor stays responsive during processing

## 🔗 Support Resources

- Check UE Output Log for Python errors
- Enable verbose logging for detailed diagnostics
- Review conversation log at `Saved/AIConsole/conversation_log.txt`
- Verify API server status: https://ue5-assistant-noahbutcher97.replit.app

---

**Version:** 2.0.0 (October 2025)  
**Author:** Noah Butcher  
**Architecture:** Modular Python with Blueprint integration
