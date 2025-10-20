"""
Enhanced context collection for UE5 scenes.
Gathers viewport, lighting, materials, and environmental data.
"""
from typing import TYPE_CHECKING, Any, Dict, Optional

if TYPE_CHECKING:
    import unreal  # type: ignore

try:
    import unreal  # type: ignore
    HAS_UNREAL = True
except ImportError:
    unreal = None  # type: ignore
    HAS_UNREAL = False

from ..core.utils import Logger


class ContextCollector:
    """Collects comprehensive scene context for AI analysis."""

    def __init__(self):
        self.logger = Logger("ContextCollector", verbose=False)

    def collect_viewport_data(self,
                              include_project_metadata: bool = False
                              ) -> Dict[str, Any]:
        """
        Collect comprehensive viewport and scene data.
        Includes camera, actors, lighting, materials, and environment.
        Optionally includes project-level metadata for enhanced context.
        
        Args:
            include_project_metadata: If True, adds project metadata
        """
        if not HAS_UNREAL:
            self.logger.error("Unreal module not available")
            return {}

        try:
            data = {
                "camera": self._collect_camera_data(),
                "actors": self._collect_actor_data(),
                "lighting": self._collect_lighting_data(),
                "environment": self._collect_environment_data(),
                "selection": self._collect_selection_data(),
            }

            # Optionally add project metadata for enhanced context
            if include_project_metadata:
                data["project_metadata"] = self.collect_project_metadata()

            self.logger.success("Context collection complete")
            return data

        except Exception as e:
            self.logger.error(f"Context collection failed: {e}")
            return {}

    def _collect_camera_data(self) -> Dict[str, Any]:
        """Collect camera position and rotation."""
        if not HAS_UNREAL or unreal is None:
            return {}

        try:
            editor_subsys = unreal.get_editor_subsystem(  # type: ignore  # type: ignore
                unreal.UnrealEditorSubsystem  # type: ignore
            )

            has_method = hasattr(editor_subsys,
                                 "get_level_viewport_camera_info")

            if editor_subsys and has_method:
                cam_loc, cam_rot = (
                    editor_subsys.get_level_viewport_camera_info())
            else:
                cam_loc, cam_rot = (
                    unreal.EditorLevelLibrary  # type: ignore
                    .get_level_viewport_camera_info())

            return {
                "location": [cam_loc.x, cam_loc.y, cam_loc.z],
                "rotation": [
                    cam_rot.pitch,
                    cam_rot.yaw,
                    cam_rot.roll,
                ],
            }
        except Exception as e:
            self.logger.warn(f"Camera data unavailable: {e}")
            return {"location": [0, 0, 0], "rotation": [0, 0, 0]}

    def _collect_actor_data(self) -> Dict[str, Any]:
        """Collect actor list with types and counts."""
        if not HAS_UNREAL or unreal is None:
            return {}

        try:
            actor_subsys = unreal.get_editor_subsystem(  # type: ignore  # type: ignore
                unreal.EditorActorSubsystem  # type: ignore
            )

            all_actors = actor_subsys.get_all_level_actors(
            ) if actor_subsys else []

            # Get world for level name
            world = None
            if all_actors:
                world = all_actors[0].get_world()

            # Categorize actors by type
            actor_types: Dict[str, int] = {}
            actor_names = []

            for actor in all_actors[:100]:  # Limit for performance
                actor_name = actor.get_name()
                actor_class = (actor.get_class().get_name()
                               if actor.get_class() else "Unknown")

                actor_names.append(actor_name)

                if actor_class in actor_types:
                    actor_types[actor_class] += 1
                else:
                    actor_types[actor_class] = 1

            return {
                "total": len(all_actors),
                "names": actor_names,
                "types": actor_types,
                "level": world.get_name() if world else "Unknown",
            }
        except Exception as e:
            self.logger.warn(f"Actor data unavailable: {e}")
            return {"total": 0, "names": [], "types": {}}

    def _collect_lighting_data(self) -> Dict[str, Any]:
        """Collect lighting setup information."""
        if not HAS_UNREAL or unreal is None:
            return {}

        try:
            actor_subsys = unreal.get_editor_subsystem(  # type: ignore
                unreal.EditorActorSubsystem)

            if not actor_subsys:
                return {}

            all_actors = actor_subsys.get_all_level_actors()
            lighting_data = {
                "directional_lights": [],
                "point_lights": [],
                "spot_lights": [],
                "sky_atmosphere": None,
            }

            for actor in all_actors:
                actor_class = (actor.get_class().get_name()
                               if actor.get_class() else "")

                # Directional lights
                if "DirectionalLight" in actor_class:
                    light_data = self._extract_light_info(actor)
                    if light_data:
                        lighting_data["directional_lights"].append(light_data)

                # Point lights
                elif "PointLight" in actor_class:
                    light_data = self._extract_light_info(actor)
                    if light_data:
                        lighting_data["point_lights"].append(light_data)

                # Spot lights
                elif "SpotLight" in actor_class:
                    light_data = self._extract_light_info(actor)
                    if light_data:
                        lighting_data["spot_lights"].append(light_data)

                # Sky atmosphere
                elif "SkyAtmosphere" in actor_class:
                    lighting_data["sky_atmosphere"] = actor.get_name()

            return lighting_data
        except Exception as e:
            self.logger.warn(f"Lighting data unavailable: {e}")
            return {}

    def _extract_light_info(self, actor: Any) -> Optional[Dict[str, Any]]:
        """Extract info from a light actor."""
        try:
            light_comp = actor.get_component_by_class(
                unreal.LightComponent  # type: ignore
            )

            if not light_comp:
                return None

            # Get transform
            transform = actor.get_actor_transform()
            location = transform.translation
            rotation = transform.rotation.rotator()

            return {
                "name": actor.get_name(),
                "location": [location.x, location.y, location.z],
                "rotation": [
                    rotation.pitch,
                    rotation.yaw,
                    rotation.roll,
                ],
                "intensity": getattr(light_comp, "intensity", "unknown"),
            }
        except Exception:
            return None

    def _collect_environment_data(self) -> Dict[str, Any]:
        """Collect environmental elements (fog, PP volumes, etc.)."""
        if not HAS_UNREAL or unreal is None:
            return {}

        try:
            actor_subsys = unreal.get_editor_subsystem(  # type: ignore
                unreal.EditorActorSubsystem)

            if not actor_subsys:
                return {}

            all_actors = actor_subsys.get_all_level_actors()
            env_data = {
                "fog": [],
                "post_process_volumes": [],
                "landscape": None,
            }

            for actor in all_actors:
                actor_class = (actor.get_class().get_name()
                               if actor.get_class() else "")

                if "Fog" in actor_class:
                    env_data["fog"].append(actor.get_name())
                elif "PostProcessVolume" in actor_class:
                    env_data["post_process_volumes"].append(actor.get_name())
                elif "Landscape" in actor_class:
                    env_data["landscape"] = actor.get_name()

            return env_data
        except Exception as e:
            self.logger.warn(f"Environment data unavailable: {e}")
            return {}

    def _collect_selection_data(self) -> Dict[str, Any]:
        """Collect selected actor information with materials."""
        if not HAS_UNREAL or unreal is None:
            return {}

        try:
            actor_subsys = unreal.get_editor_subsystem(  # type: ignore
                unreal.EditorActorSubsystem)

            if not actor_subsys:
                return {}

            selected = actor_subsys.get_selected_level_actors()

            if not selected:
                return {"count": 0, "actors": []}

            selection_data = {"count": len(selected), "actors": []}

            for actor in selected[:10]:  # Limit to 10 for performance
                actor_info = {
                    "name":
                    actor.get_name(),
                    "class": (actor.get_class().get_name()
                              if actor.get_class() else "Unknown"),
                    "location":
                    None,
                    "materials": [],
                }

                # Get location
                try:
                    transform = actor.get_actor_transform()
                    loc = transform.translation
                    actor_info["location"] = [loc.x, loc.y, loc.z]
                except Exception:
                    pass

                # Get materials (if it has a static mesh component)
                try:
                    mesh_comp = actor.get_component_by_class(
                        unreal.StaticMeshComponent)
                    if mesh_comp:
                        num_materials = mesh_comp.get_num_materials()
                        for i in range(num_materials):
                            mat = mesh_comp.get_material(i)
                            if mat:
                                mat_name = mat.get_name()
                                actor_info["materials"].append(mat_name)
                except Exception:
                    pass

                selection_data["actors"].append(actor_info)

            return selection_data
        except Exception as e:
            self.logger.warn(f"Selection data unavailable: {e}")
            return {"count": 0, "actors": []}

    def collect_project_metadata(self) -> Dict[str, Any]:
        """
        Collect project-level metadata for enhanced AI context.
        Includes project name, size, content folder stats, and asset summaries.
        """
        if not HAS_UNREAL or unreal is None:
            return {}

        try:
            from pathlib import Path

            metadata = {
                "project_name": "",
                "project_path": "",
                "content_folder_stats": {},
                "source_code_stats": {},
                "asset_summary": {},
            }

            # Get project paths
            project_dir = Path(unreal.Paths.project_dir())
            content_dir = Path(unreal.Paths.project_content_dir())

            metadata["project_name"] = project_dir.name
            metadata["project_path"] = str(project_dir)

            # Analyze content folder
            if content_dir.exists():
                asset_registry = unreal.AssetRegistryHelpers.get_asset_registry(
                )

                # Get all assets
                all_assets = asset_registry.get_assets_by_path("/Game",
                                                               recursive=True)

                # Categorize assets by type
                asset_counts: Dict[str, int] = {}
                for asset_data in all_assets[:500]:  # Limit for performance
                    asset_class = str(asset_data.asset_class_path.asset_name)
                    asset_counts[asset_class] = asset_counts.get(
                        asset_class, 0) + 1

                metadata["asset_summary"] = {
                    "total_assets":
                    len(all_assets),
                    "asset_types":
                    asset_counts,
                    "top_asset_types":
                    sorted(asset_counts.items(),
                           key=lambda x: x[1],
                           reverse=True)[:10]
                }

            # Analyze source code if exists
            source_dir = project_dir / "Source"
            if source_dir.exists():
                cpp_files = list(source_dir.rglob("*.cpp"))
                h_files = list(source_dir.rglob("*.h"))

                metadata["source_code_stats"] = {
                    "has_source": True,
                    "cpp_file_count": len(cpp_files),
                    "header_file_count": len(h_files),
                    "total_code_files": len(cpp_files) + len(h_files)
                }
            else:
                metadata["source_code_stats"] = {"has_source": False}

            # Content folder file stats
            if content_dir.exists():
                uasset_files = list(content_dir.rglob("*.uasset"))
                umap_files = list(content_dir.rglob("*.umap"))

                metadata["content_folder_stats"] = {
                    "uasset_count":
                    len(uasset_files),
                    "level_count":
                    len(umap_files),
                    "total_mb":
                    sum(f.stat().st_size for f in uasset_files[:1000]) /
                    (1024 * 1024) if uasset_files else 0
                }

            self.logger.success("Project metadata collection complete")
            return metadata

        except Exception as e:
            self.logger.error(f"Project metadata collection failed: {e}")
            return {}


# Global collector instance
_collector: Optional[ContextCollector] = None


def get_collector() -> ContextCollector:
    """Get or create the global context collector."""
    global _collector
    if _collector is None:
        _collector = ContextCollector()
    return _collector
