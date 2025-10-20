"""
Blueprint screenshot capture for UE5.6.

IMPORTANT: UE5.6 Python API does not provide direct blueprint graph
capture. This module uses viewport screenshot functionality which requires:
- Blueprint must be open in the Blueprint Editor
- Viewport must be focused on the graph area you want to capture

For automated blueprint graph capture, consider using the
"Blueprint Screenshot Tool" plugin.
"""
import base64
import os
from datetime import datetime
from typing import TYPE_CHECKING, Any, Dict, Optional

if TYPE_CHECKING:
    import unreal  # type: ignore

try:
    import unreal  # type: ignore
    UNREAL_AVAILABLE = True
except ImportError:
    UNREAL_AVAILABLE = False
    print("[BlueprintCapture] Unreal module not available")


class BlueprintCapture:
    """Handles blueprint screenshot capture using UE5.6 viewport
    screenshot functionality."""
    
    def __init__(self):
        """Initialize blueprint capture."""
        if UNREAL_AVAILABLE:
            self.screenshot_dir = os.path.join(
                unreal.Paths.project_saved_dir(),  # type: ignore
                "Screenshots",
                "AIAssistant"
            )
        else:
            self.screenshot_dir = os.path.join(
                os.getcwd(), "Saved", "Screenshots", "AIAssistant"
            )
        
        os.makedirs(self.screenshot_dir, exist_ok=True)
    
    def check_blueprint_exists(self, blueprint_path: str) -> Dict[str, Any]:
        """
        Check if a Blueprint asset exists in the project.
        
        Args:
            blueprint_path: UE asset path like '/Game/Blueprints/BP_MyActor'
            
        Returns:
            Dictionary with existence check and blueprint info.
        """
        if not UNREAL_AVAILABLE:
            return {
                "exists": False,
                "error": "Unreal module not available"
            }
        
        try:
            # Get asset subsystem
            asset_subsystem = unreal.get_editor_subsystem(  # type: ignore
                unreal.EditorAssetSubsystem  # type: ignore
            )
            
            # Check if asset exists
            exists = asset_subsystem.does_asset_exist(blueprint_path)
            
            if exists:
                # Try to get asset data
                asset_data = asset_subsystem.find_asset_data(blueprint_path)
                return {
                    "exists": True,
                    "asset_name": str(asset_data.asset_name),
                    "asset_class": str(asset_data.asset_class_path.asset_name),
                    "package_name": str(asset_data.package_name)
                }
            else:
                return {
                    "exists": False,
                    "message": f"Blueprint not found: {blueprint_path}"
                }
        except Exception as e:
            return {
                "exists": False,
                "error": str(e)
            }
    
    def list_available_blueprints(self, search_path: str = '/Game') -> Dict[str, Any]:
        """
        List available Blueprint assets in the project.
        
        Args:
            search_path: Content path to search (default: '/Game')
            
        Returns:
            Dictionary with list of blueprint assets.
        """
        if not UNREAL_AVAILABLE:
            return {
                "blueprints": [],
                "count": 0,
                "error": "Unreal module not available"
            }
        
        try:
            # Get asset registry
            asset_registry = unreal.AssetRegistryHelpers.get_asset_registry()  # type: ignore
            
            # Create filter for Blueprint assets
            ar_filter = unreal.ARFilter(  # type: ignore
                class_names=['Blueprint'],
                package_paths=[search_path],
                recursive_paths=True
            )
            
            # Get assets
            assets = asset_registry.get_assets(ar_filter)
            
            blueprints = []
            for asset_data in assets[:50]:  # Limit to 50 for performance
                blueprints.append({
                    "name": str(asset_data.asset_name),
                    "path": str(asset_data.package_name),
                    "class": str(asset_data.asset_class_path.asset_name)
                })
            
            return {
                "blueprints": blueprints,
                "count": len(blueprints),
                "search_path": search_path
            }
        except Exception as e:
            return {
                "blueprints": [],
                "count": 0,
                "error": str(e)
            }
    
    def capture_viewport_screenshot(
        self,
        blueprint_name: Optional[str] = None,
        blueprint_path: Optional[str] = None,
        resolution_multiplier: int = 2,
        check_blueprint: bool = True
    ) -> Dict[str, Any]:
        """
        Capture screenshot of current viewport (Blueprint Editor must be
        open and focused).
        
        Args:
            blueprint_name: Name of the blueprint (for filename/metadata).
            blueprint_path: UE asset path to verify blueprint exists (optional).
            resolution_multiplier: Screenshot resolution multiplier (1-4).
            check_blueprint: If True, verify blueprint exists before capture.
            
        Returns:
            Dictionary with capture information including file path and metadata.
            
        Note:
            This captures whatever is currently visible in the viewport.
            For blueprint graphs, ensure the Blueprint Editor is open and focused.
        """
        # Pre-capture validation if blueprint path provided
        if check_blueprint and blueprint_path and UNREAL_AVAILABLE:
            bp_check = self.check_blueprint_exists(blueprint_path)
            if not bp_check.get("exists"):
                # Blueprint doesn't exist - provide helpful guidance
                available = self.list_available_blueprints()
                
                error_msg = f"Blueprint not found: {blueprint_path}\n\n"
                error_msg += (
                    "📋 **Instructions for Blueprint Screenshot:**\n"
                )
                error_msg += (
                    "1. Open the desired Blueprint in the "
                    "Blueprint Editor\n"
                )
                error_msg += (
                    "2. Focus on the graph you want to capture\n"
                )
                error_msg += "3. Run the capture command again\n\n"
                
                if available.get("count", 0) > 0:
                    error_msg += (
                        f"✅ Found {available['count']} blueprints "
                        "in your project:\n"
                    )
                    for bp in available["blueprints"][:5]:
                        error_msg += f"  • {bp['name']} ({bp['path']})\n"
                    if available["count"] > 5:
                        error_msg += f"  ... and {available['count'] - 5} more\n"
                
                return {
                    "success": False,
                    "error": "blueprint_not_found",
                    "message": error_msg,
                    "available_blueprints": available.get("blueprints", []),
                    "timestamp": datetime.now().isoformat()
                }
        
        # Remind user to open Blueprint editor
        if UNREAL_AVAILABLE:
            reminder_msg = "\n⚠️ **Important:** Make sure:\n"
            reminder_msg += "1. The Blueprint Editor is currently open\n"
            reminder_msg += "2. The Blueprint tab is focused (click on it)\n"
            reminder_msg += "3. The graph view shows what you want to capture\n\n"
            unreal.log_warning(reminder_msg)  # type: ignore
        
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        
        if blueprint_name:
            filename = f"{blueprint_name}_{timestamp}"
        else:
            filename = f"Viewport_{timestamp}"
        
        capture_id = f"capture_{timestamp}"
        destination_path = os.path.join(self.screenshot_dir, f"{filename}.png")
        
        if UNREAL_AVAILABLE:
            try:
                # Get screenshot directory before capture
                screenshots_base = unreal.Paths.screenshot_dir()  # type: ignore
                
                # Record files before capture
                import glob
                import time
                existing_files = set(
                    glob.glob(os.path.join(screenshots_base, "*.png"))
                )
                
                # Use UE5.6 high-resolution screenshot command
                cmd = f"HighResShot {resolution_multiplier}"
                unreal.SystemLibrary.execute_console_command(None, cmd)  # type: ignore
                
                # Wait for screenshot to be written (max 2 seconds)
                source_path = None
                for _ in range(10):
                    time.sleep(0.2)
                    current_files = set(
                        glob.glob(
                            os.path.join(screenshots_base, "*.png")
                        )
                    )
                    new_files = current_files - existing_files
                    if new_files:
                        source_path = max(new_files, key=os.path.getmtime)
                        break
                
                if source_path:
                    # Copy to our managed directory
                    import shutil
                    shutil.copy2(source_path, destination_path)
                    
                    result = {
                        "capture_id": capture_id,
                        "blueprint_name": blueprint_name or "Unknown",
                        "blueprint_path": "",
                        "timestamp": datetime.now().isoformat(),
                        "file_path": destination_path,
                        "original_path": source_path,
                        "resolution_multiplier": resolution_multiplier,
                        "success": True,
                        "message": (
                            f"Screenshot captured and saved to "
                            f"{destination_path}"
                        )
                    }
                    
                    unreal.log(f"[BlueprintCapture] Screenshot captured: {filename}")  # type: ignore
                    return result
                else:
                    return {
                        "capture_id": capture_id,
                        "blueprint_name": blueprint_name or "Unknown",
                        "blueprint_path": "",
                        "timestamp": datetime.now().isoformat(),
                        "success": False,
                        "error": "Screenshot file not found after capture",
                        "message": (
                            f"HighResShot executed but file not "
                            f"detected in {screenshots_base}"
                        )
                    }
                
            except Exception as e:
                return {
                    "capture_id": capture_id,
                    "blueprint_name": blueprint_name or "Unknown",
                    "blueprint_path": "",
                    "timestamp": datetime.now().isoformat(),
                    "success": False,
                    "error": str(e),
                    "message": "Failed to capture screenshot"
                }
        else:
            return {
                "capture_id": capture_id,
                "blueprint_name": blueprint_name or "Unknown",
                "blueprint_path": "",
                "timestamp": datetime.now().isoformat(),
                "success": False,
                "error": "Unreal module not available",
                "message": "Cannot capture screenshot outside UE5 environment"
            }
    
    def encode_screenshot_to_base64(self, file_path: str) -> Optional[str]:
        """
        Encode a screenshot file to base64 for transmission to backend.
        
        Args:
            file_path: Path to the screenshot file.
            
        Returns:
            Base64-encoded string or None if file doesn't exist.
        """
        if not os.path.exists(file_path):
            print(f"[BlueprintCapture] Screenshot file not found: {file_path}")
            return None
        
        try:
            with open(file_path, 'rb') as f:
                image_data = f.read()
                base64_str = base64.b64encode(image_data).decode('utf-8')
                return base64_str
        except Exception as e:
            print(f"[BlueprintCapture] Failed to encode screenshot: {e}")
            return None
    
    def get_latest_screenshot(self) -> Optional[str]:
        """
        Get the path to the most recently captured screenshot.
        
        Returns:
            Path to latest screenshot or None.
        """
        if UNREAL_AVAILABLE:
            screenshot_dir = unreal.Paths.screenshot_dir()  # type: ignore
        else:
            screenshot_dir = self.screenshot_dir
        
        if not os.path.exists(screenshot_dir):
            return None
        
        # Find most recent .png file
        screenshots = [
            os.path.join(screenshot_dir, f) 
            for f in os.listdir(screenshot_dir) 
            if f.endswith('.png')
        ]
        
        if not screenshots:
            return None
        
        latest = max(screenshots, key=os.path.getmtime)
        return latest
    
    def prepare_capture_for_backend(
        self,
        blueprint_name: str,
        blueprint_path: str,
        screenshot_path: str
    ) -> Dict[str, Any]:
        """
        Prepare capture data for backend transmission.
        
        Args:
            blueprint_name: Name of the blueprint.
            blueprint_path: UE asset path to the blueprint.
            screenshot_path: File system path to the screenshot.
            
        Returns:
            Dictionary ready for backend /api/blueprints/capture endpoint.
        """
        timestamp = datetime.now().isoformat()
        capture_id = f"capture_{datetime.now().strftime('%Y%m%d_%H%M%S')}"
        
        # Encode screenshot to base64
        base64_image = self.encode_screenshot_to_base64(screenshot_path)
        
        return {
            "blueprint_name": blueprint_name,
            "blueprint_path": blueprint_path,
            "capture_id": capture_id,
            "image_base64": base64_image,
            "image_url": None,
            "metadata": {
                "file_path": screenshot_path,
                "capture_method": "viewport_screenshot",
                "note": "Blueprint must be open in editor for capture"
            },
            "timestamp": timestamp
        }
