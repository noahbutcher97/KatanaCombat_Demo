"""
Test UE5 AI Assistant Connection
Run this in UE5 Python console to verify connection
"""

print("\n" + "=" * 60)
print("🔍 Testing AI Assistant Connection...")
print("=" * 60)

try:
    # Import and initialize
    print("\n1️⃣ Importing AIAssistant...")
    from AIAssistant.core import main
    
    print("2️⃣ Getting assistant instance...")
    assistant = main.get_assistant()
    
    print("3️⃣ Checking connection status...")
    
    # Check WebSocket/HTTP client
    if assistant.ws_client:
        if assistant.ws_client.connected:
            print("✅ Connected via: " + type(assistant.ws_client).__name__)
            print("✅ Connection is ACTIVE!")
        else:
            print("❌ Client exists but not connected")
    else:
        print("❌ No client connection established")
    
    # Check project registration
    if hasattr(assistant, 'config'):
        print(f"4️⃣ Backend URL: {assistant.config.api_url}")
    
    # Try a simple command
    print("\n5️⃣ Testing command execution...")
    response = main.send_command("test connection")
    if response:
        print("✅ Command executed successfully!")
    
    print("\n" + "=" * 60)
    print("✅ Connection test complete!")
    print("=" * 60)
    
except ImportError as e:
    print(f"❌ Import failed: {e}")
    print("\n📦 Make sure AIAssistant is installed in Content/Python/")
except Exception as e:
    print(f"❌ Test failed: {e}")
    import traceback
    traceback.print_exc()