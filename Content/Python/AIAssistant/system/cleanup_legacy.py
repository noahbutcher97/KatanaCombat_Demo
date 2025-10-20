"""
Cleanup utility to remove legacy duplicate files from old installations.
This removes files that were moved from root to subdirectories during modular refactoring.
"""
from pathlib import Path


def get_legacy_files():
    """List of files that should be removed from AIAssistant root (now in subdirectories)."""
    return [
        # Moved to execution/
        "action_executor.py",
        "action_executor_extensions.py",
        "action_queue.py",

        # Moved to network/
        "api_client.py",
        "async_client.py",
        "http_polling_client.py",
        "websocket_client.py",

        # Moved to core/
        "config.py",
        "main.py",
        "utils.py",

        # Moved to system/
        "auto_update.py",
        "clear_and_reload.py",
        "install_dependencies.py",
        "local_server.py",
        "project_registration.py",
        "startup.py",

        # Moved to tools/
        "actor_manipulator.py",
        "ai_agent_utility_generator.py",
        "blueprint_capture.py",
        "blueprint_helpers.py",
        "editor_utility_generator.py",
        "scene_orchestrator.py",
        "viewport_controller.py",

        # Moved to collection/
        "context_collector.py",
        "file_collector.py",
        "project_metadata_collector.py",

        # Moved to troubleshoot/
        "connection_troubleshooter.py",
        "diagnose.py",
        "troubleshooter.py",
        "launch_troubleshooter.py",

        # Moved to ui/
        "toolbar_menu.py",
        "ui_manager.py",

        # Moved to test/
        "test_connection.py",
        "test_registration.py",

        # Moved to archived/
        "_DEPRECATED_widget_files.txt",
        "README.md",  # Only if duplicate in archived/

        # Moved to Documentation/
        "TROUBLESHOOTING.md",
    ]


def cleanup_legacy_files():
    """Remove legacy duplicate files from AIAssistant root directory."""
    try:
        # Get AIAssistant root directory
        try:
            import unreal
            project_dir = unreal.Paths.project_dir()
            aiassistant_root = Path(
                project_dir) / "Content" / "Python" / "AIAssistant"
        except ImportError:
            # Fallback for non-UE5 environment
            aiassistant_root = Path(__file__).parent.parent

        removed_count = 0
        skipped_count = 0

        print("=" * 60)
        print("🧹 Cleaning up legacy duplicate files...")
        print("=" * 60)

        for filename in get_legacy_files():
            file_path = aiassistant_root / filename

            if file_path.exists():
                try:
                    # Extra safety check - only remove .py and .txt files
                    if file_path.suffix in ['.py', '.txt', '.md']:
                        file_path.unlink()
                        print(f"✅ Removed: {filename}")
                        removed_count += 1
                    else:
                        print(f"⏭️  Skipped (unsafe extension): {filename}")
                        skipped_count += 1
                except Exception as e:
                    print(f"❌ Failed to remove {filename}: {e}")
                    skipped_count += 1
            else:
                # File doesn't exist, already cleaned
                pass

        print("=" * 60)
        print("✅ Cleanup complete!")
        print(f"   Removed: {removed_count} file(s)")
        print(f"   Skipped: {skipped_count} file(s)")
        print("=" * 60)

        return {
            "success": True,
            "removed": removed_count,
            "skipped": skipped_count
        }

    except Exception as e:
        print(f"❌ Cleanup failed: {e}")
        return {"success": False, "error": str(e)}


def cleanup_pycache_recursive():
    """Remove all __pycache__ directories recursively."""
    try:
        # Get AIAssistant root directory
        try:
            import unreal
            project_dir = unreal.Paths.project_dir()
            aiassistant_root = Path(
                project_dir) / "Content" / "Python" / "AIAssistant"
        except ImportError:
            # Fallback for non-UE5 environment
            aiassistant_root = Path(__file__).parent.parent

        removed_count = 0

        print("=" * 60)
        print("🧹 Cleaning up __pycache__ directories...")
        print("=" * 60)

        # Find all __pycache__ directories
        for pycache_dir in aiassistant_root.rglob("__pycache__"):
            try:
                # Remove all files in the directory first
                for file in pycache_dir.glob("*"):
                    if file.is_file():
                        file.unlink()

                # Remove the directory itself
                pycache_dir.rmdir()
                print(
                    f"✅ Removed: {pycache_dir.relative_to(aiassistant_root)}")
                removed_count += 1
            except Exception as e:
                print(f"❌ Failed to remove {pycache_dir}: {e}")

        print("=" * 60)
        print(f"✅ Removed {removed_count} __pycache__ director(ies)")
        print("=" * 60)

        return {"success": True, "removed": removed_count}

    except Exception as e:
        print(f"❌ pycache cleanup failed: {e}")
        return {"success": False, "error": str(e)}


if __name__ == "__main__":
    # Run both cleanups
    cleanup_legacy_files()
    cleanup_pycache_recursive()
