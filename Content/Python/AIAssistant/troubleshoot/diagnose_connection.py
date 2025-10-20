"""
Diagnose Connection Issues
Run in UE5 Python console to see what's going wrong
"""

print("\n" + "=" * 60)
print("🔍 AI Assistant Connection Diagnostics")
print("=" * 60)

try:
    # Step 1: Check if assistant exists
    print("\n1️⃣ Checking if assistant is initialized...")
    from AIAssistant.core import main
    assistant = AIAssistant.main.get_assistant()
    print("   ✅ Assistant instance exists")
    
    # Step 2: Check config
    print("\n2️⃣ Checking configuration...")
    if hasattr(assistant, 'config'):
        config = assistant.config
        print(f"   📍 Backend URL: {config.api_url}")
        print(f"   🔧 Active Server: {config.active_server if hasattr(config, 'active_server') else 'unknown'}")
        if hasattr(config, 'project_id'):
            print(f"   🆔 Project ID: {config.project_id}")
    else:
        print("   ❌ No config found!")
    
    # Step 3: Check HTTP client
    print("\n3️⃣ Checking HTTP polling client...")
    if hasattr(assistant, 'http_client') and assistant.http_client:
        http = assistant.http_client
        print(f"   ✅ HTTP client exists")
        print(f"   🔄 Running: {http.running}")
        print(f"   🔌 Connected: {http.connected}")
        print(f"   📍 Base URL: {http.base_url}")
        print(f"   🆔 Project ID: {http.project_id}")
        print(f"   📦 Project Name: {http.project_name}")
        
        # Check thread
        if hasattr(http, 'poll_thread') and http.poll_thread:
            print(f"   🧵 Poll Thread Alive: {http.poll_thread.is_alive()}")
        else:
            print("   ❌ No poll thread found!")
    else:
        print("   ❌ No HTTP client found!")
    
    # Step 4: Check WebSocket client
    print("\n4️⃣ Checking WebSocket client...")
    if hasattr(assistant, 'ws_client') and assistant.ws_client:
        ws = assistant.ws_client
        print(f"   ✅ WebSocket client exists")
        print(f"   🔌 Connected: {ws.connected}")
    else:
        print("   ℹ️  No WebSocket client (using HTTP polling)")
    
    # Step 5: Test actual connectivity
    print("\n5️⃣ Testing actual server connectivity...")
    import requests
    try:
        test_url = config.api_url if hasattr(assistant, 'config') else "https://ue5-assistant-noahbutcher97.replit.app"
        response = requests.get(f"{test_url}/health", timeout=5)
        print(f"   ✅ Server responds: {response.status_code}")
    except Exception as e:
        print(f"   ❌ Cannot reach server: {e}")
    
    # Step 6: Check if client is registered
    print("\n6️⃣ Checking server registration...")
    try:
        projects_response = requests.get(f"{test_url}/api/projects", timeout=5)
        if projects_response.status_code == 200:
            projects = projects_response.json().get('projects', [])
            project_id = config.project_id if hasattr(config, 'project_id') else 'unknown'
            found = any(p.get('project_id') == project_id for p in projects)
            if found:
                print(f"   ✅ Project registered on server: {project_id}")
            else:
                print(f"   ❌ Project NOT found on server: {project_id}")
                print(f"   📋 Server has {len(projects)} projects registered")
    except Exception as e:
        print(f"   ❌ Cannot check registration: {e}")
    
    print("\n" + "=" * 60)
    print("✅ Diagnostics complete!")
    print("\n💡 If client is running but not registered:")
    print("   Try: from AIAssistant.auto_update import force_restart_assistant; force_restart_assistant()")
    print("=" * 60)
    
except Exception as e:
    print(f"\n❌ Diagnostic failed: {e}")
    import traceback
    traceback.print_exc()
