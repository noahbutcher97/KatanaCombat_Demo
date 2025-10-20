"""
Main entry point for UE5 AI Assistant.
Supports both sync and async modes.
"""
from typing import Optional

from ..execution.action_executor import get_executor
from ..network.api_client import get_client
from ..network.async_client import get_async_client
from .config import get_config
from ..ui.ui_manager import get_ui_manager
from .utils import Logger

# Check for websocket-client dependency
try:
    from ..network.websocket_client import WebSocketClient
except ImportError:
    # Missing websocket-client library
    try:
        import unreal
        unreal.log_error("=" * 60)
        unreal.log_error("❌ Missing dependency: websocket-client")
        unreal.log_error("=" * 60)
        unreal.log_warning("📦 Install it with:")
        unreal.log_warning("   import AIAssistant.install_dependencies")
        unreal.log_error("=" * 60)
    except ImportError:
        pass
    raise


class AIAssistant:
    """Main AI Assistant orchestrator."""

    def __init__(self):
        self.config = get_config()
        self.sync_client = get_client()
        self.async_client = get_async_client()
        self.executor = get_executor()
        self.ui = get_ui_manager()
        self.logger = Logger("AIAssistant", self.config.verbose)

        # Conversation memory
        self.session_messages = [
            {
                "role": "system",
                "content": (
                    "You are a technical documentation system "
                    "for Unreal Engine 5.6. Generate structured "
                    "technical prose describing editor state and "
                    "scene contents. Use precise terminology, "
                    "include specific names and values, connect "
                    "information logically. Write in third-person "
                    "declarative style. Avoid conversational "
                    "phrasing, questions, or subjective language."
                ),
            }
        ]

        # Async state
        self._async_response: Optional[str] = None
        self._async_in_progress = False
        
        # WebSocket connection for real-time communication
        self.ws_client: Optional[WebSocketClient] = None
        
        # HTTP polling client for fallback connection
        self.http_client = None
        
        # Initialize action queue for thread-safe execution
        try:
            from ..execution.action_queue import get_action_queue
            self.action_queue = get_action_queue()
            self.action_queue.set_action_handler(self._execute_action_wrapper)
            print("[AIAssistant] ✅ Action queue initialized for thread-safe execution")
        except ImportError:
            self.action_queue = None
            print("[AIAssistant] ⚠️ Action queue not available")
        
        # Auto-register project with backend
        self._auto_register_project()
        
        # Initialize WebSocket connection
        self._init_websocket()

    def process_command(
        self, user_input: str, use_async: bool = False
    ) -> str:
        """
        Main command processing flow.

        Args:
            user_input: User's command text
            use_async: If True, uses async (non-blocking) execution.
                      If False, uses sync (blocks editor).
                      Default: False (sync is safe for UE thread model)

        Returns:
            Response text (or status if async)
        """
        try:
            # Show thinking state
            self.ui.show_thinking()

            # Log user input
            self.logger.info(f"User: {user_input}")

            # Check for tokenized actions (always sync)
            if self._is_action_request(user_input):
                token = self._extract_action_token(user_input)
                response = self.executor.execute(token)
                self.ui.show_response(response)
                self.ui.log_exchange(user_input, response)
                return response

            # Process with AI
            if use_async:
                return self._process_async(user_input)
            else:
                return self._process_sync(user_input)

        except Exception as e:
            error_msg = self.ui.format_error(str(e))
            self.ui.show_response(error_msg)
            self.logger.error(f"Command failed: {e}")
            return error_msg

    def _extract_token_from_response(self, response: str) -> tuple:
        """
        Extract action tokens from anywhere in the response.
        
        Returns:
            (token_type, token_content, explanatory_text)
            token_type: "UE_REQUEST" | "UE_CONTEXT_REQUEST" | None
            token_content: The extracted token string
            explanatory_text: AI explanation text (if any)
        """
        import re
        
        # Check for UE_REQUEST token
        # Pattern stops at: period, exclamation, question mark, newline, or another bracket
        ue_request_match = re.search(r'\[UE_REQUEST\]\s*([^\.\!\?\n\[]+?)(?=[\.\!\?\n\[]|$)', response)
        if ue_request_match:
            token_content = ue_request_match.group(1).strip()
            # Extract any explanatory text before the token
            explanatory_text = response[:ue_request_match.start()].strip()
            # Extract any trailing text after the token (for future use)
            token_end = ue_request_match.end()
            return ("UE_REQUEST", token_content, explanatory_text)
        
        # Check for UE_CONTEXT_REQUEST token
        # Questions end with ?, so capture including ? but exclude other boundary punctuation
        context_match = re.search(r'\[UE_CONTEXT_REQUEST\]\s*([^\n\[]+?)(?=[\.\!]\s+|[\?]\s+[A-Z]|[\?]\s+[a-z]{2,}|\n|\[|$)', response)
        if context_match:
            token_content = context_match.group(1).strip()
            # If there's a ? right after (at boundary), include it
            if response[context_match.end():context_match.end()+1] == '?':
                token_content += '?'
            explanatory_text = response[:context_match.start()].strip()
            return ("UE_CONTEXT_REQUEST", token_content, explanatory_text)
        
        # No token found
        return (None, None, response)
    
    def _process_sync(self, user_input: str) -> str:
        """
        Synchronous processing (blocks editor).
        Use for quick commands only.
        """
        response = self._process_with_ai_sync(user_input)

        # Extract token from anywhere in response
        token_type, token_content, explanatory_text = self._extract_token_from_response(response)
        
        # Handle UE_REQUEST tokens (direct actions)
        if token_type == "UE_REQUEST":
            response = self.executor.execute(token_content)
            # Prepend AI explanation if present
            if explanatory_text:
                response = f"{explanatory_text}\n\n{response}"
        
        # Handle UE_CONTEXT_REQUEST tokens (collect context, send to AI)
        elif token_type == "UE_CONTEXT_REQUEST":
            parts = token_content.split("|", 1)
            if len(parts) == 2:
                context_type, original_question = parts
                response = self._process_context_request(
                    context_type.strip(),
                    original_question.strip()
                )
                # Prepend AI explanation if present
                if explanatory_text:
                    response = f"{explanatory_text}\n\n{response}"

        self.ui.show_response(response)
        self.ui.log_exchange(user_input, response)
        return response

    def _process_async(self, user_input: str) -> str:
        """
        Asynchronous processing (non-blocking).
        Returns immediately, response written to file when ready.
        """
        if self._async_in_progress:
            return "⏳ Previous request still processing..."

        self._async_in_progress = True
        self._async_response = None

        # Add user message to history
        self.session_messages.append(
            {"role": "user", "content": user_input}
        )

        # Trim history if too long
        self._trim_history()

        # Define callback
        def on_complete(response_data: dict):
            try:
                response = response_data.get("response", "")

                if not response:
                    error = response_data.get(
                        "error", "Unknown error"
                    )
                    response = self.ui.format_error(error)

                # Extract token from anywhere in response
                token_type, token_content, explanatory_text = self._extract_token_from_response(response)
                
                # Handle UE_REQUEST tokens (direct actions)
                if token_type == "UE_REQUEST":
                    response = self.executor.execute(token_content)
                    # Prepend AI explanation if present
                    if explanatory_text:
                        response = f"{explanatory_text}\n\n{response}"
                
                # Handle UE_CONTEXT_REQUEST tokens (collect, send to AI)
                elif token_type == "UE_CONTEXT_REQUEST":
                    parts = token_content.split("|", 1)
                    if len(parts) == 2:
                        context_type, original_question = parts
                        response = self._process_context_request(
                            context_type.strip(),
                            original_question.strip()
                        )
                        # Prepend AI explanation if present
                        if explanatory_text:
                            response = f"{explanatory_text}\n\n{response}"
                else:
                    # Add to history
                    self.session_messages.append(
                        {"role": "assistant", "content": response}
                    )

                # Write response
                self.ui.show_response(response)
                self.ui.log_exchange(user_input, response)

                self._async_response = response
                self._async_in_progress = False
                self.logger.success("Async command completed")

            except Exception as e:
                error_msg = self.ui.format_error(str(e))
                self.ui.show_response(error_msg)
                self._async_in_progress = False
                self.logger.error(f"Async callback failed: {e}")

        # Start async request
        url = self.config.execute_endpoint
        payload = {"prompt": user_input}
        self.async_client.post_json_async(
            url, payload, on_complete
        )

        return "⏳ Processing... (async mode)"

    def _process_with_ai_sync(self, user_input: str) -> str:
        """Send input to AI backend synchronously."""
        # Add user message to history
        self.session_messages.append(
            {"role": "user", "content": user_input}
        )

        # Trim history
        self._trim_history()

        # Send to API (blocking)
        response_data = self.sync_client.execute_command(user_input)

        # Extract response
        response = response_data.get("response", "")

        if not response:
            error = response_data.get("error", "Unknown error")
            return self.ui.format_error(error)

        # Add AI response to history
        if not response.startswith("[UE_REQUEST]"):
            self.session_messages.append(
                {"role": "assistant", "content": response}
            )

        return response
    
    def _process_context_request(
        self, context_type: str, original_question: str
    ) -> str:
        """
        Collect context and send to AI for natural language processing.
        
        Args:
            context_type: Type of context (e.g., 'project_info')
            original_question: User's original question
        """
        try:
            # Collect appropriate context
            context_data = {}
            
            if context_type == "project_info":
                # Get raw project metadata
                metadata_collector = self.executor.metadata_collector
                context_data = metadata_collector.collect_all_metadata()
            
            elif context_type == "blueprint_capture":
                # Capture blueprint screenshot
                from ..tools.blueprint_capture import capture_blueprint_screenshot
                result = capture_blueprint_screenshot()
                if "error" in result:
                    return self.ui.format_error(result["error"])
                context_data = result
            
            elif context_type == "browse_files":
                # Browse project files
                from ..execution.action_executor import browse_project_files
                file_tree = browse_project_files()
                context_data = {"file_tree": file_tree}
            
            else:
                return f"[UE_ERROR] Unknown context type: {context_type}"
            
            # Send to AI for interpretation
            api_client = get_client()
            payload = {
                "question": original_question,
                "context": context_data,
                "context_type": context_type
            }
            
            response_data = api_client.post_json(
                "/answer_with_context",
                payload
            )
            
            if response_data.get("error"):
                return self.ui.format_error(response_data["error"])
            
            return response_data.get("response", "")
            
        except Exception as e:
            return self.ui.format_error(
                f"Context request failed: {e}"
            )

    def _trim_history(self) -> None:
        """Trim conversation history to max turns."""
        max_turns = self.config.get("max_context_turns", 6)
        user_assistant_msgs = [
            m
            for m in self.session_messages
            if m["role"] != "system"
        ]

        if len(user_assistant_msgs) > max_turns * 2:
            self.session_messages = (
                self.session_messages[:1]
                + user_assistant_msgs[-(max_turns * 2) :]
            )

    def _is_action_request(self, text: str) -> bool:
        """Check if input directly requests a known action."""
        lower = text.lower()
        keywords = {
            "viewport": "describe_viewport",
            "what do i see": "describe_viewport",
            "describe scene": "describe_viewport",
            "list actors": "list_actors",
            "selected": "get_selected_info",
        }

        return any(keyword in lower for keyword in keywords)

    def _extract_action_token(self, text: str) -> str:
        """Extract action token from user input."""
        lower = text.lower()

        if any(
            k in lower
            for k in ["viewport", "what do i see", "describe"]
        ):
            return "describe_viewport"
        elif "list actors" in lower:
            return "list_actors"
        elif "selected" in lower:
            return "get_selected_info"

        return ""


    def _auto_register_project(self):
        """Auto-register project on initialization."""
        try:
            from ..system.project_registration import auto_register_project
            result = auto_register_project(self.sync_client)
            if not result.get("success"):
                import unreal
                unreal.log_warning(
                    f"⚠️ Project auto-registration failed: {result.get('error', 'Unknown')}"
                )
                unreal.log_warning(
                    "💡 Diagnostic: import AIAssistant.test_registration"
                )
                unreal.log_warning(
                    "💡 Auto-update: import AIAssistant.auto_update; AIAssistant.auto_update.check_and_update()"
                )
        except Exception as e:
            import unreal
            unreal.log_error(f"❌ Project auto-registration error: {e}")
            unreal.log_warning(
                "💡 Diagnostic: import AIAssistant.test_registration"
            )
            unreal.log_warning(
                "💡 Auto-update: import AIAssistant.auto_update; AIAssistant.auto_update.check_and_update()"
            )
    
    def _init_websocket(self):
        """Initialize connection for real-time communication (supports both WebSocket and HTTP Polling)."""
        try:
            import hashlib

            import unreal
            
            # Get project info
            project_path = unreal.Paths.project_dir()
            project_name = unreal.Paths.get_project_file_path().split('/')[-1].replace('.uproject', '')
            project_id = hashlib.md5(project_path.encode()).hexdigest()
            
            unreal.log(f"📋 Project Path: {project_path}")
            unreal.log(f"📋 Project ID: {project_id}")
            
            base_url = self.config.api_url
            unreal.log(f"📋 Backend URL: {base_url}")
            
            # Fetch connection mode preference from backend
            connection_mode = self._get_connection_mode_preference(project_id, base_url)
            unreal.log(f"📡 Connection Mode: {connection_mode.upper()}")
            
            # Try the preferred mode first
            if connection_mode == "websocket":
                if self._try_websocket_connection(base_url, project_id):
                    unreal.log("✅ Real-time connection enabled (WebSocket)")
                    return
                else:
                    unreal.log_warning("⚠️ WebSocket failed, falling back to HTTP Polling...")
                    connection_mode = "http"
            
            if connection_mode == "http":
                if self._try_http_polling_connection(base_url, project_id, project_name):
                    unreal.log("✅ Real-time connection enabled (HTTP Polling)")
                    return
                else:
                    unreal.log_error("❌ All connection methods failed")
                    unreal.log_error("   Real-time dashboard features disabled")
                
        except Exception as e:
            import unreal
            unreal.log_error(f"⚠️ Connection init failed: {e}")
    
    def _get_connection_mode_preference(self, project_id: str, base_url: str) -> str:
        """Fetch connection mode preference from backend."""
        try:
            import requests
            response = requests.get(
                f"{base_url}/api/projects",
                timeout=5
            )
            if response.ok:
                projects = response.json()
                for project in projects:
                    if project.get("project_id") == project_id:
                        return project.get("connection_mode", "http")
        except Exception:
            pass
        return "http"  # Default to HTTP
    
    def _try_websocket_connection(self, base_url: str, project_id: str) -> bool:
        """Try to establish WebSocket connection."""
        try:
            import unreal

            from ..network.websocket_client import WebSocketClient
            
            self.ws_client = WebSocketClient(base_url, project_id)
            self.ws_client.set_action_handler(self._handle_websocket_action)
            
            return self.ws_client.connect()
        except Exception as e:
            import unreal
            unreal.log_warning(f"WebSocket connection failed: {e}")
            return False
    
    def _try_http_polling_connection(self, base_url: str, project_id: str, project_name: str) -> bool:
        """Try to establish HTTP Polling connection."""
        try:
            import unreal

            from ..network.http_polling_client import HTTPPollingClient
            
            print("[HTTP] Creating HTTP client with:")
            print(f"[HTTP]   Base URL: {base_url}")
            print(f"[HTTP]   Project ID: {project_id}")
            print(f"[HTTP]   Project Name: {project_name}")
            
            self.http_client = HTTPPollingClient(base_url, project_id, project_name)
            self.http_client.set_action_handler(self._handle_websocket_action)
            
            print("[HTTP] Calling connect() on HTTP client...")
            result = self.http_client.connect()
            print(f"[HTTP] Connect result: {result}")
            print(f"[HTTP] About to check result and return...")
            print(f"[HTTP] self.http_client = {self.http_client}")
            print(f"[HTTP] self.http_client.connected = {self.http_client.connected if self.http_client else 'N/A'}")
            
            if not result:
                print(f"[HTTP] Result is False, logging error...")
                unreal.log_error("HTTP Polling connection failed - connect() returned False")
            else:
                print(f"[HTTP] Result is True, about to return True...")
            
            print(f"[HTTP] Returning result: {result}")
            return result
        except Exception as e:
            import unreal
            unreal.log_error(f"HTTP Polling connection failed: {e}")
            import traceback
            traceback.print_exc()
            return False
    
    def _execute_action_wrapper(self, action: str, params: dict) -> dict:
        """
        Wrapper for executing actions - used by action queue for thread-safe execution.
        This method will be called on the main thread by the action queue ticker.
        
        Args:
            action: Action name (e.g., 'browse_files', 'get_project_info')
            params: Action parameters
        
        Returns:
            Action result dict with success status and data/error
        """
        try:
            # Execute action using action executor
            result = self.executor.execute(action, params)
            
            # Ensure result is a dict
            if not isinstance(result, dict):
                result = {"success": True, "data": result}
            
            return result
        except Exception as e:
            return {
                "success": False,
                "error": str(e)
            }
    
    def _handle_websocket_action(self, action: str, params: dict) -> dict:
        """
        Handle actions requested from dashboard via WebSocket/HTTP Polling.
        For HTTP polling, this will queue the action for main thread execution.
        For WebSocket, it may execute directly (if on main thread).
        
        Args:
            action: Action name (e.g., 'browse_files', 'get_project_info')
            params: Action parameters
        
        Returns:
            Action result dict
        """
        try:
            # If we have an action queue and we're on a background thread, use it
            import threading
            if self.action_queue and threading.current_thread() != threading.main_thread():
                # Queue for main thread execution (thread-safe)
                success, result = self.action_queue.queue_action(action, params, timeout=30.0)
                return result
            else:
                # Execute directly (we're on main thread or no queue available)
                result = self.executor.execute(action, params)
                
                return {
                    "success": True,
                    "data": result
                }
        except Exception as e:
            return {
                "success": False,
                "error": str(e)
            }


