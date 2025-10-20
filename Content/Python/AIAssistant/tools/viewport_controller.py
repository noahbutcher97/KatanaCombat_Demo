"""
Viewport Controller - Camera Movement, Focus, and Framing
UE 5.6 Compliant - Uses UnrealEditorSubsystem for viewport control
"""

import math
from typing import Any, Dict, List, Optional, Tuple

import unreal


class ViewportController:
    """Controls viewport camera for focusing, framing, and navigation."""
    
    def __init__(self):
        self.editor_subsystem = unreal.get_editor_subsystem(
            unreal.UnrealEditorSubsystem
        )
        self.actor_subsystem = unreal.get_editor_subsystem(
            unreal.EditorActorSubsystem
        )
    
    def get_camera_info(self) -> Dict[str, Any]:
        """Get current viewport camera position and rotation."""
        try:
            location, rotation = (
                self.editor_subsystem.get_level_viewport_camera_info()
            )
            
            return {
                "success": True,
                "location": {
                    "x": location.x,
                    "y": location.y,
                    "z": location.z
                },
                "rotation": {
                    "pitch": rotation.pitch,
                    "yaw": rotation.yaw,
                    "roll": rotation.roll
                }
            }
        except Exception as e:
            return {
                "success": False,
                "message": f"Error getting camera info: {str(e)}"
            }
    
    def set_camera_position(
        self,
        location: Tuple[float, float, float],
        rotation: Tuple[float, float, float]
    ) -> Dict[str, Any]:
        """Set viewport camera to specific position and rotation."""
        try:
            cam_loc = unreal.Vector(*location)
            cam_rot = unreal.Rotator(*rotation)
            
            self.editor_subsystem.set_level_viewport_camera_info(
                cam_loc, cam_rot
            )
            
            return {
                "success": True,
                "message": f"Camera moved to {location}"
            }
        except Exception as e:
            return {
                "success": False,
                "message": f"Error: {str(e)}"
            }
    
    def focus_on_actor(
        self,
        actor: unreal.Actor,
        distance_multiplier: float = 2.0
    ) -> Dict[str, Any]:
        """
        Focus viewport camera on an actor (like pressing F).
        
        Args:
            actor: Actor to focus on
            distance_multiplier: Distance from actor (2.0 = 2x bounds)
        """
        try:
            # Get actor bounds
            origin, extent = actor.get_actor_bounds(
                only_colliding_components=False
            )
            
            # Calculate camera distance
            max_extent = max(extent.x, extent.y, extent.z)
            distance = max_extent * distance_multiplier
            
            # Position camera behind and above actor
            cam_location = origin + unreal.Vector(
                -distance, 0, distance * 0.5
            )
            
            # Look at actor
            direction = (origin - cam_location).normal()
            cam_rotation = direction.rotation()
            
            # Apply
            self.editor_subsystem.set_level_viewport_camera_info(
                cam_location, cam_rotation
            )
            
            return {
                "success": True,
                "message": f"Focused on {actor.get_actor_label()}"
            }
        except Exception as e:
            return {
                "success": False,
                "message": f"Error: {str(e)}"
            }
    
    def focus_on_selected(
        self, distance_multiplier: float = 2.0
    ) -> Dict[str, Any]:
        """Focus on currently selected actors."""
        selected = self.actor_subsystem.get_selected_level_actors()
        
        if not selected:
            return {
                "success": False,
                "message": "No actors selected"
            }
        
        if len(selected) == 1:
            return self.focus_on_actor(selected[0], distance_multiplier)
        
        # Multiple actors - focus on center
        return self.focus_on_actors(selected, distance_multiplier)
    
    def focus_on_actors(
        self,
        actors: List[unreal.Actor],
        distance_multiplier: float = 2.0
    ) -> Dict[str, Any]:
        """Focus camera on multiple actors (center of all)."""
        if not actors:
            return {
                "success": False,
                "message": "No actors provided"
            }
        
        try:
            # Calculate bounding box of all actors
            min_point = unreal.Vector(float('inf'), float('inf'), 
                                      float('inf'))
            max_point = unreal.Vector(float('-inf'), float('-inf'), 
                                      float('-inf'))
            
            for actor in actors:
                origin, extent = actor.get_actor_bounds(
                    only_colliding_components=False
                )
                
                min_point.x = min(min_point.x, origin.x - extent.x)
                min_point.y = min(min_point.y, origin.y - extent.y)
                min_point.z = min(min_point.z, origin.z - extent.z)
                
                max_point.x = max(max_point.x, origin.x + extent.x)
                max_point.y = max(max_point.y, origin.y + extent.y)
                max_point.z = max(max_point.z, origin.z + extent.z)
            
            # Calculate center and size
            center = (min_point + max_point) * 0.5
            size = max_point - min_point
            max_size = max(size.x, size.y, size.z)
            
            # Position camera
            distance = max_size * distance_multiplier
            cam_location = center + unreal.Vector(
                -distance, 0, distance * 0.5
            )
            
            direction = (center - cam_location).normal()
            cam_rotation = direction.rotation()
            
            self.editor_subsystem.set_level_viewport_camera_info(
                cam_location, cam_rotation
            )
            
            return {
                "success": True,
                "message": f"Focused on {len(actors)} actors"
            }
        except Exception as e:
            return {
                "success": False,
                "message": f"Error: {str(e)}"
            }
    
    def frame_actor(
        self,
        actor: unreal.Actor,
        padding: float = 1.2
    ) -> Dict[str, Any]:
        """Frame actor perfectly in viewport."""
        return self.focus_on_actor(actor, padding)
    
    def move_camera_to_location(
        self,
        target_location: Tuple[float, float, float],
        look_at_target: bool = True
    ) -> Dict[str, Any]:
        """Move camera to a specific location."""
        try:
            cam_loc = unreal.Vector(*target_location)
            
            if look_at_target:
                # Get current camera to determine what to look at
                current_loc, _ = (
                    self.editor_subsystem.get_level_viewport_camera_info()
                )
                direction = (current_loc - cam_loc).normal()
                cam_rot = direction.rotation()
            else:
                cam_rot = unreal.Rotator(0, 0, 0)
            
            self.editor_subsystem.set_level_viewport_camera_info(
                cam_loc, cam_rot
            )
            
            return {
                "success": True,
                "message": f"Camera moved to {target_location}"
            }
        except Exception as e:
            return {
                "success": False,
                "message": f"Error: {str(e)}"
            }
    
    def orbit_around_actor(
        self,
        actor: unreal.Actor,
        angle_degrees: float = 45.0,
        distance: Optional[float] = None
    ) -> Dict[str, Any]:
        """Orbit camera around an actor by specified angle."""
        try:
            # Get actor location
            actor_loc = actor.get_actor_location()
            
            # Get current camera
            cam_loc, cam_rot = (
                self.editor_subsystem.get_level_viewport_camera_info()
            )
            
            # Calculate orbit
            if distance is None:
                distance = (cam_loc - actor_loc).length()
            
            # Convert to radians
            angle_rad = math.radians(angle_degrees)
            
            # Calculate new position
            offset = cam_loc - actor_loc
            x = offset.x * math.cos(angle_rad) - offset.y * math.sin(
                angle_rad
            )
            y = offset.x * math.sin(angle_rad) + offset.y * math.cos(
                angle_rad
            )
            
            new_cam_loc = actor_loc + unreal.Vector(x, y, offset.z)
            
            # Look at actor
            direction = (actor_loc - new_cam_loc).normal()
            new_cam_rot = direction.rotation()
            
            self.editor_subsystem.set_level_viewport_camera_info(
                new_cam_loc, new_cam_rot
            )
            
            return {
                "success": True,
                "message": f"Orbited {angle_degrees}° around actor"
            }
        except Exception as e:
            return {
                "success": False,
                "message": f"Error: {str(e)}"
            }
    
    def look_at_location(
        self, target: Tuple[float, float, float]
    ) -> Dict[str, Any]:
        """Rotate camera to look at a specific location."""
        try:
            target_loc = unreal.Vector(*target)
            cam_loc, _ = (
                self.editor_subsystem.get_level_viewport_camera_info()
            )
            
            direction = (target_loc - cam_loc).normal()
            cam_rot = direction.rotation()
            
            self.editor_subsystem.set_level_viewport_camera_info(
                cam_loc, cam_rot
            )
            
            return {
                "success": True,
                "message": f"Camera looking at {target}"
            }
        except Exception as e:
            return {
                "success": False,
                "message": f"Error: {str(e)}"
            }
    
    def top_down_view(
        self, center: Optional[Tuple[float, float, float]] = None,
        height: float = 1000.0
    ) -> Dict[str, Any]:
        """Set camera to top-down orthographic-style view."""
        try:
            if center is None:
                # Use world origin
                center = (0, 0, 0)
            
            cam_loc = unreal.Vector(center[0], center[1], height)
            cam_rot = unreal.Rotator(-90, 0, 0)  # Look straight down
            
            self.editor_subsystem.set_level_viewport_camera_info(
                cam_loc, cam_rot
            )
            
            return {
                "success": True,
                "message": f"Top-down view at height {height}"
            }
        except Exception as e:
            return {
                "success": False,
                "message": f"Error: {str(e)}"
            }
    
    def perspective_view(
        self, angle: str = "front"
    ) -> Dict[str, Any]:
        """
        Set camera to standard perspective view.
        
        Args:
            angle: 'front', 'back', 'left', 'right', 'top', 'bottom'
        """
        views = {
            "front": (
                unreal.Vector(1000, 0, 0),
                unreal.Rotator(0, 180, 0)
            ),
            "back": (
                unreal.Vector(-1000, 0, 0),
                unreal.Rotator(0, 0, 0)
            ),
            "left": (
                unreal.Vector(0, -1000, 0),
                unreal.Rotator(0, 90, 0)
            ),
            "right": (
                unreal.Vector(0, 1000, 0),
                unreal.Rotator(0, -90, 0)
            ),
            "top": (
                unreal.Vector(0, 0, 1000),
                unreal.Rotator(-90, 0, 0)
            ),
            "bottom": (
                unreal.Vector(0, 0, -1000),
                unreal.Rotator(90, 0, 0)
            )
        }
        
        angle = angle.lower()
        if angle not in views:
            return {
                "success": False,
                "message": f"Unknown view: {angle}"
            }
        
        try:
            cam_loc, cam_rot = views[angle]
            self.editor_subsystem.set_level_viewport_camera_info(
                cam_loc, cam_rot
            )
            
            return {
                "success": True,
                "message": f"Set to {angle} view"
            }
        except Exception as e:
            return {
                "success": False,
                "message": f"Error: {str(e)}"
            }


# Singleton
_viewport_controller: Optional[ViewportController] = None


def get_viewport_controller() -> ViewportController:
    """Get the global viewport controller instance."""
    global _viewport_controller
    if _viewport_controller is None:
        _viewport_controller = ViewportController()
    return _viewport_controller
