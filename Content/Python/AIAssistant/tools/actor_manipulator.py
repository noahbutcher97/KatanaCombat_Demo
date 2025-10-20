"""
Actor Manipulator - Alignment, Transform, Organization
UE 5.6 Compliant - Uses EditorActorSubsystem for actor operations
"""

import math
from typing import Any, Dict, List, Optional, Tuple

import unreal  # type: ignore


class ActorManipulator:
    """Handles actor alignment, transforms, and organization."""
    
    def __init__(self):
        self.actor_subsystem = unreal.get_editor_subsystem(
            unreal.EditorActorSubsystem
        )
    
    def align_to_grid(
        self,
        actors: Optional[List[unreal.Actor]] = None,
        grid_size: float = 10.0
    ) -> Dict[str, Any]:
        """Snap actors to grid."""
        if actors is None:
            actors = self.actor_subsystem.get_selected_level_actors()
        
        if not actors:
            return {"success": False, "message": "No actors"}
        
        def snap(value: float, size: float) -> float:
            return round(value / size) * size
        
        count = 0
        for actor in actors:
            loc = actor.get_actor_location()
            snapped = unreal.Vector(
                snap(loc.x, grid_size),
                snap(loc.y, grid_size),
                snap(loc.z, grid_size)
            )
            actor.set_actor_location(snapped, False, True)
            count += 1
        
        return {
            "success": True,
            "message": f"Aligned {count} actors to grid ({grid_size})"
        }
    
    def align_actors(
        self,
        actors: Optional[List[unreal.Actor]] = None,
        axis: str = "z",
        align_to: str = "min"
    ) -> Dict[str, Any]:
        """
        Align multiple actors along an axis.
        
        Args:
            actors: Actors to align (uses selected if None)
            axis: 'x', 'y', or 'z'
            align_to: 'min', 'max', 'center', or float value
        """
        if actors is None:
            actors = self.actor_subsystem.get_selected_level_actors()
        
        if len(actors) < 2:
            return {
                "success": False,
                "message": "Need at least 2 actors"
            }
        
        axis = axis.lower()
        if axis not in ['x', 'y', 'z']:
            return {"success": False, "message": "Invalid axis"}
        
        # Get positions
        positions = [a.get_actor_location() for a in actors]
        
        # Calculate target value
        if align_to == "min":
            values = [getattr(p, axis) for p in positions]
            target = min(values)
        elif align_to == "max":
            values = [getattr(p, axis) for p in positions]
            target = max(values)
        elif align_to == "center":
            values = [getattr(p, axis) for p in positions]
            target = sum(values) / len(values)
        else:
            try:
                target = float(align_to)
            except (ValueError, TypeError):
                return {"success": False, "message": "Invalid align_to"}
        
        # Apply alignment
        for actor in actors:
            loc = actor.get_actor_location()
            
            if axis == 'x':
                new_loc = unreal.Vector(target, loc.y, loc.z)
            elif axis == 'y':
                new_loc = unreal.Vector(loc.x, target, loc.z)
            else:
                new_loc = unreal.Vector(loc.x, loc.y, target)
            
            actor.set_actor_location(new_loc, False, True)
        
        return {
            "success": True,
            "message": f"Aligned {len(actors)} actors on {axis} to {target}"
        }
    
    def distribute_evenly(
        self,
        actors: Optional[List[unreal.Actor]] = None,
        axis: str = "x",
        spacing: Optional[float] = None
    ) -> Dict[str, Any]:
        """Distribute actors evenly along an axis."""
        if actors is None:
            actors = self.actor_subsystem.get_selected_level_actors()
        
        if len(actors) < 2:
            return {"success": False, "message": "Need at least 2 actors"}
        
        axis = axis.lower()
        if axis not in ['x', 'y', 'z']:
            return {"success": False, "message": "Invalid axis"}
        
        # Sort by position on axis
        actors_sorted = sorted(
            actors,
            key=lambda a: getattr(a.get_actor_location(), axis)
        )
        
        start_loc = actors_sorted[0].get_actor_location()
        
        if spacing is None:
            # Calculate spacing from first to last
            end_loc = actors_sorted[-1].get_actor_location()
            total_dist = abs(
                getattr(end_loc, axis) - getattr(start_loc, axis)
            )
            spacing = total_dist / (len(actors) - 1) if len(actors) > 1 else 100
        
        # Distribute
        for i, actor in enumerate(actors_sorted):
            loc = actor.get_actor_location()
            
            if axis == 'x':
                new_loc = unreal.Vector(
                    start_loc.x + i * spacing,
                    loc.y,
                    loc.z
                )
            elif axis == 'y':
                new_loc = unreal.Vector(
                    loc.x,
                    start_loc.y + i * spacing,
                    loc.z
                )
            else:
                new_loc = unreal.Vector(
                    loc.x,
                    loc.y,
                    start_loc.z + i * spacing
                )
            
            actor.set_actor_location(new_loc, False, True)
        
        return {
            "success": True,
            "message": f"Distributed {len(actors)} actors ({spacing} spacing)"
        }
    
    def rotate_actors(
        self,
        actors: Optional[List[unreal.Actor]] = None,
        rotation: Tuple[float, float, float] = (0, 0, 0),
        relative: bool = False
    ) -> Dict[str, Any]:
        """Rotate actors (absolute or relative)."""
        if actors is None:
            actors = self.actor_subsystem.get_selected_level_actors()
        
        if not actors:
            return {"success": False, "message": "No actors"}
        
        rot = unreal.Rotator(*rotation)
        
        for actor in actors:
            if relative:
                current = actor.get_actor_rotation()
                new_rot = unreal.Rotator(
                    current.pitch + rot.pitch,
                    current.yaw + rot.yaw,
                    current.roll + rot.roll
                )
                actor.set_actor_rotation(new_rot, True)
            else:
                actor.set_actor_rotation(rot, True)
        
        return {
            "success": True,
            "message": f"Rotated {len(actors)} actors"
        }
    
    def scale_actors(
        self,
        actors: Optional[List[unreal.Actor]] = None,
        scale: Tuple[float, float, float] = (1, 1, 1),
        relative: bool = False
    ) -> Dict[str, Any]:
        """Scale actors (absolute or relative)."""
        if actors is None:
            actors = self.actor_subsystem.get_selected_level_actors()
        
        if not actors:
            return {"success": False, "message": "No actors"}
        
        scale_vec = unreal.Vector(*scale)
        
        for actor in actors:
            if relative:
                current = actor.get_actor_scale3d()
                new_scale = unreal.Vector(
                    current.x * scale_vec.x,
                    current.y * scale_vec.y,
                    current.z * scale_vec.z
                )
                actor.set_actor_scale3d(new_scale)
            else:
                actor.set_actor_scale3d(scale_vec)
        
        return {
            "success": True,
            "message": f"Scaled {len(actors)} actors"
        }
    
    def arrange_in_circle(
        self,
        actors: Optional[List[unreal.Actor]] = None,
        radius: float = 500.0,
        center: Tuple[float, float, float] = (0, 0, 0)
    ) -> Dict[str, Any]:
        """Arrange actors in a circle."""
        if actors is None:
            actors = self.actor_subsystem.get_selected_level_actors()
        
        if not actors:
            return {"success": False, "message": "No actors"}
        
        center_vec = unreal.Vector(*center)
        count = len(actors)
        angle_step = 360.0 / count
        
        for i, actor in enumerate(actors):
            angle_rad = math.radians(i * angle_step)
            x = center_vec.x + radius * math.cos(angle_rad)
            y = center_vec.y + radius * math.sin(angle_rad)
            z = center_vec.z
            
            new_loc = unreal.Vector(x, y, z)
            actor.set_actor_location(new_loc, False, True)
            
            # Optionally rotate to face center
            direction = (center_vec - new_loc).normal()
            rotation = direction.rotation()
            actor.set_actor_rotation(rotation, True)
        
        return {
            "success": True,
            "message": f"Arranged {count} actors in circle (r={radius})"
        }
    
    def arrange_in_grid(
        self,
        actors: Optional[List[unreal.Actor]] = None,
        rows: int = 5,
        columns: int = 5,
        spacing: float = 200.0,
        start_location: Tuple[float, float, float] = (0, 0, 0)
    ) -> Dict[str, Any]:
        """Arrange actors in a grid pattern."""
        if actors is None:
            actors = self.actor_subsystem.get_selected_level_actors()
        
        if not actors:
            return {"success": False, "message": "No actors"}
        
        start = unreal.Vector(*start_location)
        
        for i, actor in enumerate(actors):
            row = i // columns
            col = i % columns
            
            x = start.x + col * spacing
            y = start.y + row * spacing
            z = start.z
            
            actor.set_actor_location(
                unreal.Vector(x, y, z), False, True
            )
        
        return {
            "success": True,
            "message": f"Arranged {len(actors)} in {rows}x{columns} grid"
        }
    
    def group_actors(
        self,
        actors: Optional[List[unreal.Actor]] = None,
        group_name: str = "ActorGroup"
    ) -> Dict[str, Any]:
        """Group actors together (creates folder in outliner)."""
        if actors is None:
            actors = self.actor_subsystem.get_selected_level_actors()
        
        if not actors:
            return {"success": False, "message": "No actors"}
        
        # Set folder path for all actors
        for actor in actors:
            actor.set_folder_path(group_name)
        
        return {
            "success": True,
            "message": f"Grouped {len(actors)} actors in '{group_name}'"
        }
    
    def move_to_location(
        self,
        actors: Optional[List[unreal.Actor]] = None,
        location: Tuple[float, float, float] = (0, 0, 0),
        keep_relative_positions: bool = True
    ) -> Dict[str, Any]:
        """Move actors to a location."""
        if actors is None:
            actors = self.actor_subsystem.get_selected_level_actors()
        
        if not actors:
            return {"success": False, "message": "No actors"}
        
        target = unreal.Vector(*location)
        
        if keep_relative_positions and len(actors) > 1:
            # Calculate center
            positions = [a.get_actor_location() for a in actors]
            center = unreal.Vector(
                sum(p.x for p in positions) / len(positions),
                sum(p.y for p in positions) / len(positions),
                sum(p.z for p in positions) / len(positions)
            )
            
            # Move with offset
            offset = target - center
            for actor in actors:
                loc = actor.get_actor_location()
                actor.set_actor_location(loc + offset, False, True)
        else:
            # Move all to same location
            for actor in actors:
                actor.set_actor_location(target, False, True)
        
        return {
            "success": True,
            "message": f"Moved {len(actors)} actors to {location}"
        }


# Singleton
_manipulator: Optional[ActorManipulator] = None


def get_manipulator() -> ActorManipulator:
    """Get the global actor manipulator instance."""
    global _manipulator
    if _manipulator is None:
        _manipulator = ActorManipulator()
    return _manipulator
