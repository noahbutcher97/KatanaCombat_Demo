"""
Project Registration - Auto-register UE5 project with backend
"""
import hashlib
from typing import Any, Dict

import unreal  # type: ignore


class ProjectRegistration:
    """Handles automatic project registration with backend."""
    
    def __init__(self, api_client):
        self.api_client = api_client
    
    def register_current_project(self) -> Dict[str, Any]:
        """
        Register the current UE5 project with the backend.
        
        Returns:
            Registration result
        """
        try:
            # Collect project information
            project_path = unreal.Paths.project_dir()
            project_name = unreal.SystemLibrary.get_game_name()
            engine_version = unreal.SystemLibrary.get_engine_version()
            
            # Normalize path: remove quotes and trailing slashes
            project_path = project_path.strip().strip('"').strip("'").rstrip('/\\')
            
            # Generate unique project ID from path
            project_id = hashlib.md5(project_path.encode()).hexdigest()
            
            # Collect project metadata
            project_data = {
                "name": project_name,
                "path": project_path,
                "version": engine_version,
                "metadata": self._collect_project_metadata()
            }
            
            # Send to backend
            response = self.api_client.post_json(
                "/api/register_project",
                {
                    "project_id": project_id,
                    "project_data": project_data
                }
            )
            
            if response.get("success"):
                unreal.log(f"✅ Project registered: {project_name}")
                return response
            else:
                error = response.get("error", "Unknown error")
                unreal.log_warning(f"❌ Registration failed: {error}")
                return response
                
        except Exception as e:
            unreal.log_error(f"❌ Project registration error: {e}")
            return {"success": False, "error": str(e)}
    
    def _collect_project_metadata(self) -> Dict[str, Any]:
        """Collect project metadata for registration."""
        try:
            # Get basic project stats
            asset_registry = unreal.AssetRegistryHelpers.get_asset_registry()
            
            # Count blueprints
            blueprint_filter = unreal.ARFilter(
                class_names=["Blueprint"],
                recursive_paths=True
            )
            blueprints = asset_registry.get_assets(blueprint_filter)
            
            # Count C++ classes
            cpp_filter = unreal.ARFilter(
                class_names=["Class"],
                recursive_paths=True
            )
            cpp_classes = asset_registry.get_assets(cpp_filter)
            
            metadata = {
                "blueprint_count": len(blueprints),
                "cpp_class_count": len(cpp_classes),
                "content_path": unreal.Paths.project_content_dir(),
                "plugins_enabled": True  # Could enumerate plugins
            }
            
            return metadata
            
        except Exception as e:
            unreal.log_warning(f"Failed to collect metadata: {e}")
            return {}


def auto_register_project(api_client):
    """
    Auto-register project on startup.
    Call this from main.py initialization.
    """
    registration = ProjectRegistration(api_client)
    return registration.register_current_project()
