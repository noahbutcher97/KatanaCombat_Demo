"""
UE5 AI Assistant - Connection Troubleshooting Tools
Simple Python-based troubleshooting utilities accessible from console.

Usage:
    from AIAssistant.troubleshoot import troubleshooter as ts
    ts.reconnect()              # Quick reconnect
    ts.test_server()            # Test server connectivity
    ts.status()                 # Show connection status
    ts.info()                   # Show connection details
    ts.dashboard()              # Open web dashboard
"""
import sys


def reconnect():
    """Restart the HTTP polling connection."""
    try:
        import unreal
        unreal.log("🔄 Restarting AI Assistant connection...")
        
        from AIAssistant.core import main as assistant_main
        assistant = assistant_main.get_assistant()
        
        if not hasattr(assistant, 'http_client') or not assistant.http_client:
            print("❌ HTTP client not available")
            return
        
        # Disconnect then reconnect
        assistant.http_client.disconnect()
        unreal.log("🔌 Disconnected...")
        
        import time
        time.sleep(1)
        
        result = assistant.http_client.connect()
        if result:
            unreal.log("✅ Reconnected successfully!")
            print("\n✅ Reconnection successful! Check Output Log for details\n")
        else:
            unreal.log_warning("⚠️ Reconnection failed - check network/server")
            print("\n⚠️ Reconnection failed - check Output Log for errors\n")
        
    except Exception as e:
        print(f"❌ Reconnect failed: {e}")
        import traceback
        traceback.print_exc()


def test_server():
    """Test connection to the dashboard server."""
    try:
        import unreal
        unreal.log("🧪 Testing server connection...")
        
        # Get the assistant instance to access http_client
        from AIAssistant.core import main as assistant_main
        assistant = assistant_main.get_assistant()
        
        if not hasattr(assistant, 'http_client') or not assistant.http_client:
            print("❌ HTTP client not available")
            return
        
        import requests
        base_url = assistant.http_client.base_url
        
        unreal.log(f"Testing: {base_url}")
        response = requests.get(f"{base_url}/api/config", timeout=5)
        
        if response.status_code == 200:
            unreal.log("✅ Server is reachable!")
            print(f"\n✅ Server test: SUCCESS\n   URL: {base_url}\n")
        else:
            unreal.log_warning(f"⚠️ Server returned: {response.status_code}")
            print(f"\n⚠️ Server returned status: {response.status_code}\n")
            
    except Exception as e:
        import unreal
        unreal.log_error(f"❌ Server test failed: {e}")
        print(f"\n❌ Server test failed: {e}\n")


def status():
    """Show current connection status."""
    try:
        import unreal
        from AIAssistant.core import main as assistant_main
        assistant = assistant_main.get_assistant()
        
        if not hasattr(assistant, 'http_client') or not assistant.http_client:
            print("\n❌ HTTP client not available\n")
            return
        
        client = assistant.http_client
        
        if client.connected:
            print(f"\n✅ CONNECTED to {client.base_url}\n")
        else:
            print(f"\n⚠️ NOT CONNECTED (polling may have stopped)\n")
            print("   Run: ts.reconnect() to restart\n")
            
    except Exception as e:
        print(f"\n❌ Status check failed: {e}\n")


def info():
    """Display detailed connection information."""
    try:
        from AIAssistant.core import main as assistant_main
        assistant = assistant_main.get_assistant()
        
        if not hasattr(assistant, 'http_client') or not assistant.http_client:
            print("\n❌ HTTP client not available\n")
            return
        
        client = assistant.http_client
        
        print("\n" + "=" * 60)
        print("🔧 AI ASSISTANT CONNECTION INFO")
        print("=" * 60)
        print(f"Backend URL:     {client.base_url}")
        print(f"Project ID:      {client.project_id[:24]}...")
        print(f"Project Name:    {client.project_name}")
        print(f"Connected:       {'Yes ✅' if client.connected else 'No ⚠️'}")
        print(f"Poll Interval:   {client.poll_interval}s")
        print(f"Heartbeat:       {client.heartbeat_interval}s")
        print("=" * 60)
        print("\n💡 Commands:")
        print("   ts.reconnect()  - Restart connection")
        print("   ts.test_server() - Test server connectivity")
        print("   ts.dashboard()   - Open web dashboard")
        print("")
        
    except Exception as e:
        print(f"\n❌ Info retrieval failed: {e}\n")


def dashboard():
    """Open the web dashboard in default browser."""
    try:
        from AIAssistant.core import main as assistant_main
        assistant = assistant_main.get_assistant()
        
        if not hasattr(assistant, 'http_client') or not assistant.http_client:
            print("\n❌ HTTP client not available\n")
            return
        
        import webbrowser
        url = assistant.http_client.base_url + "/dashboard"
        webbrowser.open(url)
        
        import unreal
        unreal.log(f"🌐 Opening dashboard: {url}")
        print(f"\n🌐 Opening dashboard in browser...\n   {url}\n")
        
    except Exception as e:
        print(f"\n❌ Failed to open dashboard: {e}\n")


def help():
    """Show available troubleshooting commands."""
    print("\n" + "=" * 60)
    print("🔧 AI ASSISTANT TROUBLESHOOTING TOOLS")
    print("=" * 60)
    print("\nQuick Commands:")
    print("  ts.reconnect()     - Restart HTTP polling connection")
    print("  ts.test_server()   - Test server connectivity")
    print("  ts.status()        - Show connection status")
    print("  ts.info()          - Display connection details")
    print("  ts.dashboard()     - Open web dashboard")
    print("  ts.help()          - Show this help message")
    print("\nCommon Issues:")
    print("  • Polling stopped → ts.reconnect()")
    print("  • Can't reach server → ts.test_server()")
    print("  • Need connection info → ts.info()")
    print("\nFull docs: AIAssistant/TROUBLESHOOTING.md")
    print("=" * 60 + "\n")


# Auto-display help on import
print("\n✅ Troubleshooting tools loaded!")
print("   Type: ts.help() for available commands")
print("   Quick reconnect: ts.reconnect()")
print("")