# Global assistant instance
_assistant: Optional[AIAssistant] = None

# Persistent client references that survive module reloads
_persistent_http_client = None
_persistent_ws_client = None


def get_assistant() -> AIAssistant:
    """Get or create the global AI assistant."""
    global _assistant, _persistent_http_client, _persistent_ws_client
    
    if _assistant is None:
        _assistant = AIAssistant()
        
        # Restore persistent clients after module reload
        if _persistent_http_client is not None:
            print("[AIAssistant] 🔄 Restoring HTTP client after module reload...")
            _assistant.http_client = _persistent_http_client
        
        if _persistent_ws_client is not None:
            print("[AIAssistant] 🔄 Restoring WebSocket client after module reload...")
            _assistant.ws_client = _persistent_ws_client
    
    return _assistant


def _preserve_clients():
    """Save client references before module reload."""
    global _assistant, _persistent_http_client, _persistent_ws_client
    
    if _assistant is not None:
        _persistent_http_client = _assistant.http_client
        _persistent_ws_client = _assistant.ws_client
        print(f"[AIAssistant] 💾 Preserving clients: HTTP={_persistent_http_client is not None}, WS={_persistent_ws_client is not None}")


# Auto-initialize on import to establish WebSocket connection immediately
def _auto_init():
    """Auto-initialize assistant when module is imported."""
    try:
        import unreal
        unreal.log("=" * 60)
        unreal.log("🤖 UE5 AI Assistant - Initializing...")
        unreal.log("=" * 60)
        
        get_assistant()
        
        # Register toolbar menu for easy access to troubleshooting tools
        try:
            from ..ui.toolbar_menu import register_toolbar_menu
            register_toolbar_menu()
        except Exception as e:
            unreal.log_warning(f"⚠️ Could not register toolbar menu: {e}")
        
        unreal.log("=" * 60)
        unreal.log("✅ AI Assistant initialized successfully!")
        unreal.log("💡 Use: AIAssistant.main.send_command('your question here')")
        unreal.log("💡 Or use toolbar: AI Assistant > Launch Troubleshooter")
        unreal.log("=" * 60)
    except Exception as e:
        import unreal
        unreal.log_error("=" * 60)
        unreal.log_error(f"❌ AI Assistant initialization failed: {e}")
        unreal.log_error("=" * 60)
        import traceback
        traceback.print_exc()

# Run auto-init only if imported in UE5 environment
try:
    import unreal
    _auto_init()
except ImportError:
    # Not in UE5 environment, skip auto-init
    pass


def send_command(user_input: str, use_async: bool = False) -> str:
    """
    Main entry point for Editor Utility Widget.

    Args:
        user_input: The user's command text
        use_async: If True, uses non-blocking async execution.
                   If False (default), blocks editor but is thread-safe.
                   
    IMPORTANT: Async mode has thread safety issues with UE API calls.
    Use sync mode (default) for reliable operation.

    Usage (Recommended - Sync):
        import AIAssistant
        AIAssistant.send_command("what do I see?")
        # Blocks briefly (~15-20s) but all UE API calls are safe
        # Response written to Saved/AIConsole/last_reply.txt

    Usage (Experimental - Async):
        import AIAssistant
        AIAssistant.send_command("what do I see?", use_async=True)
        # Non-blocking but may fail with thread errors
    """
    assistant = get_assistant()
    return assistant.process_command(user_input, use_async=use_async)
