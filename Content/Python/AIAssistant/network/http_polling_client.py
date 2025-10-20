"""
HTTP Polling client for UE5 - Fallback when WebSocket doesn't work.
Provides same interface as WebSocketClient for seamless fallback.
Now with thread-safe action execution via queue system.
"""
import sys
import threading
import time
from typing import Any, Callable, Dict, Optional

import requests

# Import action queue for thread-safe execution
try:
    from ..execution.action_queue import get_action_queue
    HAS_ACTION_QUEUE = True
except ImportError:
    HAS_ACTION_QUEUE = False
    print("⚠️ Action queue not available - using direct execution")


class HTTPPollingClient:
    """
    HTTP Polling client for UE5 to connect to the backend.
    Fallback for when WebSocket connections are blocked.
    Now with thread-safe action execution via queue system.
    """

    def __init__(self,
                 base_url: str,
                 project_id: str,
                 project_name: str = "Unknown"):
        """
        Initialize HTTP Polling client.

        Args:
            base_url: Backend base URL (e.g., "https://ue5-assistant-noahbutcher97.replit.app")
            project_id: Unique project identifier
            project_name: Human-readable project name
        """
        self.base_url = base_url.rstrip('/')
        self.project_id = project_id
        self.project_name = project_name

        self.connected = False
        self.running = False
        self.poll_thread = None
        self.action_handler: Optional[Callable[[str, Dict[str, Any]],
                                               Dict[str, Any]]] = None

        # Polling settings
        self.poll_interval = 2.0  # seconds
        self.heartbeat_interval = 10.0  # seconds
        self.last_heartbeat = 0

        # Auto-reconnect settings
        self.consecutive_failures = 0
        self.max_consecutive_failures = 3
        self.reconnect_delay = 5.0  # seconds
        self._reconnect_requested = False  # Flag for dashboard-triggered reconnect

        # Version tracking for module reload
        self.last_module_version = None
        self.last_version_check = 0
        self.version_check_interval = 10  # Check every 10 seconds

        # Initialize action queue if available
        if HAS_ACTION_QUEUE:
            self.action_queue = get_action_queue()  # type: ignore
            print("✅ Using thread-safe action queue for execution")
        else:
            self.action_queue = None
            print(
                "⚠️ Action queue not available - direct execution (may cause threading issues)"
            )

    def set_action_handler(self, handler: Callable[[str, Dict[str, Any]],
                                                   Dict[str, Any]]):
        """
        Set the handler for incoming action commands.

        Args:
            handler: Function that takes (action, params) and returns response dict
        """
        self.action_handler = handler

        # Also register with action queue if available
        if self.action_queue:
            self.action_queue.set_action_handler(handler)
            print("✅ Action handler registered with thread-safe queue")

    def connect(self) -> bool:
        """
        Connect to the backend via HTTP polling.

        Returns:
            True if connected successfully, False otherwise
        """
        try:
            print(f"🔌 Attempting HTTP polling connection to: {self.base_url}")

            # Register with backend
            url = f"{self.base_url}/api/ue5/register_http"
            payload = {
                "project_id": self.project_id,
                "project_name": self.project_name
            }
            print(f"[HTTP] Making registration POST request to: {url}")
            print(f"[HTTP] Payload: {payload}")

            # Log just before making the request
            print("[HTTP] About to send POST request with requests library...")
            try:
                response = requests.post(url, json=payload, timeout=5)
                print("[HTTP] Request completed! Got response object")
            except requests.exceptions.Timeout:
                print("[HTTP] ❌ Request timed out after 5 seconds!")
                raise
            except requests.exceptions.ConnectionError as ce:
                print(f"[HTTP] ❌ Connection error: {ce}")
                raise
            except requests.exceptions.RequestException as re:
                print(f"[HTTP] ❌ Request exception: {re}")
                raise

            print(
                f"[HTTP] Registration response status: {response.status_code}")
            print(
                f"[HTTP] Registration response text: {response.text[:200] if response.text else 'No response body'}"
            )

            if response.status_code == 200:
                result = response.json()
                if result.get("success"):
                    self.connected = True

                    # Start action queue ticker if available
                    if self.action_queue:
                        self.action_queue.start_ticker()

                    # Start polling thread
                    self.running = True
                    self.poll_thread = threading.Thread(target=self._poll_loop,
                                                        daemon=True)
                    self.poll_thread.start()

                    print(f"✅ HTTP polling connected: {self.project_id}")
                    return True
                else:
                    print(
                        f"❌ HTTP polling registration failed: {result.get('error', 'Unknown error')}"
                    )
                    return False
            else:
                print(
                    f"❌ HTTP polling registration failed: {response.status_code} - {response.text}"
                )
                return False

        except requests.exceptions.RequestException as e:
            print(f"❌ HTTP polling connection error (network): {e}")
            return False
        except Exception as e:
            print(f"❌ HTTP polling connection error: {e}")
            import traceback
            traceback.print_exc()
            return False

    def _poll_loop(self):
        """Main polling loop - runs in background thread."""
        print("[HTTPPolling] Background polling thread started")

        # Import critical modules locally to survive module cache clearing
        import requests

        while self.running:
            try:
                # Check for module updates periodically
                current_time = time.time()
                if current_time - self.last_version_check > self.version_check_interval:
                    self._check_module_version()
                    self.last_version_check = current_time

                # Send heartbeat periodically
                if current_time - self.last_heartbeat > self.heartbeat_interval:
                    self._send_heartbeat()
                    self.last_heartbeat = current_time

                # Poll for commands (include project_name for auto-registration)
                response = requests.post(f"{self.base_url}/api/ue5/poll",
                                         json={
                                             "project_id": self.project_id,
                                             "project_name": self.project_name
                                         },
                                         timeout=5)

                if response.status_code == 200:
                    data = response.json()
                    commands = data.get("commands", [])

                    # Check if we're still registered
                    if not data.get("registered", True):
                        print(
                            "⚠️ Server doesn't recognize us - re-registering..."
                        )
                        self._attempt_reconnect()
                        continue

                    # Process each command
                    for cmd in commands:
                        self._handle_command(cmd)

                    # Check if reconnect was requested via dashboard
                    if self._reconnect_requested:
                        self._reconnect_requested = False  # Clear flag
                        print("🔌 Executing dashboard-triggered reconnect...")
                        # Simply re-register without spawning new threads
                        try:
                            re_reg_response = requests.post(
                                f"{self.base_url}/api/ue5/register_http",
                                json={
                                    "project_id": self.project_id,
                                    "project_name": self.project_name
                                },
                                timeout=5
                            )
                            if re_reg_response.status_code == 200:
                                print("✅ Re-registered with backend successfully")
                                self.connected = True
                            else:
                                print(f"⚠️ Re-registration failed: {re_reg_response.status_code}")
                        except Exception as e:
                            print(f"❌ Re-registration error: {e}")
                        continue  # Go to next poll cycle

                    # Reset failure counter on success
                    self.consecutive_failures = 0
                    self.connected = True

                else:
                    # Non-200 status code
                    self.consecutive_failures += 1
                    print(
                        f"⚠️ Poll failed with status {response.status_code} (attempt {self.consecutive_failures}/{self.max_consecutive_failures})"
                    )

                    if self.consecutive_failures >= self.max_consecutive_failures:
                        self._attempt_reconnect()

                # Wait before next poll
                time.sleep(self.poll_interval)

            except Exception as e:
                self.consecutive_failures += 1
                print(
                    f"⚠️ Polling error: {e} (attempt {self.consecutive_failures}/{self.max_consecutive_failures})"
                )

                if self.consecutive_failures >= self.max_consecutive_failures:
                    self._attempt_reconnect()
                else:
                    time.sleep(self.poll_interval * 2)  # Wait longer on error

        print("[HTTPPolling] Background polling thread stopped")

    def _check_module_version(self):
        """Check if modules have been updated and trigger reload if needed."""
        # Import locally to survive module cache clearing

        try:
            # Check if auto_update module has a version marker
            if 'AIAssistant.auto_update' in sys.modules:
                auto_update = sys.modules['AIAssistant.auto_update']
                if hasattr(auto_update, '_version_marker'):
                    current_version = auto_update._version_marker
                    if self.last_module_version and current_version != self.last_module_version:
                        print(
                            f"[HTTPPolling] 📦 Module version changed: {self.last_module_version} → {current_version}"
                        )
                        self._trigger_module_reload()
                    self.last_module_version = current_version
        except Exception:
            pass  # Silently ignore version check errors

    def _trigger_module_reload(self):
        """Trigger a full module reload when updates are detected."""
        # Import locally to survive module cache clearing

        print("[HTTPPolling] 🔄 Triggering automatic module reload...")

        try:
            # CRITICAL: Preserve client references BEFORE clearing modules
            if 'AIAssistant.main' in sys.modules:
                main_module = sys.modules['AIAssistant.main']
                if hasattr(main_module, '_preserve_clients'):
                    main_module._preserve_clients()
                    print("[HTTPPolling] ✅ Client references preserved for reload")
            
            # Clear all AIAssistant modules except action_queue
            modules_to_remove = [
                key for key in list(sys.modules.keys())
                if 'AIAssistant' in key and 'action_queue' not in key
            ]

            for module in modules_to_remove:
                del sys.modules[module]

            print(
                f"[HTTPPolling] 🗑️ Cleared {len(modules_to_remove)} cached modules"
            )

            # Re-import action handler module to get fresh code
            if self.action_handler:
                # Re-register action handler after reload
                print(
                    "[HTTPPolling] 🔄 Re-registering action handler with fresh code..."
                )
                # The handler will be re-imported on next use

            print("[HTTPPolling] ✅ Module reload complete - fresh code active")

        except Exception as e:
            print(f"[HTTPPolling] ❌ Module reload failed: {e}")

    def _send_heartbeat(self):
        """Send heartbeat to keep connection alive."""
        # Import locally to survive module cache clearing
        import requests

        try:
            requests.post(f"{self.base_url}/api/ue5/heartbeat",
                          json={"project_id": self.project_id},
                          timeout=5)
        except Exception as e:
            print(f"⚠️ Heartbeat failed: {e}")

    def _attempt_reconnect(self):
        """Attempt to reconnect after multiple failures."""
        # Import critical modules locally to survive module cache clearing
        import requests

        self.connected = False
        print(
            f"🔄 Backend connection lost. Attempting to re-register in {self.reconnect_delay}s..."
        )

        # Clear modules before reconnecting (fresh start)
        self._trigger_module_reload()

        time.sleep(self.reconnect_delay)

        try:
            # Re-register with backend
            response = requests.post(f"{self.base_url}/api/ue5/register_http",
                                     json={
                                         "project_id": self.project_id,
                                         "project_name": self.project_name
                                     },
                                     timeout=5)

            if response.status_code == 200:
                result = response.json()
                if result.get("success"):
                    self.connected = True
                    self.consecutive_failures = 0
                    print(f"✅ Successfully re-registered: {self.project_id}")

                    # Restart action queue ticker if needed
                    if self.action_queue:
                        self.action_queue.start_ticker()

                    return True

            print(f"❌ Re-registration failed: {response.status_code}")

        except Exception as e:
            print(f"❌ Re-registration error: {e}")

        # Reset counter to try again after more failures
        self.consecutive_failures = 0
        return False

    def _handle_command(self, cmd: dict):
        """Handle incoming command from backend with automatic error recovery."""
        try:
            message_type = cmd.get("type")

            if message_type == "execute_action":
                # Execute action requested by dashboard
                action = cmd.get("action")
                params = cmd.get("params", {})
                request_id = cmd.get("request_id")

                # Ensure action is a string (fix type error)
                if not action:
                    action = "unknown"
                else:
                    action = str(action)

                # Fix action name mapping
                if action == "project_info":
                    action = "get_project_info"

                print(
                    f"[HTTPPolling] 📥 Received action: {action} (from background thread)"
                )

                if self.action_handler:
                    if self.action_queue:
                        # Use thread-safe queue execution (RECOMMENDED)
                        print(
                            f"[HTTPPolling] 🎯 Queueing action for main thread: {action}"
                        )
                        success, result = self.action_queue.queue_action(
                            action, params, timeout=30.0)

                        # Check for threading errors and trigger automatic recovery
                        if not success and result.get("error"):
                            error_msg = str(result.get("error", ""))
                            if any(err in error_msg.lower() for err in [
                                    "main thread scheduling failed", "thread",
                                    "threading", "unreal.ticker",
                                    "ticker failed"
                            ]):
                                print(
                                    "[HTTPPolling] 🚨 THREADING ERROR DETECTED!"
                                )
                                print(
                                    "[HTTPPolling] 🔄 Triggering automatic module reload for recovery..."
                                )
                                self._trigger_emergency_recovery()

                                # Retry the action after recovery
                                print(
                                    "[HTTPPolling] 🔁 Retrying action after recovery..."
                                )
                                success, result = self.action_queue.queue_action(
                                    action, params, timeout=30.0)

                        if success:
                            print(
                                f"[HTTPPolling] ✅ Action executed on main thread: {action}"
                            )
                        else:
                            print(
                                f"[HTTPPolling] ❌ Action failed: {result.get('error', 'Unknown error')}"
                            )

                        # Send response back via HTTP
                        response = {
                            "request_id": request_id,
                            "action": action,
                            "success": success,
                            "data": result.get("data"),
                            "error": result.get("error")
                        }

                        self.send_message(response)

                    else:
                        # Fallback to direct execution (may cause threading issues)
                        print(
                            f"[HTTPPolling] ⚠️ Direct execution from background thread: {action}"
                        )
                        print(
                            "[HTTPPolling] ⚠️ This may cause 'Main thread scheduling failed' errors!"
                        )

                        try:
                            result = self.action_handler(action, params)

                            # Check for threading errors in response
                            if not result.get("success") and result.get(
                                    "error"):
                                error_msg = str(result.get("error", ""))
                                if any(err in error_msg.lower() for err in [
                                        "main thread scheduling failed",
                                        "thread", "threading"
                                ]):
                                    print(
                                        "[HTTPPolling] 🚨 THREADING ERROR DETECTED!"
                                    )
                                    print(
                                        "[HTTPPolling] 🔄 Triggering automatic module reload for recovery..."
                                    )
                                    self._trigger_emergency_recovery()

                                    # Retry after recovery
                                    result = self.action_handler(
                                        action, params)

                            # Send response back via HTTP
                            response = {
                                "request_id": request_id,
                                "action": action,
                                "success": result.get("success", True),
                                "data": result.get("data"),
                                "error": result.get("error")
                            }

                            self.send_message(response)
                            print(f"[HTTPPolling] Action completed: {action}")

                        except Exception as e:
                            error_msg = str(e)
                            # Check if it's a threading error
                            if any(err in error_msg.lower() for err in [
                                    "main thread", "thread", "threading",
                                    "ticker"
                            ]):
                                print(
                                    "[HTTPPolling] 🚨 THREADING ERROR DETECTED!"
                                )
                                self._trigger_emergency_recovery()

                            print(
                                f"[HTTPPolling] ❌ Direct execution error: {e}")
                            self.send_message({
                                "request_id": request_id,
                                "success": False,
                                "error": str(e)
                            })
                else:
                    # No handler - send error response
                    self.send_message({
                        "request_id": request_id,
                        "success": False,
                        "error": "No action handler configured"
                    })

            elif message_type == "auto_update":
                # Backend triggered auto-update
                print("📢 Backend update detected! Running auto-update...")
                self._handle_auto_update()

            elif message_type == "emergency_update":
                # Backend triggered emergency update (no restart mode)
                mode = cmd.get("mode", "no_restart")
                print("🚨 EMERGENCY UPDATE REQUESTED! (No-restart mode)")
                print("⚠️  Files will be updated WITHOUT restarting UE5")
                self._handle_auto_update(mode="no_restart")

            elif message_type == "emergency_recovery":
                # Backend triggered emergency recovery (dashboard "Fix Crash Issues")
                print("🚑 EMERGENCY RECOVERY REQUESTED from dashboard!")
                print("[HTTPPolling] Triggering self-healing recovery...")
                self._trigger_emergency_recovery()

            elif message_type == "force_reload":
                # Backend triggered force module reload
                print("📢 Force module reload requested by dashboard!")
                self._trigger_emergency_recovery()

            elif message_type == "widget_generated":
                # Write widget file to local project
                print("📝 Writing widget file to project...")
                self._write_widget_file(cmd)

            elif message_type == "file_drop":
                # Write custom file to project
                print("📝 Writing file from File Drop tool...")
                self._write_custom_file(cmd)

            elif message_type == "reconnect":
                # Restart HTTP polling connection
                print("🔌 Reconnect command received from dashboard...")
                self._handle_reconnect_command()

            elif message_type == "server_switch":
                # Switch backend server endpoint
                server_type = cmd.get("server_type", "production")
                server_url = cmd.get("server_url", "")
                print(f"🔄 Server switch command received: {server_type} → {server_url}")
                self._handle_server_switch(server_type, server_url)

        except Exception as e:
            print(f"❌ Error handling command: {e}")

    def _handle_auto_update(self, mode="auto"):
        """Handle auto-update triggered by backend - thread safe version.

        Args:
            mode: "auto" (default) - Normal update with auto-restart
                  "no_restart" - Emergency update mode, no automatic restart
        """
        # Import critical modules locally to survive module cache clearing
        import uuid

        try:
            if mode == "no_restart":
                print(
                    "[HTTPPolling] 🚨 Starting EMERGENCY UPDATE (no-restart mode)..."
                )
            else:
                print("[HTTPPolling] 🔄 Starting auto-update process...")

            # Check if we're in UE5 environment
            try:
                import unreal  # type: ignore
            except ImportError:
                print("⚠️ Auto-update skipped (not in UE5 environment)")
                return

            # Clear old auto_update module first
            if 'AIAssistant.auto_update' in sys.modules:
                del sys.modules['AIAssistant.auto_update']
                print("[HTTPPolling] Cleared old auto_update module")

            # Import fresh auto_update module
            # Add version marker for tracking
            from ..system import auto_update
            version_marker = str(uuid.uuid4())[:8]
            auto_update._version_marker = version_marker

            # Run update (safe from background thread - just downloads files)
            result = auto_update.check_and_update(mode=mode)

            if result:
                if mode == "no_restart":
                    print(
                        "[HTTPPolling] ✅ Emergency update downloaded successfully!"
                    )
                    print("[HTTPPolling] ⚠️  NO automatic restart performed")
                    print(
                        "[HTTPPolling] 🔄 Please restart Unreal Engine 5 manually to apply fixes"
                    )
                else:
                    print(
                        "[HTTPPolling] ✅ Auto-update downloaded successfully")
                    print("[HTTPPolling] ⏳ Queueing restart on main thread...")
                    print("[HTTPPolling] ℹ️ Module reload will happen safely on main thread")

                    # Queue restart action to main thread instead of direct call
                    # The restart action handles module clearing safely on the main thread
                    if self.action_queue:
                        success, restart_result = self.action_queue.queue_action(
                            'restart_assistant', {
                                'reason': 'auto_update',
                                'version': version_marker
                            },
                            timeout=10.0)

                        if success:
                            print(
                                "[HTTPPolling] ✅ Assistant restart queued for main thread"
                            )
                        else:
                            print(
                                f"[HTTPPolling] ❌ Failed to queue restart: {restart_result.get('error')}"
                            )
                    else:
                        print(
                            "[HTTPPolling] ⚠️ No action queue - restart will happen on next ticker cycle"
                        )

                    # Update version tracker
                    self.last_module_version = version_marker
                    print(
                        f"[HTTPPolling] 📦 Version marker updated: {version_marker}"
                    )

            else:
                print("[HTTPPolling] ℹ️ No updates available or update failed")

        except Exception as e:
            print(f"[HTTPPolling] ❌ Auto-update failed: {e}")
            import traceback
            traceback.print_exc()

    def send_message(self, data: dict):
        """
        Send message to backend via HTTP POST with retry logic.

        Args:
            data: Message data to send
        """
        # Import locally to survive module cache clearing
        import requests

        max_retries = 3
        retry_delay = 1  # seconds

        for attempt in range(max_retries):
            try:
                response = requests.post(f"{self.base_url}/api/ue5/response",
                                         json={
                                             "project_id": self.project_id,
                                             "response": data
                                         },
                                         timeout=5)

                if response.status_code == 200:
                    return  # Success
                else:
                    print(
                        f"⚠️ Response send failed ({response.status_code}), attempt {attempt + 1}/{max_retries}"
                    )

            except Exception as e:
                print(
                    f"⚠️ Failed to send message (attempt {attempt + 1}/{max_retries}): {e}"
                )

            # Retry with delay (except on last attempt)
            if attempt < max_retries - 1:
                time.sleep(retry_delay)

        # All retries failed
        print(f"❌ Failed to send message after {max_retries} attempts")

    def _trigger_emergency_recovery(self):
        """Force complete module reload and assistant restart - thread safe version."""
        try:
            print(
                "[HTTPPolling] 🚨 Emergency Recovery: Queueing restart on main thread..."
            )

            # Step 1: Stop current connections
            self.disconnect()

            # Step 2: Queue restart action to main thread (thread-safe)
            if self.action_queue:
                print(
                    "[HTTPPolling] 🎯 Queueing restart action for main thread execution..."
                )
                success, result = self.action_queue.queue_action(
                    'restart_assistant',  # Special internal action
                    {'reason': 'emergency_recovery'},
                    timeout=10.0)

                if success:
                    print(
                        "[HTTPPolling] ✅ Assistant restart queued successfully"
                    )
                else:
                    print(
                        f"[HTTPPolling] ❌ Failed to queue restart: {result.get('error', 'Unknown error')}"
                    )
                    # Try manual restart as fallback
                    self._queue_manual_restart()
            else:
                # No action queue - queue a manual restart
                print(
                    "[HTTPPolling] ⚠️ No action queue available - attempting manual restart..."
                )
                self._queue_manual_restart()

        except Exception as e:
            print(f"[HTTPPolling] ❌ Emergency recovery failed: {e}")
            import traceback
            traceback.print_exc()

    def _queue_manual_restart(self):
        """Queue a manual restart action to main thread."""
        try:
            if self.action_queue:
                print(
                    "[HTTPPolling] 🔄 Queueing manual restart on main thread..."
                )
                success, result = self.action_queue.queue_action(
                    'manual_restart', {'reason': 'fallback'}, timeout=10.0)
                if success:
                    print("[HTTPPolling] ✅ Manual restart queued successfully")
                else:
                    print(
                        f"[HTTPPolling] ❌ Failed to queue manual restart: {result.get('error')}"
                    )
            else:
                print(
                    "[HTTPPolling] ❌ Cannot queue manual restart - no action queue available"
                )
        except Exception as e:
            print(f"[HTTPPolling] ❌ Failed to queue manual restart: {e}")

    def _manual_restart_assistant(self):
        """Manual restart fallback if force_restart_assistant is not available."""
        # Import locally to survive module cache clearing

        # This should only be called from main thread via action queue
        try:
            print("[HTTPPolling] 🔄 Attempting manual restart (main thread)...")

            # CRITICAL: Preserve client references BEFORE clearing modules
            if 'AIAssistant.main' in sys.modules:
                main_module = sys.modules['AIAssistant.main']
                if hasattr(main_module, '_preserve_clients'):
                    main_module._preserve_clients()
                    print("[HTTPPolling] ✅ Client references preserved for manual restart")

            # Clear all AIAssistant modules except action_queue
            modules_to_remove = [
                key for key in list(sys.modules.keys())
                if 'AIAssistant' in key and 'action_queue' not in key
            ]
            for module in modules_to_remove:
                del sys.modules[module]

            print(
                f"[HTTPPolling] 🗑️ Cleared {len(modules_to_remove)} cached modules"
            )

            # Re-import main module
            import AIAssistant.core.main as fresh_main
            fresh_main.get_assistant()
            print("[HTTPPolling] ✅ Manual restart complete")

        except Exception as e:
            print(f"[HTTPPolling] ❌ Manual restart failed: {e}")

    def _write_widget_file(self, cmd: dict):
        """Write widget script file to local UE5 project."""
        try:
            import os
            try:
                import unreal
            except ImportError:
                print("⚠️ Not in UE5 environment - cannot write widget file")
                return

            widget_name = cmd.get("widget_name", "UnknownWidget")
            script_content = cmd.get("script_content", "")
            
            if not script_content:
                print(f"❌ No script content provided for widget: {widget_name}")
                return

            # Use UE5 paths to get project directory
            project_dir = unreal.Paths.project_dir()
            widget_folder = os.path.join(project_dir, "Content", "Python", "AIAgentUtilities")
            
            # Create directory if needed
            os.makedirs(widget_folder, exist_ok=True)
            
            # Write file
            script_path = os.path.join(widget_folder, f"{widget_name}.py")
            with open(script_path, 'w', encoding='utf-8') as f:
                f.write(script_content)
            
            print(f"✅ Widget file written: {script_path}")
            unreal.log(f"✅ Widget '{widget_name}' written to: {script_path}")
            
        except Exception as e:
            print(f"❌ Failed to write widget file: {e}")

    def _write_custom_file(self, cmd: dict):
        """Write custom file from File Drop tool to project."""
        try:
            import os
            try:
                import unreal
            except ImportError:
                print("⚠️ Not in UE5 environment - cannot write file")
                return

            filename = cmd.get("filename", "custom_file.txt")
            content = cmd.get("content", "")
            
            # Use UE5 paths to get project directory
            project_dir = unreal.Paths.project_dir()
            target_folder = os.path.join(project_dir, "Content", "Python", "AIAgentUtilities")
            
            # Create directory if needed
            os.makedirs(target_folder, exist_ok=True)
            
            # Write file
            file_path = os.path.join(target_folder, filename)
            with open(file_path, 'w', encoding='utf-8') as f:
                f.write(content)
            
            file_size = os.path.getsize(file_path)
            print(f"✅ File written: {file_path} ({file_size} bytes)")
            unreal.log(f"✅ File Drop: '{filename}' written to: {file_path}")
            
        except Exception as e:
            print(f"❌ Failed to write file: {e}")

    def _handle_reconnect_command(self):
        """Handle reconnect command from dashboard - set flag to trigger reconnection."""
        try:
            try:
                import unreal
            except ImportError:
                print("⚠️ Not in UE5 environment - cannot restart connection")
                return
            
            unreal.log("🔌 Dashboard reconnect command received...")
            print("🔌 Reconnect command received from dashboard...")
            
            # Set reconnect flag - poll loop will check this and trigger _attempt_reconnect()
            self._reconnect_requested = True
            
            print("✅ Reconnection requested - will reconnect on next poll cycle")
            unreal.log("✅ Reconnect flag set, will reconnect on next poll")
            
        except Exception as e:
            print(f"❌ Reconnect command failed: {e}")
            try:
                import unreal
                unreal.log_error(f"❌ Reconnect failed: {e}")
            except:
                pass

    def _handle_server_switch(self, server_type: str, server_url: str):
        """Handle server switch command from dashboard."""
        try:
            try:
                import unreal
            except ImportError:
                print("⚠️ Not in UE5 environment - cannot switch server")
                return
            
            # Import config locally to survive module reloads
            from ..core import config
            
            print(f"🔄 Switching to {server_type} server: {server_url}")
            unreal.log(f"🔄 Server switch: {server_type} → {server_url}")
            
            # Update config to use new server
            cfg = config.get_config()
            success = cfg.switch_server(server_type)
            
            if success:
                # Update our base_url
                self.base_url = server_url
                print(f"✅ Base URL updated to: {self.base_url}")
                
                # Trigger reconnection to new server
                self._reconnect_requested = True
                print(f"✅ Server switched to {server_type}, reconnecting...")
                unreal.log(f"✅ Switched to {server_type}, reconnecting to {server_url}")
            else:
                print(f"❌ Failed to switch server: invalid server type '{server_type}'")
                unreal.log_error(f"❌ Server switch failed: invalid type '{server_type}'")
            
        except Exception as e:
            print(f"❌ Server switch error: {e}")
            try:
                import unreal
                unreal.log_error(f"❌ Server switch error: {e}")
            except:
                pass

    def disconnect(self):
        """Disconnect from backend - thread safe version."""
        self.running = False
        self.connected = False

        # NOTE: Do NOT call stop_ticker() here!
        # stop_ticker() unregisters Slate callbacks which MUST be done on main thread
        # The ticker will be stopped during restart which happens on main thread
        
        print("🔌 HTTP polling disconnected")

    def is_connected(self) -> bool:
        """Check if connected."""
        return self.connected
