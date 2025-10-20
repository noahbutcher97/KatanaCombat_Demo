# Thread Safety and Async Limitations

## ⚠️ Critical Issue: Async Mode Thread Safety

**Status:** Async mode is **NOT RECOMMENDED** due to Unreal Engine thread safety requirements.

## The Problem

Unreal Engine requires all API calls to execute on the **main/game thread**. The async implementation uses background threads for network requests, but the callback attempts to:

1. Execute context collection (camera, actors, lighting) - **❌ Fails off main thread**
2. Write response files via `unreal.Paths` - **❌ Fails off main thread**  
3. Execute UE_REQUEST actions - **❌ Fails off main thread**

### Error Logs from UE
```
[ContextCollector] ⚠️ Camera data unavailable: UnrealEditorSubsystem: 
    Attempted to access Unreal API from outside the main game thread.
[ContextCollector] ⚠️ Actor data unavailable: EditorActorSubsystem: 
    Attempted to access Unreal API from outside the main game thread.
[AIAssistant] ❌ Async callback failed: Paths: 
    Attempted to access Unreal API from outside the main game thread.
```

## Current Solution: Sync Mode (Default)

**The system now defaults to synchronous execution:**

```python
AIAssistant.send_command("what do I see?")  # use_async=False by default
```

### Sync Mode Characteristics
- ✅ **Thread-safe**: All UE API calls on main thread
- ✅ **Reliable**: No thread errors or failed context collection
- ✅ **Complete data**: Camera, actors, lighting, materials all collected
- ⚠️ **Blocks editor**: 15-20 second wait during API call
- ✅ **Blueprint compatible**: File-based communication works perfectly

### Why Sync Works with Blueprint

Your Editor Utility Widget uses file-based communication:
1. Button clicked → Execute Python
2. Python writes response to file (sync, blocks ~15-20s)
3. Blueprint reads file
4. Display response

The "blocking" happens in Python, but Blueprint can handle it. The file gets written when ready.

## Future: Proper Async Implementation

To implement true async without thread errors requires:

### Option 1: Tick-Based Polling (Recommended)
```python
# Background thread stores result in queue
result_queue = Queue()

def async_callback(response):
    result_queue.put(response)  # Thread-safe queue

# Main thread ticker polls queue (runs on game thread)
def on_tick():
    if not result_queue.empty():
        response = result_queue.get()
        # Now safe to call UE APIs
        write_response_file(response)
        execute_ue_actions(response)
```

### Option 2: Unreal's AsyncTask
```python
# Use UE's built-in async system
class MyAsyncTask(unreal.AsyncTask):
    def do_work(self):
        # Background thread - network only
        return make_api_call()
    
    def on_complete(self, result):
        # Main thread callback - safe for UE APIs
        process_response(result)
```

### Option 3: Python asyncio with Game Thread Callbacks
```python
import asyncio

async def async_command(user_input):
    # Network call in background
    response = await make_api_call(user_input)
    
    # Marshal back to main thread
    unreal.PythonScriptLibrary.execute_python_command_on_game_thread(
        f"process_response('{response}')"
    )
```

## Recommendation

**Use sync mode (current default).** The 15-20 second blocking is acceptable for:
- Editor tools (not runtime)
- User-initiated commands (they expect to wait)
- File-based Blueprint pattern (already handles async at UI level)

If you need true non-blocking async:
- Implement tick-based polling in UE
- Use Unreal's AsyncTask system
- Requires UE-specific code (can't be done in pure Python)

## Current Configuration

```python
# Default in main.py
def send_command(user_input: str, use_async: bool = False) -> str:
    # use_async=False is default and recommended
```

```python
# Explicitly use sync mode
AIAssistant.send_command("what do I see?")  # Safe, reliable

# Experimental async (has thread errors)
AIAssistant.send_command("what do I see?", use_async=True)  # Not recommended
```

## Summary

| Mode | Thread Safety | UE APIs | Performance | Recommended |
|------|--------------|---------|-------------|-------------|
| **Sync (default)** | ✅ Safe | ✅ Works | ⚠️ Blocks 15-20s | ✅ **Yes** |
| **Async** | ❌ Thread errors | ❌ Fails | ✅ Non-blocking | ❌ No |
| **Future: UE Async** | ✅ Safe | ✅ Works | ✅ Non-blocking | 🔮 Future |

**Bottom line:** Use the default sync mode. It's reliable and safe.
