"""
UE5 AI Assistant - Connection Troubleshooter Widget
Editor Utility Widget for easy reconnection and diagnostics.
Appears in Tools menu: Tools > AI Assistant > Connection Troubleshooter
"""
import unreal


@unreal.uclass()
class ConnectionTroubleshooter(unreal.EditorUtilityWidget):
    """Editor Utility Widget for AI Assistant connection troubleshooting."""

    def __init__(self):
        super().__init__()
        self.assistant_instance = None
        self.http_client = None

    @unreal.ufunction(override=True)
    def construct(self):
        """Called when the widget is constructed."""
        super(ConnectionTroubleshooter, self).construct()
        self._update_status_display()

    def _get_assistant(self):
        """Get the AI Assistant instance."""
        try:
            if not self.assistant_instance:
                from AIAssistant.core import main as assistant_main
                self.assistant_instance = assistant_main.get_assistant()
                if hasattr(self.assistant_instance, 'http_client'):
                    self.http_client = self.assistant_instance.http_client
            return self.assistant_instance
        except Exception as e:
            unreal.log_error(f"Failed to get AI Assistant: {e}")
            return None

    def _update_status_display(self):
        """Update the connection status display."""
        assistant = self._get_assistant()
        
        if not assistant:
            status_text = "❌ AI Assistant not loaded"
            self._set_status_text(status_text, unreal.LinearColor(1.0, 0.3, 0.3))
            return
        
        if self.http_client and hasattr(self.http_client, 'connected'):
            if self.http_client.connected:
                status_text = f"✅ Connected to {self.http_client.base_url}"
                self._set_status_text(status_text, unreal.LinearColor(0.3, 1.0, 0.3))
            else:
                status_text = "⚠️ Not connected - Click 'Restart Connection'"
                self._set_status_text(status_text, unreal.LinearColor(1.0, 0.8, 0.3))
        else:
            status_text = "ℹ️ Status unknown"
            self._set_status_text(status_text, unreal.LinearColor(0.5, 0.5, 1.0))

    def _set_status_text(self, text, color):
        """Set status text and color (override in Blueprint)."""
        # This will be connected to UI elements in the Blueprint
        pass

    @unreal.ufunction(ret=None, meta=dict(Category="AI Assistant"))
    def restart_connection(self):
        """Restart the HTTP polling connection."""
        unreal.log("🔄 Restarting AI Assistant connection...")
        
        try:
            assistant = self._get_assistant()
            if assistant:
                # Call restart method
                from AIAssistant.core import main as assistant_main
                assistant_main.restart_assistant()
                
                # Update display
                self._update_status_display()
                
                unreal.log("✅ Connection restart initiated!")
                self._show_notification("Connection Restart", "Attempting to reconnect...", unreal.EditorDialog.EAppReturnType.Ok)
            else:
                unreal.log_error("❌ Could not get AI Assistant instance")
                self._show_notification("Error", "AI Assistant not loaded", unreal.EditorDialog.EAppReturnType.Ok)
        except Exception as e:
            unreal.log_error(f"❌ Restart failed: {e}")
            self._show_notification("Error", f"Restart failed: {str(e)}", unreal.EditorDialog.EAppReturnType.Ok)

    @unreal.ufunction(ret=None, meta=dict(Category="AI Assistant"))
    def refresh_status(self):
        """Refresh the connection status display."""
        unreal.log("🔄 Refreshing connection status...")
        self._update_status_display()
        self._show_notification("Status Refreshed", "Connection status updated", unreal.EditorDialog.EAppReturnType.Ok)

    @unreal.ufunction(ret=None, meta=dict(Category="AI Assistant"))
    def test_server_connection(self):
        """Test connection to the dashboard server."""
        unreal.log("🧪 Testing server connection...")
        
        try:
            if not self.http_client:
                assistant = self._get_assistant()
                if not assistant or not hasattr(assistant, 'http_client'):
                    self._show_notification("Error", "HTTP client not available", unreal.EditorDialog.EAppReturnType.Ok)
                    return
                self.http_client = assistant.http_client

            # Try a simple GET request to the server
            import requests
            base_url = self.http_client.base_url
            
            unreal.log(f"Testing connection to: {base_url}")
            response = requests.get(f"{base_url}/api/config", timeout=5)
            
            if response.status_code == 200:
                unreal.log("✅ Server is reachable!")
                self._show_notification(
                    "Server Test: Success",
                    f"Server is reachable at:\n{base_url}",
                    unreal.EditorDialog.EAppReturnType.Ok
                )
            else:
                unreal.log_warning(f"⚠️ Server returned status: {response.status_code}")
                self._show_notification(
                    "Server Test: Warning",
                    f"Server returned status: {response.status_code}",
                    unreal.EditorDialog.EAppReturnType.Ok
                )
        except requests.exceptions.Timeout:
            unreal.log_error("❌ Server connection timeout")
            self._show_notification(
                "Server Test: Failed",
                "Connection timeout - server may be unreachable",
                unreal.EditorDialog.EAppReturnType.Ok
            )
        except Exception as e:
            unreal.log_error(f"❌ Server test failed: {e}")
            self._show_notification(
                "Server Test: Failed",
                f"Error: {str(e)}",
                unreal.EditorDialog.EAppReturnType.Ok
            )

    @unreal.ufunction(ret=None, meta=dict(Category="AI Assistant"))
    def open_dashboard(self):
        """Open the web dashboard in default browser."""
        try:
            if self.http_client:
                import webbrowser
                webbrowser.open(self.http_client.base_url + "/dashboard")
                unreal.log(f"🌐 Opening dashboard: {self.http_client.base_url}/dashboard")
            else:
                unreal.log_error("❌ Dashboard URL not available")
        except Exception as e:
            unreal.log_error(f"❌ Failed to open dashboard: {e}")

    @unreal.ufunction(ret=None, meta=dict(Category="AI Assistant"))
    def show_connection_info(self):
        """Display current connection information."""
        try:
            assistant = self._get_assistant()
            if not assistant or not self.http_client:
                self._show_notification("Connection Info", "AI Assistant not loaded", unreal.EditorDialog.EAppReturnType.Ok)
                return

            info_lines = [
                f"Backend URL: {self.http_client.base_url}",
                f"Project ID: {self.http_client.project_id[:16]}...",
                f"Project Name: {self.http_client.project_name}",
                f"Connected: {'Yes' if self.http_client.connected else 'No'}",
                f"Poll Interval: {self.http_client.poll_interval}s",
            ]
            
            info_text = "\n".join(info_lines)
            unreal.log(f"ℹ️ Connection Info:\n{info_text}")
            self._show_notification("Connection Information", info_text, unreal.EditorDialog.EAppReturnType.Ok)
        except Exception as e:
            unreal.log_error(f"❌ Failed to get connection info: {e}")

    def _show_notification(self, title, message, return_type):
        """Show a notification dialog."""
        try:
            unreal.EditorDialog.show_message(
                title=title,
                message=message,
                message_type=return_type
            )
        except Exception as e:
            unreal.log_error(f"Failed to show notification: {e}")


# Register the widget in the Tools menu
def register_troubleshooter_widget():
    """Register the Connection Troubleshooter in the Tools menu."""
    try:
        # Get the editor utility subsystem
        editor_utility_subsystem = unreal.get_editor_subsystem(unreal.EditorUtilitySubsystem)
        
        # Load the widget blueprint (will be created by user or auto-generated)
        widget_path = "/Game/Python/AIAgentUtilities/ConnectionTroubleshooter"
        widget_asset = unreal.load_asset(widget_path)
        
        if widget_asset:
            # Spawn and register the widget
            editor_utility_subsystem.spawn_and_register_tab(widget_asset)
            unreal.log(f"✅ Connection Troubleshooter registered in Tools menu")
        else:
            unreal.log_warning(f"⚠️ Widget blueprint not found at: {widget_path}")
            unreal.log("Create a Blueprint based on this Python class to enable the Tools menu entry")
    except Exception as e:
        unreal.log_error(f"❌ Failed to register Connection Troubleshooter: {e}")


# Auto-register on import (if desired)
# Uncomment to enable auto-registration:
# register_troubleshooter_widget()
