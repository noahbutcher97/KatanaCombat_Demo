"""
WebSocket client for UE5 to enable real-time communication with backend.
"""
import json
import threading
import time
from typing import Any, Callable, Dict, Optional


class WebSocketClient:
    """
    WebSocket client for UE5 to connect to the backend.
    Enables real-time bidirectional communication.
    """
    
    def __init__(self, base_url: str, project_id: str):
        """
        Initialize WebSocket client.
        
        Args:
            base_url: Backend base URL (e.g., "wss://ue5-assistant-noahbutcher97.replit.app")
            project_id: Unique project identifier
        """
        self.base_url = base_url.replace("https://", "wss://").replace("http://", "ws://")
        self.project_id = project_id
        self.ws_url = f"{self.base_url}/ws/ue5/{project_id}"
        
        self.ws = None
        self.connected = False
        self.running = False
        self.receive_thread = None
        self.action_handler: Optional[Callable[[str, Dict[str, Any]], Dict[str, Any]]] = None
        
        # Reconnection settings
        self.reconnect_attempts = 0
        self.max_reconnect_attempts = 10
        self.base_reconnect_delay = 2  # seconds
        
    def set_action_handler(self, handler: Callable[[str, Dict[str, Any]], Dict[str, Any]]):
        """
        Set the handler for incoming action commands.
        
        Args:
            handler: Function that takes (action, params) and returns response dict
        """
        self.action_handler = handler
    
    def connect(self) -> bool:
        """
        Connect to the WebSocket server.
        
        Returns:
            True if connected successfully, False otherwise
        """
        try:
            import websocket
            
            print(f"🔌 Attempting WebSocket connection to: {self.ws_url}")
            
            self.ws = websocket.WebSocketApp(
                self.ws_url,
                on_message=self._on_message,
                on_error=self._on_error,
                on_close=self._on_close,
                on_open=self._on_open
            )
            
            # Start connection in background thread
            self.running = True
            self.receive_thread = threading.Thread(target=self._run_forever, daemon=True)
            self.receive_thread.start()
            
            # Wait for connection (max 5 seconds)
            for i in range(50):
                if self.connected:
                    print(f"✅ WebSocket connected: {self.project_id}")
                    return True
                time.sleep(0.1)
            
            print("⚠️ WebSocket connection timeout after 5 seconds")
            print(f"   URL attempted: {self.ws_url}")
            print(f"   Project ID: {self.project_id}")
            return False
            
        except ImportError:
            print("⚠️ websocket-client not installed. Install with: pip install websocket-client")
            return False
        except Exception as e:
            print(f"❌ WebSocket connection failed: {e}")
            print(f"   URL: {self.ws_url}")
            return False
    
    def _run_forever(self):
        """Run WebSocket connection loop."""
        if self.ws:
            self.ws.run_forever()
    
    def _on_open(self, ws):
        """Handle WebSocket connection opened."""
        self.connected = True
        self.reconnect_attempts = 0  # Reset counter on successful connection
        print(f"🔗 WebSocket opened for project: {self.project_id}")
    
    def _on_message(self, ws, message):
        """Handle incoming WebSocket message."""
        try:
            data = json.loads(message)
            message_type = data.get("type")
            
            if message_type == "execute_action":
                # Execute action requested by dashboard
                action = data.get("action")
                params = data.get("params", {})
                request_id = data.get("request_id")
                
                if self.action_handler:
                    # Check if we're on a background thread and have action queue
                    import threading
                    try:
                        from ..execution.action_queue import get_action_queue
                        action_queue = get_action_queue()
                        
                        if threading.current_thread() != threading.main_thread():
                            # Queue for main thread execution (thread-safe)
                            print(f"[WebSocket] 🎯 Queueing action for main thread: {action}")
                            success, result = action_queue.queue_action(action, params, timeout=30.0)
                            if not success:
                                result = {"success": False, "error": result.get("error", "Queue timeout")}
                        else:
                            # Execute directly on main thread
                            result = self.action_handler(action, params)
                    except ImportError:
                        # No action queue, execute directly (may cause threading issues)
                        result = self.action_handler(action, params)
                    
                    # Send response back
                    response = {
                        "request_id": request_id,
                        "action": action,
                        "success": result.get("success", True),
                        "data": result.get("data"),
                        "error": result.get("error")
                    }
                    
                    self.send_message(response)
                else:
                    # No handler set
                    self.send_message({
                        "request_id": request_id,
                        "success": False,
                        "error": "No action handler configured"
                    })
            
            elif message_type == "auto_update":
                # Backend triggered auto-update
                print("📢 Backend update detected! Running auto-update...")
                self._handle_auto_update()
                    
        except Exception as e:
            print(f"❌ Error handling message: {e}")
    
    def _handle_auto_update(self):
        """Handle auto-update triggered by backend."""
        try:
            # Import here to avoid circular dependency
            import importlib
            import sys
            
            # Check if we're in UE5 environment
            try:
                import unreal
            except ImportError:
                print("⚠️ Auto-update skipped (not in UE5 environment)")
                return
            
            # Run auto-update to download files
            if 'AIAssistant.auto_update' in sys.modules:
                # Reload the module
                importlib.reload(sys.modules['AIAssistant.auto_update'])
            
            from AIAssistant import auto_update
            result = auto_update.check_and_update()
            
            # check_and_update returns bool
            if result:
                print("✅ Auto-update completed successfully")
                print("🔄 Force reloading ALL AIAssistant modules...")
                
                # Force clear ALL AIAssistant modules from cache
                modules_to_remove = [key for key in sys.modules.keys() if 'AIAssistant' in key]
                for module in modules_to_remove:
                    del sys.modules[module]
                print(f"🗑️ Cleared {len(modules_to_remove)} cached modules")
                
                # Don't re-import from background thread - let UE5 reload naturally
                print("✅ Modules cleared. Fresh code will load on next request.")
            else:
                print("ℹ️ Auto-update failed or no updates available")
                
        except Exception as e:
            print(f"❌ Auto-update failed: {e}")
    
    def _on_error(self, ws, error):
        """Handle WebSocket error."""
        print(f"❌ WebSocket error: {error}")
        self.connected = False
    
    def _on_close(self, ws, close_status_code, close_msg):
        """Handle WebSocket connection closed."""
        self.connected = False
        print(f"🔌 WebSocket closed: {close_msg}")
        
        # Auto-reconnect with exponential backoff
        if self.running and self.reconnect_attempts < self.max_reconnect_attempts:
            self.reconnect_attempts += 1
            delay = min(self.base_reconnect_delay * (2 ** (self.reconnect_attempts - 1)), 60)
            print(f"🔄 Reconnecting in {delay}s (attempt {self.reconnect_attempts}/{self.max_reconnect_attempts})...")
            time.sleep(delay)
            self._reconnect()
        elif self.reconnect_attempts >= self.max_reconnect_attempts:
            print("❌ Max reconnection attempts reached. WebSocket permanently disconnected.")
            self.running = False
    
    def _reconnect(self):
        """Attempt to reconnect to WebSocket."""
        if not self.running:
            return
            
        try:
            import websocket
            
            self.ws = websocket.WebSocketApp(
                self.ws_url,
                on_message=self._on_message,
                on_error=self._on_error,
                on_close=self._on_close,
                on_open=self._on_open
            )
            
            # Run in current thread (called from _on_close)
            self.ws.run_forever()
            
        except Exception as e:
            print(f"❌ Reconnection failed: {e}")
    
    def send_message(self, data: dict):
        """
        Send message to backend.
        
        Args:
            data: Message data to send
        """
        if self.ws and self.connected:
            try:
                self.ws.send(json.dumps(data))
            except Exception as e:
                print(f"❌ Failed to send message: {e}")
        else:
            print("⚠️ WebSocket not connected")
    
    def disconnect(self):
        """Disconnect from WebSocket server."""
        self.running = False
        if self.ws:
            self.ws.close()
        self.connected = False
        print("🔌 WebSocket disconnected")
    
    def is_connected(self) -> bool:
        """Check if WebSocket is connected."""
        return self.connected
