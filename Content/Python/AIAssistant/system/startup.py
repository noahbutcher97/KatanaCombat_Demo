"""
UE5 AI Assistant - Auto-Startup Script
Automatically configures and initializes the assistant after installation.
"""

def configure_and_start(backend_url=None, force_production=True):
    """Configure backend URL and start the assistant.
    
    Args:
        backend_url: Optional backend URL (defaults to production)
        force_production: If True, always use production server (default: True)
    """
    import sys
    
    try:
        from AIAssistant.core.config import get_config
        config = get_config()
        
        # Step 1: Determine which server to use
        if force_production:
            # ALWAYS use production server by default
            print("🔧 Configuring backend URL: https://ue5-assistant-noahbutcher97.replit.app")
            config.set("active_server", "production")
            print("✅ Configured for PRODUCTION server (auto-selected)")
            print(f"✅ Backend URL: {config.api_url}")
        elif backend_url:
            # Only use provided URL if force_production=False
            print(f"🔧 Configuring backend URL: {backend_url}")
            
            # Check if URL matches a known preset
            matched_preset = None
            for preset_name, preset_url in config.SERVER_PRESETS.items():
                if backend_url.strip().rstrip('/') == preset_url.strip().rstrip('/'):
                    matched_preset = preset_name
                    break
            
            if matched_preset:
                # Use preset if exact match found
                config.set("active_server", matched_preset)
                print(f"✅ Configured for {matched_preset} server")
            else:
                # Use custom URL directly
                config.set("api_base_url", backend_url)
                config.set("active_server", "custom")
                print("✅ Configured for custom server")
            
            print(f"✅ Backend URL: {config.api_url}")
        else:
            # No URL provided and force_production=False - use config default
            print(f"✅ Using default backend URL: {config.api_url}")
        
        # Step 2: Reload all AIAssistant modules to pick up new config
        print("🔄 Reloading AIAssistant modules...")
        modules_to_reload = [mod for mod in list(sys.modules.keys()) if mod.startswith('AIAssistant')]
        for mod in modules_to_reload:
            del sys.modules[mod]
        
        # Step 3: Import and initialize main module
        print("🚀 Initializing AI Assistant...")
        
        # ACTUALLY IMPORT AND INITIALIZE THE ASSISTANT!
        import AIAssistant.core.main
        assistant = AIAssistant.core.main.get_assistant()
        
        print("")
        print("=" * 60)
        print("✅ AI Assistant is ready!")
        print("=" * 60)
        print("💡 Usage: AIAssistant.main.send_command('your question')")
        print("🌐 Dashboard: https://ue5-assistant-noahbutcher97.replit.app/dashboard")
        print("")
        print("🔧 Troubleshooting:")
        print("   import AIAssistant.troubleshoot.troubleshooter as ts")
        print("   ts.reconnect()      # Restart connection")
        print("   ts.test_server()    # Test connectivity")
        print("   ts.help()           # Show all commands")
        print("")
        
    except Exception as e:
        print(f"❌ Startup failed: {e}")
        import traceback
        traceback.print_exc()


# Auto-run if executed directly
if __name__ == "__main__":
    configure_and_start()
