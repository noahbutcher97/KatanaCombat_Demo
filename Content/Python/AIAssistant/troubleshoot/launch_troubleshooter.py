"""
Quick launcher for the Connection Troubleshooter widget.
Run this in UE5 Python Console to open the troubleshooting tools.

Usage:
    import AIAssistant.launch_troubleshooter
"""
import unreal


def launch_troubleshooter():
    """Launch the Connection Troubleshooter widget."""
    try:
        # Import the troubleshooter class
        from AIAssistant.connection_troubleshooter import ConnectionTroubleshooter
        
        # Create an instance of the widget
        widget = ConnectionTroubleshooter()
        
        # Show it in a dockable window
        editor_utility_subsystem = unreal.get_editor_subsystem(unreal.EditorUtilitySubsystem)
        
        # For now, just create the widget instance and log instructions
        unreal.log("=" * 60)
        unreal.log("🔧 AI Assistant Connection Troubleshooter")
        unreal.log("=" * 60)
        unreal.log("")
        unreal.log("Available Commands (run in Python Console):")
        unreal.log("")
        unreal.log("1. Restart Connection:")
        unreal.log("   >>> troubleshooter.restart_connection()")
        unreal.log("")
        unreal.log("2. Refresh Status:")
        unreal.log("   >>> troubleshooter.refresh_status()")
        unreal.log("")
        unreal.log("3. Test Server:")
        unreal.log("   >>> troubleshooter.test_server_connection()")
        unreal.log("")
        unreal.log("4. Show Connection Info:")
        unreal.log("   >>> troubleshooter.show_connection_info()")
        unreal.log("")
        unreal.log("5. Open Dashboard:")
        unreal.log("   >>> troubleshooter.open_dashboard()")
        unreal.log("")
        unreal.log("=" * 60)
        
        # Store widget globally for easy access
        unreal.get_editor_subsystem(unreal.EditorUtilitySubsystem).troubleshooter = widget
        globals()['troubleshooter'] = widget
        
        unreal.log("✅ Troubleshooter loaded! Use 'troubleshooter' variable in console")
        
        return widget
        
    except Exception as e:
        unreal.log_error(f"❌ Failed to launch troubleshooter: {e}")
        import traceback
        traceback.print_exc()
        return None


# Auto-launch on import
troubleshooter = launch_troubleshooter()

if troubleshooter:
    unreal.log("")
    unreal.log("💡 Quick Start:")
    unreal.log("   troubleshooter.restart_connection()  # Reconnect to server")
    unreal.log("   troubleshooter.test_server_connection()  # Test connectivity")
    unreal.log("")
