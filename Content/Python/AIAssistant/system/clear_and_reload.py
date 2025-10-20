"""
Force clear all AIAssistant modules and reload with fresh code.
Run this in UE5 Python console to get the latest updates.
"""
import sys
import gc

print("=" * 60)
print("🧹 FORCE CLEARING ALL AIASSISTANT MODULES...")
print("=" * 60)

# List all AIAssistant modules
ai_modules = [key for key in list(sys.modules.keys()) if 'AIAssistant' in key]
print(f"Found {len(ai_modules)} AIAssistant modules to clear:")
for mod in ai_modules:
    print(f"  - {mod}")

# Clear ALL AIAssistant modules
for module in ai_modules:
    try:
        del sys.modules[module]
        print(f"✅ Cleared: {module}")
    except Exception as e:
        print(f"⚠️ Failed to clear {module}: {e}")

# Also clear any cached imports
import importlib
importlib.invalidate_caches()

# Force garbage collection
gc.collect()

print("\n" + "=" * 60)
print("🔄 RELOADING AIASSISTANT WITH FRESH CODE...")
print("=" * 60)

# Now reload everything fresh
try:
    from AIAssistant.system import startup
    startup.configure_and_start(force_production=True)
    print("\n✅ AIAssistant reloaded successfully with latest code!")
except Exception as e:
    print(f"\n❌ Failed to reload: {e}")
    import traceback
    traceback.print_exc()