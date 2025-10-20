"""
Action execution system with command registry pattern.
Handles [UE_REQUEST] tokens and executes Unreal Engine operations.
Thread-safe implementation using action queue for background thread calls.
"""
import threading
from typing import Any, Callable, Dict, Optional, Tuple

try:
    import unreal  # type: ignore
    HAS_UNREAL = True
except ImportError:
    HAS_UNREAL = False
    unreal = None  # type: ignore  # Only available in UE environment

from ..network.api_client import get_client
from ..tools.blueprint_capture import BlueprintCapture
from ..core.config import get_config
from ..collection.context_collector import get_collector
from ..collection.file_collector import FileCollector
from ..collection.project_metadata_collector import get_collector as get_metadata_collector
from ..core.utils import Logger

# Import action queue for thread-safe execution
try:
    from ..execution.action_queue import get_action_queue
    HAS_ACTION_QUEUE = True
except ImportError:
    HAS_ACTION_QUEUE = False
    print("⚠️ ActionExecutor: Action queue not available - thread safety disabled")

# Import new orchestration systems
try:
    from ..tools.actor_manipulator import get_manipulator
    from ..tools.editor_utility_generator import get_utility_generator
    from ..tools.scene_orchestrator import get_orchestrator
    from ..tools.viewport_controller import get_viewport_controller
    HAS_ORCHESTRATION = True
except ImportError:
    HAS_ORCHESTRATION = False


