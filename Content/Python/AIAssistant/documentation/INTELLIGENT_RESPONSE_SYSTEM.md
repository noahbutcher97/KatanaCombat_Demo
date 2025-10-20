# Intelligent Response System v3.0

## Overview

The AI Assistant now features a **multi-layer intelligence system** that produces **distinctively different responses** based on the configured style. The system intelligently filters data BEFORE sending to AI, ensuring responses are contextually appropriate rather than just differently worded.

##  Key Improvements

### 1. **Smart Data Filtering**
Data is pre-processed based on response style BEFORE sending to OpenAI:

- **Concise**: Only camera + selected objects (minimal data)
- **Natural**: Selected objects + 5 representative actors (highlights only)
- **Balanced**: Selected + 10 actors + lighting summary
- **Descriptive**: Selected + 15 actors + full lighting/environment  
- **Technical**: All data + metadata (actor counts, lighting counts)
- **Detailed**: Complete exhaustive data including all actors
- **Creative**: Selected + 10 actors + environment (standard data with creative framing)

###  2. **Style-Specific Token Limits**
Each style has optimized max_tokens to enforce appropriate response length:

| Style | Max Tokens | Character Range |
|-------|------------|-----------------|
| Concise | 150 | ~100-400 chars |
| Natural | 300 | ~200-800 chars |
| Balanced | 350 | ~250-900 chars |
| Descriptive | 400 | ~300-1000 chars |
| Creative | 450 | ~350-1200 chars |
| Technical | 500 | ~400-1200 chars |
| Detailed | 800 | ~600-2000 chars |

### 3. **Focus Instructions**
AI receives explicit instructions on what to prioritize:

- **Concise**: "Focus ONLY on camera position, selected objects, total actor count. One short paragraph."
- **Natural**: "Describe what's interesting or notable. Skip mundane details. Conversational tone."
- **Balanced**: "Summarize key elements. Skip repetitive details."
- **Creative**: "Paint a vivid picture using creative language and imagery. Use metaphors and sensory details."
- **Technical**: "Provide precise technical specifications with exact coordinates and classifications."
- **Detailed**: "Analyze all elements comprehensively. Include spatial relationships and complete inventories."

### 4. **Project Metadata Collection** 
Optional rich project context for more intelligent AI responses:

```python
{
    "project_name": "MyUE5Game",
    "project_path": "/path/to/project",
    "asset_summary": {
        "total_assets": 1250,
        "asset_types": {"StaticMesh": 450, "Material": 320, ...},
        "top_asset_types": [["StaticMesh", 450], ["Material", 320], ...]
    },
    "source_code_stats": {
        "has_source": true,
        "cpp_file_count": 45,
        "header_file_count": 52
    },
    "content_folder_stats": {
        "uasset_count": 1180,
        "level_count": 8,
        "total_mb": 342.5
    }
}
```

## Configuration

### Backend (`config.json`)
```json
{
    "response_style": "technical",
    "temperature": 0.7,
    "model": "gpt-4o-mini"
}
```

### UE5 Side (`Saved/AIConsole/config.json`)
```json
{
    "response_style": "technical",
    "collect_project_metadata": true
}
```

## How It Works

### Data Flow

```
1. UE5 collects viewport data
   ↓
2. UE5 optionally adds project metadata
   ↓
3. Backend receives full context
   ↓
4. Backend filters data based on response style
   ↓
5. Backend sends filtered data + style instructions to OpenAI
   ↓
6. OpenAI generates response constrained by max_tokens
   ↓
7. Response returned to UE5
```

### Example: Same Scene, Different Styles

**Scene**: 50 actors, 3 selected, camera at (100, 200, 300)

**Concise Output** (~150 chars):
> Camera at (100, 200, 300). 3 selected: BP_PlayerStart, Cube_42, SM_Rock_01. Total: 50 actors in scene.

**Natural Output** (~300 chars):
> You're positioned at (100, 200, 300) looking at three selected objects: a player start, a cube, and a rock mesh. The scene has about 50 actors total with some interesting static meshes and a few lights.

**Technical Output** (~500 chars):
> Camera spatial configuration: location [100.0, 200.0, 300.0], rotation [pitch: 0.0, yaw: 90.0, roll: 0.0]. Selection state: 3 actors selected - BP_PlayerStart (class: PlayerStart), Cube_42 (class: StaticMeshActor), SM_Rock_01 (class: StaticMeshActor). Scene inventory: 50 total actors comprising 32 StaticMeshActors, 8 DirectionalLights, 5 PointLights, 3 PostProcessVolumes, 2 Landscapes. Lighting configuration: 1 directional (intensity: 10.0), 5 point lights (avg intensity: 5.2), 0 spot lights.

**Detailed Output** (~800 chars):
> The viewport displays a comprehensive scene composition with the camera positioned at coordinates [100.0, 200.0, 300.0] with rotation angles [pitch: 0.0, yaw: 90.0, roll: 0.0], providing a westward viewing direction. The scene contains a total of 50 actors distributed across multiple categories. Currently selected are three actors: BP_PlayerStart (PlayerStart class) located at [50, 50, 0], Cube_42 (StaticMeshActor) at [120, 180, 45], and SM_Rock_01 (StaticMeshActor) at [200, 250, 10]. The complete actor inventory includes 32 StaticMeshActors (meshes: SM_Rock_01, SM_Tree_02, SM_Wall_03...), 8 DirectionalLights providing primary illumination, 5 PointLights for accent lighting, 3 PostProcessVolumes for atmospheric effects, and 2 Landscape actors forming the terrain foundation. Environmental configuration includes exponential height fog with density 0.02 and multiple post-process volumes controlling color grading and exposure settings.

