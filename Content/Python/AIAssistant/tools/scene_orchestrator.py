"""
Scene Orchestrator - AI-Powered Level Building & Actor Spawning
UE 5.6 Compliant - Uses EditorActorSubsystem and modern APIs
"""

from typing import Any, Dict, List, Optional, Tuple

import unreal  # type: ignore


class SceneOrchestrator:
    """Handles spawning and organizing actors in the level."""
    
    def __init__(self):
        self.editor_actor_subsystem = unreal.get_editor_subsystem(
            unreal.EditorActorSubsystem
        )
        self.spawned_actors: List[unreal.Actor] = []
    
    def spawn_static_mesh(
        self,
        asset_path: str,
        location: Tuple[float, float, float] = (0, 0, 0),
        rotation: Tuple[float, float, float] = (0, 0, 0),
        scale: Tuple[float, float, float] = (1, 1, 1),
        label: Optional[str] = None
    ) -> Dict[str, Any]:
        """
        Spawn a static mesh actor.
        
        Args:
            asset_path: Path to static mesh 
                (/Game/Meshes/MyMesh or /Game/StarterContent/...)
            location: (x, y, z) world coordinates
            rotation: (pitch, yaw, roll) in degrees
            scale: (x, y, z) scale factors
            label: Optional actor label for outliner
        
        Returns:
            Dict with 'success', 'actor', 'message'
        """
        try:
            # Load the static mesh asset
            mesh_asset = unreal.EditorAssetLibrary.load_asset(asset_path)
            
            if not mesh_asset:
                return {
                    "success": False,
                    "message": f"Failed to load asset: {asset_path}"
                }
            
            if not isinstance(mesh_asset, unreal.StaticMesh):
                return {
                    "success": False,
                    "message": f"Asset is not a StaticMesh: {asset_path}"
                }
            
            # Spawn StaticMeshActor
            spawn_loc = unreal.Vector(*location)
            spawn_rot = unreal.Rotator(*rotation)
            
            actor = self.editor_actor_subsystem.spawn_actor_from_class(
                unreal.StaticMeshActor.static_class(),
                spawn_loc,
                spawn_rot
            )
            
            if not actor:
                return {
                    "success": False,
                    "message": "Failed to spawn actor"
                }
            
            # Set the mesh
            actor.static_mesh_component.set_static_mesh(mesh_asset)
            
            # Set scale
            actor.set_actor_scale3d(unreal.Vector(*scale))
            
            # Set label if provided
            if label:
                actor.set_actor_label(label)
            
            # Track spawned actor
            self.spawned_actors.append(actor)
            
            return {
                "success": True,
                "actor": actor,
                "message": f"Spawned {label or 'StaticMesh'} at {location}"
            }
            
        except Exception as e:
            return {
                "success": False,
                "message": f"Error spawning static mesh: {str(e)}"
            }
    
    def spawn_primitive(
        self,
        primitive_type: str,
        location: Tuple[float, float, float] = (0, 0, 0),
        rotation: Tuple[float, float, float] = (0, 0, 0),
        scale: Tuple[float, float, float] = (1, 1, 1),
        label: Optional[str] = None
    ) -> Dict[str, Any]:
        """
        Spawn a primitive shape (cube, sphere, cylinder, etc).
        
        Args:
            primitive_type: 'cube', 'sphere', 'cylinder', 'cone', 'plane'
            location: (x, y, z) world coordinates
            rotation: (pitch, yaw, roll) in degrees
            scale: (x, y, z) scale factors
            label: Optional actor label
        
        Returns:
            Dict with 'success', 'actor', 'message'
        """
        primitive_paths = {
            "cube": "/Engine/BasicShapes/Cube",
            "sphere": "/Engine/BasicShapes/Sphere",
            "cylinder": "/Engine/BasicShapes/Cylinder",
            "cone": "/Engine/BasicShapes/Cone",
            "plane": "/Engine/BasicShapes/Plane"
        }
        
        prim_type = primitive_type.lower()
        if prim_type not in primitive_paths:
            return {
                "success": False,
                "message": f"Unknown primitive: {primitive_type}. "
                f"Use: {', '.join(primitive_paths.keys())}"
            }
        
        asset_path = primitive_paths[prim_type]
        final_label = label or f"{primitive_type.capitalize()}_001"
        
        return self.spawn_static_mesh(
            asset_path, location, rotation, scale, final_label
        )
    
    def spawn_skeletal_mesh(
        self,
        asset_path: str,
        location: Tuple[float, float, float] = (0, 0, 0),
        rotation: Tuple[float, float, float] = (0, 0, 0),
        scale: Tuple[float, float, float] = (1, 1, 1),
        label: Optional[str] = None
    ) -> Dict[str, Any]:
        """Spawn a skeletal mesh actor."""
        try:
            # Load skeletal mesh
            skel_mesh = unreal.EditorAssetLibrary.load_asset(asset_path)
            
            if not isinstance(skel_mesh, unreal.SkeletalMesh):
                return {
                    "success": False,
                    "message": f"Not a SkeletalMesh: {asset_path}"
                }
            
            # Spawn actor
            spawn_loc = unreal.Vector(*location)
            spawn_rot = unreal.Rotator(*rotation)
            
            actor = self.editor_actor_subsystem.spawn_actor_from_class(
                unreal.SkeletalMeshActor.static_class(),
                spawn_loc,
                spawn_rot
            )
            
            # Set skeletal mesh
            actor.skeletal_mesh_component.set_skeletal_mesh(skel_mesh)
            
            # Set scale
            actor.set_actor_scale3d(unreal.Vector(*scale))
            
            # Set label
            if label:
                actor.set_actor_label(label)
            
            self.spawned_actors.append(actor)
            
            return {
                "success": True,
                "actor": actor,
                "message": f"Spawned skeletal mesh at {location}"
            }
            
        except Exception as e:
            return {
                "success": False,
                "message": f"Error: {str(e)}"
            }
    
    def spawn_blueprint(
        self,
        blueprint_path: str,
        location: Tuple[float, float, float] = (0, 0, 0),
        rotation: Tuple[float, float, float] = (0, 0, 0),
        label: Optional[str] = None
    ) -> Dict[str, Any]:
        """Spawn an actor from a Blueprint class."""
        try:
            # Load blueprint class
            bp_class = unreal.EditorAssetLibrary.load_blueprint_class(
                blueprint_path
            )
            
            if not bp_class:
                return {
                    "success": False,
                    "message": f"Failed to load Blueprint: {blueprint_path}"
                }
            
            # Spawn
            spawn_loc = unreal.Vector(*location)
            spawn_rot = unreal.Rotator(*rotation)
            
            actor = self.editor_actor_subsystem.spawn_actor_from_class(
                bp_class,
                spawn_loc,
                spawn_rot
            )
            
            if label:
                actor.set_actor_label(label)
            
            self.spawned_actors.append(actor)
            
            return {
                "success": True,
                "actor": actor,
                "message": f"Spawned Blueprint actor at {location}"
            }
            
        except Exception as e:
            return {
                "success": False,
                "message": f"Error: {str(e)}"
            }
    
    def spawn_light(
        self,
        light_type: str,
        location: Tuple[float, float, float] = (0, 0, 0),
        rotation: Tuple[float, float, float] = (0, 0, 0),
        intensity: float = 1000.0,
        color: Tuple[float, float, float] = (1.0, 1.0, 1.0),
        label: Optional[str] = None
    ) -> Dict[str, Any]:
        """
        Spawn a light actor.
        
        Args:
            light_type: 'point', 'spot', 'directional', 'rect'
            location: (x, y, z)
            rotation: (pitch, yaw, roll)
            intensity: Light intensity
            color: (r, g, b) 0.0-1.0
            label: Actor label
        """
        light_classes = {
            "point": unreal.PointLight,
            "spot": unreal.SpotLight,
            "directional": unreal.DirectionalLight,
            "rect": unreal.RectLight
        }
        
        light_type = light_type.lower()
        if light_type not in light_classes:
            return {
                "success": False,
                "message": f"Unknown light type: {light_type}"
            }
        
        try:
            spawn_loc = unreal.Vector(*location)
            spawn_rot = unreal.Rotator(*rotation)
            
            actor = self.editor_actor_subsystem.spawn_actor_from_class(
                light_classes[light_type].static_class(),
                spawn_loc,
                spawn_rot
            )
            
            # Set intensity and color
            light_comp = actor.get_component_by_class(
                unreal.LightComponent
            )
            if light_comp:
                light_comp.set_intensity(intensity)
                light_comp.set_light_color(
                    unreal.LinearColor(*color, 1.0)
                )
            
            if label:
                actor.set_actor_label(label)
            
            self.spawned_actors.append(actor)
            
            return {
                "success": True,
                "actor": actor,
                "message": f"Spawned {light_type} light at {location}"
            }
            
        except Exception as e:
            return {
                "success": False,
                "message": f"Error: {str(e)}"
            }
    
    def build_structure(
        self, plan: List[Dict[str, Any]]
    ) -> Dict[str, Any]:
        """
        Execute a multi-step building plan.
        
        Args:
            plan: List of spawn actions
                [
                    {
                        "action": "spawn_static_mesh",
                        "asset_path": "/Game/...",
                        "location": [0, 0, 0],
                        ...
                    },
                    ...
                ]
        
        Returns:
            Dict with results
        """
        results = []
        errors = []
        
        for step in plan:
            action = step.get("action")
            
            if action == "spawn_static_mesh":
                asset_path = step.get("asset_path")
                if not asset_path:
                    result = {"success": False, "message": "Missing asset_path"}
                else:
                    result = self.spawn_static_mesh(
                        asset_path=asset_path,
                        location=step.get("location", (0, 0, 0)),
                        rotation=step.get("rotation", (0, 0, 0)),
                        scale=step.get("scale", (1, 1, 1)),
                        label=step.get("label")
                    )
            
            elif action == "spawn_primitive":
                primitive_type = step.get("primitive_type")
                if not primitive_type:
                    result = {"success": False, "message": "Missing primitive_type"}
                else:
                    result = self.spawn_primitive(
                        primitive_type=primitive_type,
                        location=step.get("location", (0, 0, 0)),
                        rotation=step.get("rotation", (0, 0, 0)),
                        scale=step.get("scale", (1, 1, 1)),
                        label=step.get("label")
                    )
            
            elif action == "spawn_skeletal_mesh":
                asset_path = step.get("asset_path")
                if not asset_path:
                    result = {"success": False, "message": "Missing asset_path"}
                else:
                    result = self.spawn_skeletal_mesh(
                        asset_path=asset_path,
                        location=step.get("location", (0, 0, 0)),
                        rotation=step.get("rotation", (0, 0, 0)),
                        scale=step.get("scale", (1, 1, 1)),
                        label=step.get("label")
                    )
            
            elif action == "spawn_blueprint":
                blueprint_path = step.get("blueprint_path")
                if not blueprint_path:
                    result = {"success": False, "message": "Missing blueprint_path"}
                else:
                    result = self.spawn_blueprint(
                        blueprint_path=blueprint_path,
                        location=step.get("location", (0, 0, 0)),
                        rotation=step.get("rotation", (0, 0, 0)),
                        label=step.get("label")
                    )
            
            elif action == "spawn_light":
                light_type = step.get("light_type")
                if not light_type:
                    result = {"success": False, "message": "Missing light_type"}
                else:
                    result = self.spawn_light(
                        light_type=light_type,
                        location=step.get("location", (0, 0, 0)),
                        rotation=step.get("rotation", (0, 0, 0)),
                        intensity=step.get("intensity", 1000.0),
                        color=step.get("color", (1.0, 1.0, 1.0)),
                        label=step.get("label")
                    )
            
            else:
                result = {
                    "success": False,
                    "message": f"Unknown action: {action}"
                }
            
            results.append(result)
            
            if not result.get("success"):
                errors.append(result.get("message"))
        
        return {
            "success": len(errors) == 0,
            "total_actions": len(plan),
            "successful": len([r for r in results if r.get("success")]),
            "failed": len(errors),
            "errors": errors,
            "spawned_count": len(self.spawned_actors)
        }
    
    def clear_spawned_actors(self) -> Dict[str, Any]:
        """Delete all actors spawned in this session."""
        count = 0
        
        for actor in self.spawned_actors:
            try:
                self.editor_actor_subsystem.destroy_actor(actor)
                count += 1
            except Exception:
                pass
        
        self.spawned_actors.clear()
        
        return {
            "success": True,
            "message": f"Deleted {count} spawned actors"
        }


# Singleton instance
_orchestrator: Optional[SceneOrchestrator] = None


def get_orchestrator() -> SceneOrchestrator:
    """Get the global scene orchestrator instance."""
    global _orchestrator
    if _orchestrator is None:
        _orchestrator = SceneOrchestrator()
    return _orchestrator