class ActionExecutor:
    """Executes actions in Unreal Engine based on AI commands."""

    def __init__(self):
        self.logger = Logger("ActionExecutor", verbose=False)
        self.config = get_config()
        self.actions: Dict[str, Callable[[], str]] = {}
        self.file_collector = FileCollector()
        self.blueprint_capture = BlueprintCapture()

        # Initialize metadata collector with config cache TTL
        cache_ttl = self.config.get("project_metadata_cache_ttl", 300)
        self.metadata_collector = get_metadata_collector()
        self.metadata_collector.cache_ttl_seconds = cache_ttl
        
        # Track main thread ID for thread safety checks
        self.main_thread_id = threading.main_thread().ident
        
        # Initialize action queue if available
        if HAS_ACTION_QUEUE:
            self.action_queue = get_action_queue()  # type: ignore
            # Don't set handler here - let main.py coordinate the handlers
            # The main.py will set up proper handler that calls our execute_with_queue
            self.logger.info("✅ Thread-safe action queue initialized")
        else:
            self.action_queue = None
            self.logger.info("⚠️ Action queue not available - thread safety disabled")

        self._register_default_actions()

        # Add orchestration action implementations
        if HAS_ORCHESTRATION:
            from ..execution.action_executor_extensions import add_orchestration_actions
            add_orchestration_actions(self)

    def _register_default_actions(self) -> None:
        """Register the default action set."""
        # Existing actions
        self.register("describe_viewport", self._describe_viewport)
        self.register("list_actors", self._list_actors)
        self.register("get_selected_info", self._get_selected_info)

        # File operation actions
        self.register("browse_files", self._browse_files)
        self.register("read_source_files", self._read_source_files)
        self.register("search_files", self._search_files)

        # Project metadata actions
        self.register("show_project_info", self._show_project_info)
        self.register("get_project_info", self._show_project_info)  # Alias

        # Blueprint capture actions
        self.register("capture_blueprint", self._capture_blueprint)
        self.register("list_blueprints", self._list_blueprints)
        
        # System restart actions (for thread-safe restart from background threads)
        self.register("restart_assistant", self._restart_assistant)
        self.register("manual_restart", self._manual_restart)

        # Note: Orchestration actions are registered by action_executor_extensions.py
        # when HAS_ORCHESTRATION is True (see __init__ method above)

    def register(self, action_name: str, handler: Callable[[], str]) -> None:
        """Register a new action handler."""
        self.actions[action_name] = handler
        self.logger.debug(f"Registered action: {action_name}")
    
    def _is_main_thread(self) -> bool:
        """Check if we're running on the main thread."""
        current_thread_id = threading.current_thread().ident
        return current_thread_id == self.main_thread_id

    def execute(self, action_token: str, params: Optional[Dict] = None) -> str:
        """Execute an action by token name.
        Thread-safe implementation that uses action queue when called from background threads.
        
        Args:
            action_token: The action to execute
            params: Optional parameters (currently unused, for future expansion)
        """
        action_name = action_token.lower().strip()

        if action_name not in self.actions:
            error_msg = (f"[UE_ERROR] Unknown action: {action_name}")
            self.logger.error(error_msg)
            return error_msg

        # Check if we're on the main thread
        if self._is_main_thread():
            # On main thread - execute directly
            try:
                self.logger.info(f"[Main Thread] Executing: {action_name}")
                result = self.actions[action_name]()
                self.logger.success(f"[Main Thread] Action completed: {action_name}")
                return result
            except Exception as e:
                error_msg = (f"[UE_ERROR] Action '{action_name}' failed: {e}")
                self.logger.error(error_msg)
                return error_msg
        else:
            # On background thread - use action queue for thread safety
            if self.action_queue:
                self.logger.info(f"[Background Thread] Queueing action: {action_name}")
                try:
                    # Queue the action for main thread execution
                    success, result = self.action_queue.queue_action(
                        action_name, 
                        params or {},
                        timeout=30.0  # 30 second timeout (matches backend)
                    )
                    
                    if success:
                        self.logger.success(f"[Background Thread] Action completed via queue: {action_name}")
                        # Extract the actual result data
                        if isinstance(result, dict) and 'data' in result:
                            return result['data']
                        elif isinstance(result, dict) and 'error' in result:
                            return f"[UE_ERROR] {result['error']}"
                        else:
                            return str(result)
                    else:
                        error_msg = result.get('error', 'Unknown error')
                        self.logger.error(f"[Background Thread] Action failed: {error_msg}")
                        return f"[UE_ERROR] {error_msg}"
                        
                except Exception as e:
                    error_msg = f"[UE_ERROR] Failed to queue action '{action_name}': {e}"
                    self.logger.error(error_msg)
                    return error_msg
            else:
                # No action queue available - fallback to direct execution with warning
                self.logger.info(f"⚠️ [Background Thread] No action queue - executing directly (may cause threading issues)")
                try:
                    result = self.actions[action_name]()
                    return result
                except Exception as e:
                    error_msg = (f"[UE_ERROR] Action '{action_name}' failed (threading issue likely): {e}")
                    self.logger.error(error_msg)
                    return error_msg
    
    def execute_with_queue(self, action_name: str, params: Dict[str, Any]) -> Dict[str, Any]:
        """
        Execute an action using the action queue system.
        This method is called by the action queue's handler.
        
        Args:
            action_name: Name of the action to execute
            params: Parameters for the action
        
        Returns:
            Dict with success status and result data
        """
        action_name = action_name.lower().strip()
        
        if action_name not in self.actions:
            return {
                'success': False,
                'error': f"Unknown action: {action_name}"
            }
        
        try:
            # Execute the action handler
            result = self.actions[action_name]()
            return {
                'success': True,
                'data': result
            }
        except Exception as e:
            return {
                'success': False,
                'error': str(e)
            }

    def _describe_viewport(self) -> str:
        """Collect viewport data and send to API for description."""
        if not HAS_UNREAL:
            return "[UE_ERROR] Unreal module not available"

        try:
            collector = get_collector()

            # Check if project metadata collection is enabled
            include_metadata = self.config.get("collect_project_metadata",
                                               True)
            context = collector.collect_viewport_data(
                include_project_metadata=include_metadata)

            if not context:
                return "[UE_ERROR] Failed to collect viewport data"

            client = get_client()
            response = client.describe_viewport(context)

            return response.get(
                "response", response.get("description", "(no description)"))
        except Exception as e:
            return f"[UE_ERROR] Viewport description failed: {e}"

    def _list_actors(self, limit: int = 30) -> str:
        """List actors in the current level."""
        if not HAS_UNREAL or unreal is None:
            return "[UE_ERROR] Unreal module not available"

        try:
            # unreal is available here (checked above, UE environment only)
            ell = unreal.EditorLevelLibrary  # type: ignore[union-attr]
            actors = list(ell.get_all_level_actors())
            names = [a.get_fname() for a in actors]
            total = len(names)

            lines = [f"Found {total} actors in the current level."]
            for name in names[:limit]:
                lines.append(f" - {name}")

            if total > limit:
                lines.append(f"... and {total - limit} more")

            return "\n".join(lines)
        except Exception as e:
            return f"[UE_ERROR] list_actors failed: {e}"

    def _get_selected_info(self) -> str:
        """Get detailed info about selected actors."""
        if not HAS_UNREAL or unreal is None:
            return "[UE_ERROR] Unreal module not available"

        try:
            # unreal is available here (checked above, UE environment only)
            ell = unreal.EditorLevelLibrary  # type: ignore[union-attr]
            selected = list(ell.get_selected_level_actors())

            if not selected:
                return "No actors currently selected."

            lines = [f"Selected Actors: {len(selected)}"]

            for actor in selected:
                name = actor.get_fname()
                cls = (actor.get_class().get_name()
                       if actor.get_class() else "UnknownClass")

                loc_str = ""
                try:
                    t = actor.get_root_component().get_component_transform()
                    x = round(t.translation.x, 1)
                    y = round(t.translation.y, 1)
                    z = round(t.translation.z, 1)
                    loc_str = f" at ({x}, {y}, {z})"
                except Exception:
                    pass

                lines.append(f" - {name} ({cls}){loc_str}")

            return "\n".join(lines)
        except Exception as e:
            return f"[UE_ERROR] get_selected_info failed: {e}"

    def _browse_files(self) -> str:
        """Browse files in the Content directory."""
        if not self.config.get("enable_file_operations", True):
            return "[UE_ERROR] File operations are disabled in configuration"

        try:
            result = self.file_collector.list_directory(
                directory_path=self.file_collector.content_dir,
                recursive=True,
                max_depth=2)

            if result.get("error"):
                return f"[UE_ERROR] {result['error']}"

            lines = [
                f"📁 Content Directory: {result.get('root_path', 'Unknown')}"
            ]
            lines.append(f"Total files: {result.get('total_files', 0)}\n")

            for file in result.get("files", [])[:20]:
                icon = "📁" if file.get("type") == "directory" else "📄"
                lines.append(f"{icon} {file.get('path', 'Unknown')}")

            if result.get("total_files", 0) > 20:
                lines.append(
                    f"\n... and {result['total_files'] - 20} more files")

            return "\n".join(lines)
        except Exception as e:
            return f"[UE_ERROR] browse_files failed: {e}"

    def _read_source_files(self) -> str:
        """Read C++ source files."""
        if not self.config.get("enable_file_operations", True):
            return "[UE_ERROR] File operations are disabled in configuration"

        try:
            result = self.file_collector.get_source_files(max_files=10)

            if result.get("error"):
                return f"[UE_ERROR] {result['error']}"

            root_path = result.get('root_path', 'Unknown')
            lines = [f"📂 Source Directory: {root_path}"]
            types = result.get("file_types", {})
            lines.append(f"C++ files: {types.get('cpp', 0)}, "
                         f"Headers: {types.get('headers', 0)}\n")

            for file in result.get("files", [])[:10]:
                ext = file.get("extension", "")
                lines.append(f"  {ext} {file.get('name', 'Unknown')}")

            return "\n".join(lines)
        except Exception as e:
            return f"[UE_ERROR] read_source_files failed: {e}"

    def _search_files(self) -> str:
        """Search for files by pattern."""
        if not self.config.get("enable_file_operations", True):
            return "[UE_ERROR] File operations are disabled in configuration"

        try:
            # Default search for Blueprint files
            result = self.file_collector.search_files(pattern="BP_",
                                                      max_results=20)

            if result.get("error"):
                return f"[UE_ERROR] {result['error']}"

            lines = [
                f"🔍 Search Results: {result.get('search_query', 'All files')}"
            ]
            lines.append(f"Found {result.get('total_files', 0)} files\n")

            for file in result.get("files", []):
                lines.append(f"  • {file.get('name', 'Unknown')}")

            return "\n".join(lines)
        except Exception as e:
            return f"[UE_ERROR] search_files failed: {e}"

    def _restart_assistant(self) -> str:
        """Restart the assistant with fresh code - thread-safe main thread operation."""
        try:
            # This method is called on the main thread via action queue
            # Safe to perform UI operations here
            self.logger.info("🔄 Executing assistant restart on main thread...")
            
            # Import auto_update module
            import sys
            if 'AIAssistant.auto_update' in sys.modules:
                del sys.modules['AIAssistant.auto_update']
            
            # Import fresh and call force_restart
            import AIAssistant.auto_update as auto_update
            result = auto_update.force_restart_assistant()
            
            if result:
                return "✅ Assistant restarted successfully with fresh code"
            else:
                return "[UE_ERROR] Failed to restart assistant - check logs"
                
        except Exception as e:
            return f"[UE_ERROR] restart_assistant failed: {e}"
    
    def _manual_restart(self) -> str:
        """Manual restart of the assistant - thread-safe main thread operation."""
        try:
            # This method is called on the main thread via action queue
            self.logger.info("🔄 Executing manual restart on main thread...")
            
            # Check if another restart is already in progress
            import AIAssistant.auto_update as auto_update
            if hasattr(auto_update, '_restart_in_progress') and auto_update._restart_in_progress:
                return "⚠️ Restart already in progress - skipping duplicate request"
            
            # Use the guarded restart function instead of manual logic
            result = auto_update.force_restart_assistant()
            
            if result:
                return "✅ Manual restart complete - assistant reloaded"
            else:
                return "[UE_ERROR] Manual restart failed - check logs"
            
        except Exception as e:
            return f"[UE_ERROR] manual_restart failed: {e}"

    def _show_project_info(self) -> str:
        """Show project metadata summary."""
        try:
            summary = self.metadata_collector.get_summary()
            return summary
        except Exception as e:
            return f"[UE_ERROR] show_project_info failed: {e}"

    def _capture_blueprint(self) -> str:
        """Capture screenshot of current Blueprint editor."""
        if not self.config.get("enable_blueprint_capture", True):
            return "[UE_ERROR] Blueprint capture is disabled in configuration"

        try:
            resolution = self.config.get("blueprint_capture_resolution", 2)
            result = self.blueprint_capture.capture_viewport_screenshot(
                blueprint_name="CurrentBlueprint",
                resolution_multiplier=resolution,
                check_blueprint=False  # Don't validate, just capture
            )

            if not result.get("success"):
                error_msg = result.get("message",
                                       result.get("error", "Unknown error"))
                return f"[UE_ERROR] Blueprint capture failed:\n{error_msg}"

            lines = [
                "✅ Blueprint screenshot captured successfully!",
                f"📸 File: {result.get('filename', 'Unknown')}",
                f"💾 Path: {result.get('path', 'Unknown')}"
            ]

            return "\n".join(lines)
        except Exception as e:
            return f"[UE_ERROR] capture_blueprint failed: {e}"

    def _list_blueprints(self) -> str:
        """List available blueprints in the project."""
        if not self.config.get("enable_blueprint_capture", True):
            return "[UE_ERROR] Blueprint operations are disabled in configuration"

        try:
            result = self.blueprint_capture.list_available_blueprints()

            if result.get("error"):
                return f"[UE_ERROR] {result['error']}"

            count = result.get("count", 0)
            lines = [f"📋 Found {count} Blueprints in project:\n"]

            for bp in result.get("blueprints", [])[:15]:
                lines.append(
                    f"  • {bp.get('name', 'Unknown')} - {bp.get('path', '')}")

            if count > 15:
                lines.append(f"\n... and {count - 15} more")

            return "\n".join(lines)
        except Exception as e:
            return f"[UE_ERROR] list_blueprints failed: {e}"


# Global executor instance
_executor: Optional[ActionExecutor] = None


def get_executor() -> ActionExecutor:
    """Get or create the global action executor."""
    global _executor
    if _executor is None:
        _executor = ActionExecutor()
    return _executor
