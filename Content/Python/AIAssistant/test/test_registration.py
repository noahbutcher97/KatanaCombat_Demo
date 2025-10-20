"""
Test script to manually trigger project registration with verbose output.
Run this in UE5 Python Console to diagnose registration issues.
"""
import unreal

def test_registration():
    """Test project registration with detailed logging."""
    try:
        # Import and get client
        from AIAssistant.network.api_client import get_client
        from AIAssistant.system.project_registration import ProjectRegistration
        
        print("=" * 60)
        print("🔧 Testing Project Registration")
        print("=" * 60)
        
        # Get API client
        client = get_client()
        print(f"\n✅ API Client initialized")
        print(f"   Backend URL: {client.config.api_url}")
        
        # Test connection
        print(f"\n📡 Testing connection...")
        try:
            response = client.get_json("/health")
            print(f"   ✅ Backend is reachable: {response}")
        except Exception as e:
            print(f"   ❌ Backend unreachable: {e}")
            print(f"   💡 Check if server is running and URL is correct")
            return
        
        # Attempt registration
        print(f"\n🚀 Attempting project registration...")
        registration = ProjectRegistration(client)
        result = registration.register_current_project()
        
        print(f"\n📋 Registration Result:")
        print(f"   {result}")
        
        if result.get("success"):
            print(f"\n✅ SUCCESS! Project is registered")
            print(f"   Project ID: {result.get('project_id')}")
        else:
            print(f"\n❌ FAILED: {result.get('error', 'Unknown error')}")
        
        print("=" * 60)
        
    except Exception as e:
        print(f"\n❌ ERROR: {e}")
        import traceback
        traceback.print_exc()

# Run the test
test_registration()
