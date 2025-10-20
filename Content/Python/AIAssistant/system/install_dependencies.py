"""
Install required Python dependencies in UE5's Python environment.
Run in UE5 Python Console: import AIAssistant.install_dependencies
"""

def install_websocket_client():
    """Install websocket-client library in UE5 Python environment."""
    import subprocess
    import sys

    import unreal
    
    unreal.log("=" * 60)
    unreal.log("📦 Installing AI Assistant Dependencies")
    unreal.log("=" * 60)
    
    try:
        # Try importing websocket first
        import websocket
        unreal.log("✅ websocket-client already installed")
        unreal.log(f"   Version: {websocket.__version__}")
        return True
    except ImportError:
        unreal.log("⚠️  websocket-client not found, installing...")
        
        try:
            # Install using pip in UE5's Python environment
            result = subprocess.run(
                [sys.executable, "-m", "pip", "install", "websocket-client"],
                capture_output=True,
                text=True,
                timeout=60
            )
            
            if result.returncode == 0:
                unreal.log("✅ websocket-client installed successfully!")
                unreal.log("=" * 60)
                unreal.log("💡 Next step: from AIAssistant.core import main")
                unreal.log("=" * 60)
                return True
            else:
                unreal.log_error(f"❌ Installation failed: {result.stderr}")
                return False
                
        except subprocess.TimeoutExpired:
            unreal.log_error("❌ Installation timed out after 60 seconds")
            return False
        except Exception as e:
            unreal.log_error(f"❌ Installation error: {e}")
            import traceback
            traceback.print_exc()
            return False

# Run automatically when imported
install_websocket_client()
