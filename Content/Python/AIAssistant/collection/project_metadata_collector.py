"""
Project metadata collector for UE5 AI Assistant.

Gathers comprehensive project information including:
- Enabled modules and plugins
- Asset counts and types
- Blueprint counts and categories  
- Content folder structure
- Source code file counts
- Project configuration details

Uses UE5.6 Python API (Asset Registry, Plugin Manager, etc.)
"""

import os
from datetime import datetime
from typing import Any, Dict, Optional

# Type checking guard for Unreal module
try:
    import unreal  # type: ignore
    UNREAL_AVAILABLE = True
except ImportError:
    UNREAL_AVAILABLE = False
    print("[ProjectMetadata] Unreal module not available - running in test mode")


class ProjectMetadataCollector:
    """Collects comprehensive metadata about the UE5 project."""
    
    def __init__(self, cache_ttl_seconds: int = 300):
        """Initialize the project metadata collector.
        
        Args:
            cache_ttl_seconds: Cache time-to-live in seconds (default 300 = 5 minutes)
        """
        self.cache: Optional[Dict[str, Any]] = None
        self.cache_timestamp: Optional[datetime] = None
        self.cache_ttl_seconds = cache_ttl_seconds
    
    def collect_all_metadata(self, use_cache: bool = True) -> Dict[str, Any]:
        """
        Collect all project metadata.
        
        Args:
            use_cache: If True, return cached data if available and fresh.
            
        Returns:
            Dictionary containing comprehensive project metadata.
        """
        # Check cache
        if use_cache and self._is_cache_valid():
            return self.cache  # type: ignore
        
        metadata = {
            "project": self._collect_project_info(),
            "modules": self._collect_module_info(),
            "plugins": self._collect_plugin_info(),
            "assets": self._collect_asset_info(),
            "blueprints": self._collect_blueprint_info(),
            "source_code": self._collect_source_code_info(),
            "content_structure": self._collect_content_structure(),
            "timestamp": datetime.now().isoformat()
        }
        
        # Update cache
        self.cache = metadata
        self.cache_timestamp = datetime.now()
        
        return metadata
    
    def _is_cache_valid(self) -> bool:
        """Check if cache is valid and fresh."""
        if not self.cache or not self.cache_timestamp:
            return False
        
        elapsed = (datetime.now() - self.cache_timestamp).total_seconds()
        return elapsed < self.cache_ttl_seconds
    
    def _collect_project_info(self) -> Dict[str, Any]:
        """Collect basic project information."""
        if not UNREAL_AVAILABLE:
            return {
                "name": "TestProject",
                "engine_version": "5.6.0",
                "project_dir": "/test/project"
            }
        
        try:
            return {
                "name": unreal.Paths.get_project_file_path().split('/')[-1].replace('.uproject', ''),  # type: ignore
                "project_dir": unreal.Paths.project_dir(),  # type: ignore
                "content_dir": unreal.Paths.project_content_dir(),  # type: ignore
                "saved_dir": unreal.Paths.project_saved_dir(),  # type: ignore
                "engine_version": unreal.SystemLibrary.get_engine_version()  # type: ignore
            }
        except Exception as e:
            return {"error": str(e)}
    
    def _collect_module_info(self) -> Dict[str, Any]:
        """Collect information about enabled modules."""
        if not UNREAL_AVAILABLE:
            return {
                "total_modules": 0,
                "modules": []
            }
        
        try:
            # Read .uproject file to get module information
            project_file = unreal.Paths.get_project_file_path()  # type: ignore
            if os.path.exists(project_file):
                with open(project_file, 'r') as f:
                    import json
                    project_data = json.load(f)
                    modules = project_data.get("Modules", [])
                    return {
                        "total_modules": len(modules),
                        "modules": [
                            {"name": m.get("Name"), "type": m.get("Type")}
                            for m in modules
                        ]
                    }
            return {
                "total_modules": 0,
                "modules": []
            }
        except Exception as e:
            return {"error": str(e)}
    
    def _collect_plugin_info(self) -> Dict[str, Any]:
        """Collect information about enabled plugins."""
        if not UNREAL_AVAILABLE:
            return {
                "total_plugins": 0,
                "enabled_plugins": []
            }
        
        try:
            # Read .uproject file to get plugin information
            project_file = unreal.Paths.get_project_file_path()  # type: ignore
            if os.path.exists(project_file):
                with open(project_file, 'r') as f:
                    import json
                    project_data = json.load(f)
                    plugins = project_data.get("Plugins", [])
                    return {
                        "total_plugins": len(plugins),
                        "enabled_plugins": [
                            {"name": p.get("Name"), "enabled": p.get("Enabled", True)} 
                            for p in plugins if p.get("Enabled", True)
                        ]
                    }
            return {
                "total_plugins": 0,
                "enabled_plugins": []
            }
        except Exception as e:
            return {"error": str(e)}
    
    def _collect_asset_info(self) -> Dict[str, Any]:
        """Collect asset statistics using Asset Registry."""
        if not UNREAL_AVAILABLE:
            return {
                "total_assets": 0,
                "by_type": {}
            }
        
        try:
            # Get asset registry
            asset_registry = unreal.AssetRegistryHelpers.get_asset_registry()  # type: ignore
            
            # Get all assets in /Game
            ar_filter = unreal.ARFilter(  # type: ignore
                package_paths=['/Game'],
                recursive_paths=True
            )
            
            all_assets = asset_registry.get_assets(ar_filter)
            
            # Count by type
            type_counts: Dict[str, int] = {}
            for asset_data in all_assets:
                asset_class = str(asset_data.asset_class_path.asset_name)
                type_counts[asset_class] = type_counts.get(asset_class, 0) + 1
            
            # Sort by count descending
            sorted_types = sorted(type_counts.items(), key=lambda x: x[1], reverse=True)
            
            return {
                "total_assets": len(all_assets),
                "by_type": dict(sorted_types[:20]),  # Top 20 types
                "unique_types": len(type_counts)
            }
        except Exception as e:
            return {"error": str(e)}
    
    def _collect_blueprint_info(self) -> Dict[str, Any]:
        """Collect Blueprint-specific information."""
        if not UNREAL_AVAILABLE:
            return {
                "total_blueprints": 0,
                "blueprints": []
            }
        
        try:
            # Get asset registry
            asset_registry = unreal.AssetRegistryHelpers.get_asset_registry()  # type: ignore
            
            # Filter for blueprints only
            ar_filter = unreal.ARFilter(  # type: ignore
                class_names=['Blueprint'],
                package_paths=['/Game'],
                recursive_paths=True
            )
            
            blueprints = asset_registry.get_assets(ar_filter)
            
            # Categorize blueprints by path
            by_folder: Dict[str, int] = {}
            blueprint_list = []
            
            for bp_data in blueprints[:100]:  # Limit to 100 for performance
                package_path = str(bp_data.package_name)
                folder = '/'.join(package_path.split('/')[:-1])
                by_folder[folder] = by_folder.get(folder, 0) + 1
                
                blueprint_list.append({
                    "name": str(bp_data.asset_name),
                    "path": package_path,
                    "class": str(bp_data.asset_class_path.asset_name)
                })
            
            return {
                "total_blueprints": len(blueprints),
                "by_folder": dict(
                    sorted(
                        by_folder.items(),
                        key=lambda x: x[1],
                        reverse=True
                    )[:10]
                ),
                "blueprints": blueprint_list[:50]  # First 50 blueprints
            }
        except Exception as e:
            return {"error": str(e)}
    
    def _collect_source_code_info(self) -> Dict[str, Any]:
        """Collect source code file statistics."""
        if not UNREAL_AVAILABLE:
            source_dir = os.path.join(os.getcwd(), "Source")
        else:
            project_dir = unreal.Paths.project_dir()  # type: ignore
            source_dir = os.path.join(project_dir, "Source")
        
        if not os.path.exists(source_dir):
            return {
                "has_source_code": False,
                "total_files": 0
            }
        
        try:
            # Count source files
            cpp_files = 0
            h_files = 0
            cs_files = 0
            
            for _, _, files in os.walk(source_dir):
                for file in files:
                    if file.endswith('.cpp'):
                        cpp_files += 1
                    elif file.endswith('.h'):
                        h_files += 1
                    elif file.endswith('.cs'):
                        cs_files += 1
            
            return {
                "has_source_code": True,
                "total_files": cpp_files + h_files + cs_files,
                "cpp_files": cpp_files,
                "header_files": h_files,
                "csharp_files": cs_files,
                "source_directory": source_dir
            }
        except Exception as e:
            return {"error": str(e)}
    
    def _collect_content_structure(self) -> Dict[str, Any]:
        """Collect content folder structure information."""
        if not UNREAL_AVAILABLE:
            content_dir = os.path.join(os.getcwd(), "Content")
        else:
            content_dir = unreal.Paths.project_content_dir()  # type: ignore
        
        if not os.path.exists(content_dir):
            return {
                "content_folders": [],
                "total_folders": 0
            }
        
        try:
            # Get top-level content folders
            folders = []
            for item in os.listdir(content_dir):
                item_path = os.path.join(content_dir, item)
                if os.path.isdir(item_path):
                    # Count files in folder
                    file_count = sum(len(files) for _, _, files in os.walk(item_path))
                    folders.append({
                        "name": item,
                        "path": item_path,
                        "file_count": file_count
                    })
            
            # Sort by file count
            folders.sort(key=lambda x: x['file_count'], reverse=True)
            
            return {
                "content_folders": folders[:20],  # Top 20 folders
                "total_folders": len(folders)
            }
        except Exception as e:
            return {"error": str(e)}
    
    def get_summary(self) -> str:
        """
        Get a text summary of project metadata.
        
        Returns:
            Formatted text summary for display.
        """
        metadata = self.collect_all_metadata()
        
        summary = "📊 **Project Metadata Summary**\n\n"
        
        # Project info
        if "project" in metadata and not metadata["project"].get("error"):
            proj = metadata["project"]
            summary += f"**Project:** {proj.get('name', 'Unknown')}\n"
            summary += f"**Engine:** {proj.get('engine_version', 'Unknown')}\n\n"
        
        # Assets
        if "assets" in metadata and not metadata["assets"].get("error"):
            assets = metadata["assets"]
            summary += f"**Total Assets:** {assets.get('total_assets', 0)}\n"
            if "by_type" in assets:
                summary += "**Top Asset Types:**\n"
                for asset_type, count in list(assets["by_type"].items())[:5]:
                    summary += f"  • {asset_type}: {count}\n"
            summary += "\n"
        
        # Blueprints
        if "blueprints" in metadata and not metadata["blueprints"].get("error"):
            bp = metadata["blueprints"]
            summary += f"**Total Blueprints:** {bp.get('total_blueprints', 0)}\n\n"
        
        # Source code
        if "source_code" in metadata and not metadata["source_code"].get("error"):
            src = metadata["source_code"]
            if src.get("has_source_code"):
                summary += (
                    f"**Source Files:** {src.get('total_files', 0)} "
                    f"({src.get('cpp_files', 0)} .cpp, "
                    f"{src.get('header_files', 0)} .h)\n\n"
                )
        
        return summary


# Module-level instance for easy import
_collector_instance: Optional[ProjectMetadataCollector] = None

def get_collector() -> ProjectMetadataCollector:
    """Get or create the singleton collector instance."""
    global _collector_instance
    if _collector_instance is None:
        _collector_instance = ProjectMetadataCollector()
    return _collector_instance