## Response Style Presets

### Descriptive (Default)
- **Use case**: General purpose, balanced detail
- **Data filter**: Selected + 15 actors + lighting/environment
- **Tone**: Clear, factual technical prose

### Technical/Precise
- **Use case**: Documentation, specifications, bug reports
- **Data filter**: All data + technical metadata
- **Tone**: Precise coordinates, exact values, structured format

### Natural/Conversational
- **Use case**: Casual queries, quick checks
- **Data filter**: Only notable/selected elements
- **Tone**: Friendly, conversational, skips boring details

### Balanced
- **Use case**: Code reviews, team communication
- **Data filter**: Key elements + summaries
- **Tone**: Technical accuracy with readability

### Concise/Brief
- **Use case**: Quick status checks, terminal output
- **Data filter**: Minimal (camera + selected only)
- **Tone**: Extremely brief, one short paragraph

### Detailed/Verbose
- **Use case**: Comprehensive analysis, training, documentation
- **Data filter**: Complete exhaustive data
- **Tone**: Thorough analysis with spatial relationships

### Creative/Imaginative
- **Use case**: Visualization, presentations, creative exploration, scene storytelling
- **Data filter**: Standard (selected + 10 actors + environment)
- **Tone**: Vivid imagery, metaphors, sensory details, flowing narrative paragraphs
- **Temperature**: 0.9 (higher creativity)

## Enabling Project Metadata

### In UE5 Config
```python
from AIAssistant.config import get_config

config = get_config()
config.set("collect_project_metadata", True)
```

### What Gets Collected
- Project name and path
- Total asset count and categorization
- C++ source code statistics
- Content folder size and file counts
- Top 10 most common asset types

### Performance Impact
- ~500ms first call (caches asset registry)
- ~50ms subsequent calls
- Adds ~2KB to request payload
- Provides significantly richer AI context

## Switching Styles

### Via Dashboard
1. Navigate to `/dashboard`
2. Click **Settings** tab
3. Select desired response style from dropdown
4. Click **Save Settings**
5. New style applies immediately to all subsequent requests

### Via UE5 Code
```python
from AIAssistant.config import get_config

config = get_config()
config.set("response_style", "technical")  # or any other style
```

### Via API
```bash
curl -X POST http://localhost:5000/api/config \
  -H "Content-Type: application/json" \
  -d '{"response_style": "concise"}'
```

## Troubleshooting

### "All styles produce similar output"
- ✅ Check dashboard Settings tab shows correct style
- ✅ Restart UE5 Python console to reload config
- ✅ Verify backend logs show correct filter mode
- ✅ Try dramatically different styles (concise vs detailed)

### "Response too long/short"
- Each style has fixed max_tokens
- Concise is intentionally brief (~150 chars)
- Detailed can reach ~2000 chars
- Use appropriate style for use case

### "Missing project context"
- Verify `collect_project_metadata: true` in config
- Check UE5 console for metadata collection logs
- First call may be slow (~500ms) while indexing
- Subsequent calls use cached data

## Migration Guide

### From v2.0 to v3.0

**No breaking changes!** The system is fully backward compatible.

**New features available:**
1. Intelligent data filtering (automatic)
2. Style-specific token limits (automatic)
3. Project metadata collection (opt-in)

**To upgrade:**
1. Copy new backend `main.py` to Replit
2. Copy updated UE5 scripts to project
3. Optionally enable metadata: `config.set("collect_project_metadata", True)`
4. Restart both backend and UE5 console

## Architecture

### Backend Components
- `filter_viewport_data()`: Pre-filters data based on style
- `RESPONSE_STYLES`: Enhanced with filter modes and token limits
- `ViewportContext`: Updated with `project_metadata` field
- `/describe_viewport`: Completely refactored to use intelligent system

### UE5 Components
- `ContextCollector.collect_project_metadata()`: New method for project analysis
- `ContextCollector.collect_viewport_data(include_project_metadata)`: Enhanced parameter
- `ActionExecutor._describe_viewport()`: Uses config to enable/disable metadata
- `Config.RESPONSE_STYLES`: Synced with backend definitions

## Performance Characteristics

| Operation | Time | Notes |
|-----------|------|-------|
| Viewport collection | 50-100ms | Standard |
| + Project metadata (first) | +500ms | Asset registry indexing |
| + Project metadata (cached) | +50ms | Subsequent calls |
| Backend filtering | <5ms | Local processing |
| OpenAI API call | 1-3s | Network + generation |
| **Total (no metadata)** | **1.1-3.1s** | Typical |
| **Total (with metadata)** | **1.6-3.6s** | First call |

## Best Practices

1. **Match style to use case**
   - Quick checks → Concise
   - Documentation → Technical or Detailed
   - Team communication → Natural or Balanced

2. **Enable metadata selectively**
   - First viewport query → Include metadata (establishes context)
   - Subsequent queries → Skip metadata (faster)
   - Scene changes → Include metadata again

3. **Temperature tuning**
   - Technical/specs → Lower temperature (0.3-0.5)
   - Creative/exploration → Higher temperature (0.7-0.9)
   - Balanced → Default (0.7)

4. **Monitor response quality**
   - Too generic → Enable project metadata
   - Too verbose → Switch to concise/balanced
   - Missing details → Switch to technical/detailed

## Future Enhancements

Planned for v4.0:
- [ ] Source code analysis integration
- [ ] Blueprint graph parsing
- [ ] Material network analysis
- [ ] Animation system context
- [ ] Plugin/module detection
- [ ] Version control status (Git)
- [ ] Custom style presets
- [ ] Per-request style override
