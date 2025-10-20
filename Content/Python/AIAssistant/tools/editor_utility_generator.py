"""
Editor Utility Widget Generator - AI-Powered UI Tool Creation
Generates editor utilities from natural language prompts
UE 5.6 Compliant
"""

import json
from typing import Any, Dict, List, Optional

import unreal


class EditorUtilityGenerator:
    """Generates Editor Utility Widgets from AI descriptions."""
    
    def __init__(self):
        self.editor_util_subsystem = unreal.get_editor_subsystem(
            unreal.EditorUtilitySubsystem
        )
        self.asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    
    def generate_utility_widget(
        self,
        widget_name: str,
        description: str,
        ui_elements: List[Dict[str, Any]],
        functions: List[Dict[str, Any]]
    ) -> Dict[str, Any]:
        """
        Generate an Editor Utility Widget from specification.
        
        Args:
            widget_name: Name for the widget
            description: What the widget does
            ui_elements: List of UI elements 
                (buttons, text boxes, sliders, etc.)
            functions: List of functions to implement
        
        Returns:
            Result dict with widget path
        """
        try:
            # Create widget blueprint
            widget_path = f"/Game/EditorUtilities/{widget_name}"
            
            # Generate Python script for the widget
            script = self._generate_widget_script(
                widget_name, description, ui_elements, functions
            )
            
            # Save script to project
            script_path = self._save_widget_script(widget_name, script)
            
            return {
                "success": True,
                "widget_name": widget_name,
                "script_path": script_path,
                "message": f"Created editor utility: {widget_name}",
                "script": script
            }
            
        except Exception as e:
            return {
                "success": False,
                "message": f"Error generating widget: {str(e)}"
            }
    
    def _generate_widget_script(
        self,
        name: str,
        description: str,
        ui_elements: List[Dict],
        functions: List[Dict]
    ) -> str:
        """Generate Python script for editor utility widget."""
        
        script_parts = []
        
        # Header
        script_parts.append(f'''"""
{name} - {description}
Auto-generated Editor Utility Widget
"""

import unreal

@unreal.uclass()
class {name}(unreal.EditorUtilityWidget):
    """
    {description}
    """
    
    def __init__(self):
        super().__init__()
''')
        
        # Add function implementations
        for func in functions:
            func_name = func.get("name", "execute")
            func_desc = func.get("description", "")
            func_code = func.get("code", "pass")
            
            script_parts.append(f'''
    @unreal.ufunction(ret=None, params=[])
    def {func_name}(self):
        """{func_desc}"""
        {func_code}
''')
        
        return "\n".join(script_parts)
    
    def _save_widget_script(
        self, widget_name: str, script: str
    ) -> str:
        """Save widget script to project Content/Python directory."""
        import os
        
        project_dir = unreal.Paths.project_content_dir()
        python_dir = os.path.join(
            project_dir, "..", "Python", "EditorUtilities"
        )
        
        # Create directory if needed
        if not os.path.exists(python_dir):
            os.makedirs(python_dir)
        
        script_path = os.path.join(python_dir, f"{widget_name}.py")
        
        with open(script_path, 'w') as f:
            f.write(script)
        
        return script_path
    
    def create_simple_button_tool(
        self,
        name: str,
        button_label: str,
        action_code: str,
        description: str = ""
    ) -> Dict[str, Any]:
        """
        Create a simple single-button editor utility.
        
        Args:
            name: Tool name
            button_label: Text on button
            action_code: Python code to execute when clicked
            description: What the tool does
        """
        ui_elements = [
            {
                "type": "button",
                "label": button_label,
                "function": "execute_action"
            }
        ]
        
        functions = [
            {
                "name": "execute_action",
                "description": description,
                "code": action_code
            }
        ]
        
        return self.generate_utility_widget(
            name, description, ui_elements, functions
        )
    
    def create_spawn_tool(
        self,
        name: str,
        asset_path: str,
        spawn_count: int = 1,
        pattern: str = "grid"
    ) -> Dict[str, Any]:
        """Create a tool that spawns actors in a pattern."""
        
        code = f'''
from AIAssistant.scene_orchestrator import get_orchestrator

orchestrator = get_orchestrator()

# Spawn actors in {pattern} pattern
for i in range({spawn_count}):
    if "{pattern}" == "grid":
        row = i // 10
        col = i % 10
        location = (col * 200, row * 200, 0)
    elif "{pattern}" == "circle":
        import math
        angle = (i / {spawn_count}) * 2 * math.pi
        radius = 500
        location = (
            math.cos(angle) * radius,
            math.sin(angle) * radius,
            0
        )
    else:
        location = (i * 200, 0, 0)
    
    orchestrator.spawn_static_mesh(
        "{asset_path}",
        location=location,
        label=f"Spawned_{{i}}"
    )

unreal.log(f"Spawned {spawn_count} actors")
'''
        
        return self.create_simple_button_tool(
            name=name,
            button_label=f"Spawn {spawn_count} {pattern}",
            action_code=code,
            description=f"Spawns {spawn_count} actors in {pattern} pattern"
        )
    
    def create_alignment_tool(
        self, name: str, axis: str = "z", align_to: str = "min"
    ) -> Dict[str, Any]:
        """Create a tool that aligns selected actors."""
        
        code = f'''
from AIAssistant.actor_manipulator import get_manipulator

manipulator = get_manipulator()
result = manipulator.align_actors(axis="{axis}", align_to="{align_to}")

if result.get("success"):
    unreal.log(result["message"])
else:
    unreal.log_error(result.get("message", "Failed"))
'''
        
        return self.create_simple_button_tool(
            name=name,
            button_label=f"Align {axis.upper()} to {align_to}",
            action_code=code,
            description=f"Aligns selected actors on {axis} axis to {align_to}"
        )
    
    def create_camera_focus_tool(
        self, name: str
    ) -> Dict[str, Any]:
        """Create a tool that focuses camera on selection."""
        
        code = '''
from AIAssistant.viewport_controller import get_viewport_controller

controller = get_viewport_controller()
result = controller.focus_on_selected()

if result.get("success"):
    unreal.log(result["message"])
else:
    unreal.log_error(result.get("message", "Failed"))
'''
        
        return self.create_simple_button_tool(
            name=name,
            button_label="Focus Camera on Selected",
            action_code=code,
            description="Focuses viewport camera on selected actors"
        )
    
    def create_batch_rename_tool(
        self, name: str, prefix: str, start_number: int = 1
    ) -> Dict[str, Any]:
        """Create a tool that batch renames actors."""
        
        code = f'''
import unreal

actor_subsystem = unreal.get_editor_subsystem(
    unreal.EditorActorSubsystem
)
selected = actor_subsystem.get_selected_level_actors()

for i, actor in enumerate(selected):
    new_name = f"{prefix}_{{i + {start_number}:03d}}"
    actor.set_actor_label(new_name)

unreal.log(f"Renamed {{len(selected)}} actors")
'''
        
        return self.create_simple_button_tool(
            name=name,
            button_label=f"Rename with prefix '{prefix}'",
            action_code=code,
            description=f"Batch renames selected actors with prefix {prefix}"
        )
    
    def parse_ai_widget_request(
        self, ai_response: str
    ) -> Dict[str, Any]:
        """
        Parse AI's widget specification from natural language.
        
        Expected format (JSON):
        {
            "widget_name": "MyTool",
            "description": "Does something cool",
            "ui_elements": [...],
            "functions": [...]
        }
        """
        try:
            # Try to extract JSON from AI response
            start = ai_response.find('{')
            end = ai_response.rfind('}') + 1
            
            if start == -1 or end == 0:
                return {
                    "success": False,
                    "message": "No JSON found in AI response"
                }
            
            spec = json.loads(ai_response[start:end])
            
            return self.generate_utility_widget(
                widget_name=spec.get("widget_name", "CustomTool"),
                description=spec.get("description", ""),
                ui_elements=spec.get("ui_elements", []),
                functions=spec.get("functions", [])
            )
            
        except json.JSONDecodeError as e:
            return {
                "success": False,
                "message": f"Failed to parse JSON: {str(e)}"
            }
    
    def list_generated_utilities(self) -> Dict[str, Any]:
        """List all generated editor utilities."""
        import os
        
        project_dir = unreal.Paths.project_content_dir()
        python_dir = os.path.join(
            project_dir, "..", "Python", "EditorUtilities"
        )
        
        if not os.path.exists(python_dir):
            return {
                "success": True,
                "utilities": [],
                "message": "No utilities created yet"
            }
        
        utilities = []
        for filename in os.listdir(python_dir):
            if filename.endswith('.py'):
                utilities.append(filename[:-3])  # Remove .py
        
        return {
            "success": True,
            "utilities": utilities,
            "count": len(utilities)
        }


# Singleton
_generator: Optional[EditorUtilityGenerator] = None


def get_utility_generator() -> EditorUtilityGenerator:
    """Get the global utility generator instance."""
    global _generator
    if _generator is None:
        _generator = EditorUtilityGenerator()
    return _generator
