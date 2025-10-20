# UE5 AI Assistant - Connection Troubleshooting Guide

## Quick Reconnection Methods

### Method 1: Troubleshooting Tools (Recommended)
The easiest way to troubleshoot connection issues:

```python
# In UE5 Python Console:
import AIAssistant.troubleshoot.troubleshooter as ts

# Available commands:
ts.reconnect()      # Restart HTTP polling connection
ts.test_server()    # Test server connectivity
ts.status()         # Show connection status
ts.info()           # Display connection details
ts.dashboard()      # Open web dashboard in browser
ts.help()           # Show all commands
```

### Method 2: Direct Restart
Quick one-liner to restart the connection:

```python
# In UE5 Python Console:
import AIAssistant.core.main as assistant
assistant.restart_assistant()
```

### Method 3: Full Reload
If the assistant is stuck or not responding:

```python
# In UE5 Python Console:
import sys

# Clear cached modules
if 'AIAssistant' in sys.modules:
    del sys.modules['AIAssistant']

# Reimport and restart
import AIAssistant.core.main as assistant
assistant.get_assistant()
```

## Common Issues

### Issue: Client Not Showing as Connected in Dashboard

**Symptoms:**
- Dashboard shows 0 connected clients
- Widget generation doesn't work
- File Drop tool fails

**Solution:**
1. Open UE5 Python Console
2. Run: `import AIAssistant.troubleshoot.troubleshooter as ts`
3. Run: `ts.test_server()`
4. If server is reachable, run: `ts.reconnect()`

### Issue: Connection Dropped After Several Hours

**Cause:** HTTP polling thread stopped due to network timeout or exception

**Solution:**
```python
import AIAssistant.core.main as assistant
assistant.restart_assistant()
```

### Issue: "AI Assistant not loaded" Error

**Solution:**
```python
# Reload the entire assistant
import sys
for mod in list(sys.modules.keys()):
    if 'AIAssistant' in mod:
        del sys.modules[mod]

# Re-initialize
from AIAssistant.startup import configure_and_start
configure_and_start()
```

## Troubleshooting Tools Reference

### ts.reconnect()
- Stops current polling thread
- Re-establishes HTTP connection
- Resumes polling every 2 seconds
- Shows status in Output Log

### ts.test_server()
- Sends GET request to backend
- Verifies server is reachable
- Shows timeout or connection errors
- Displays result in console and Output Log

### ts.status()
- Shows current connection status
- Indicates if polling is active
- Quick yes/no connected state

### ts.info()
- Backend URL
- Project ID (truncated)
- Project Name
- Connection status
- Poll interval settings
- Available commands

### ts.dashboard()
- Launches default web browser
- Opens dashboard at backend URL
- Provides visual status monitoring

### ts.help()
- Shows all available commands
- Displays usage examples
- Quick reference guide

## Installation Notes

The troubleshooting tools are automatically installed with the AI Assistant client. After installation:

1. Restart UE5 to load the assistant
2. Check Output Log for initialization messages
3. Use troubleshooter if connection issues occur

## Dashboard Integration

The troubleshooting tools work seamlessly with the web dashboard:
- Dashboard shows real-time connection status
- Live Feed displays client activity
- Widget Generator queues commands for UE5
- File Drop tool tests file writing

For persistent connection issues, check:
1. Firewall settings (allow HTTPS to replit.app)
2. Network stability
3. UE5 Python environment status
