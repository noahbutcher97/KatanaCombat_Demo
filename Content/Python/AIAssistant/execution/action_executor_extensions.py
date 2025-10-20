"""
Action Executor Extensions - New orchestration action implementations
"""

def add_orchestration_actions(executor):
    """Add implementation methods for orchestration actions."""
    
    # Scene Building Actions
    def _spawn_actor():
        """Spawn an actor (primitive, mesh, blueprint)."""
        from ..tools.scene_orchestrator import get_orchestrator
        
        get_orchestrator()
        # This would be called with parameters from AI plan
        # For now, return instruction
        return (
            "spawn_actor requires parameters from AI plan. "
            "Use build_scene action for AI-generated spawning."
        )
    
    def _spawn_primitive():
        """Spawn a primitive shape."""
        from ..tools.scene_orchestrator import get_orchestrator
        
        orchestrator = get_orchestrator()
        result = orchestrator.spawn_primitive(
            "cube", location=(0, 0, 100)
        )
        
        if result["success"]:
            return f"✅ {result['message']}"
        return f"❌ {result['message']}"
    
    def _build_scene():
        """Execute AI-generated scene building plan."""
        
        # This will be called with a full action plan from AI
        return (
            "build_scene requires action plan from AI. "
            "AI will provide JSON plan for execution."
        )
    
    # Viewport Control Actions
    def _focus_camera():
        """Focus camera on selected actors."""
        from ..tools.viewport_controller import get_viewport_controller
        
        controller = get_viewport_controller()
        result = controller.focus_on_selected()
        
        if result["success"]:
            return f"🎥 {result['message']}"
        return f"❌ {result['message']}"
    
    def _move_camera():
        """Move camera to location."""
        
        return (
            "move_camera requires target location from AI. "
            "AI will provide coordinates."
        )
    
    def _orbit_camera():
        """Orbit camera around selection."""
        import unreal

        from ..tools.viewport_controller import get_viewport_controller
        
        actor_subsystem = unreal.get_editor_subsystem(
            unreal.EditorActorSubsystem
        )
        selected = actor_subsystem.get_selected_level_actors()
        
        if not selected:
            return "❌ No actors selected"
        
        controller = get_viewport_controller()
        result = controller.orbit_around_actor(selected[0], 45)
        
        if result["success"]:
            return f"🎥 {result['message']}"
        return f"❌ {result['message']}"
    
    def _top_down_view():
        """Set top-down camera view."""
        from ..tools.viewport_controller import get_viewport_controller
        
        controller = get_viewport_controller()
        result = controller.top_down_view()
        
        if result["success"]:
            return f"🎥 {result['message']}"
        return f"❌ {result['message']}"
    
    # Actor Manipulation Actions
    def _align_actors():
        """Align selected actors."""
        from ..tools.actor_manipulator import get_manipulator
        
        manipulator = get_manipulator()
        result = manipulator.align_actors(axis="z", align_to="min")
        
        if result["success"]:
            return f"📐 {result['message']}"
        return f"❌ {result['message']}"
    
    def _distribute_actors():
        """Distribute actors evenly."""
        from ..tools.actor_manipulator import get_manipulator
        
        manipulator = get_manipulator()
        result = manipulator.distribute_evenly(axis="x")
        
        if result["success"]:
            return f"📐 {result['message']}"
        return f"❌ {result['message']}"
    
    def _arrange_grid():
        """Arrange actors in grid."""
        from ..tools.actor_manipulator import get_manipulator
        
        manipulator = get_manipulator()
        result = manipulator.arrange_in_grid(rows=5, columns=5)
        
        if result["success"]:
            return f"📐 {result['message']}"
        return f"❌ {result['message']}"
    
    def _arrange_circle():
        """Arrange actors in circle."""
        from ..tools.actor_manipulator import get_manipulator
        
        manipulator = get_manipulator()
        result = manipulator.arrange_in_circle(radius=500)
        
        if result["success"]:
            return f"📐 {result['message']}"
        return f"❌ {result['message']}"
    
    def _snap_to_grid():
        """Snap actors to grid."""
        from ..tools.actor_manipulator import get_manipulator
        
        manipulator = get_manipulator()
        result = manipulator.align_to_grid(grid_size=50)
        
        if result["success"]:
            return f"📐 {result['message']}"
        return f"❌ {result['message']}"
    
    # Editor Utility Generation
    def _generate_editor_tool():
        """Generate editor utility widget from AI spec."""
        
        return (
            "generate_tool requires AI specification. "
            "AI will provide widget definition."
        )
    
    # Register all new methods
    executor._spawn_actor = _spawn_actor
    executor._spawn_primitive = _spawn_primitive
    executor._build_scene = _build_scene
    executor._focus_camera = _focus_camera
    executor._move_camera = _move_camera
    executor._orbit_camera = _orbit_camera
    executor._top_down_view = _top_down_view
    executor._align_actors = _align_actors
    executor._distribute_actors = _distribute_actors
    executor._arrange_grid = _arrange_grid
    executor._arrange_circle = _arrange_circle
    executor._snap_to_grid = _snap_to_grid
    executor._generate_editor_tool = _generate_editor_tool
