# PT-11: Preview Tool "God Widget" Refactor

> **Status**: Phase 1 COMPLETE | Phase 2 COMPLETE
> **Branch**: `feature/pt-11-preview-tool-refactor`
> **Extracted From**: gap-mitigation-plan.md (2026-02-03)
> **Current Size**: ~5,500 lines (.cpp) + ~500 lines (.h) | **Target**: ~2,800 lines (.cpp)

---

## Problem

The `SPairedAnimationPreview` widget duplicates optimization/analysis logic that already exists in `UPairedAnimationAnalysisSubsystem`. The widget is ~6,000 lines and needs decomposition.

## Discovery: Subsystem Duplication

| Widget Method (DUPLICATE) | Subsystem Method (EXISTS) |
|---------------------------|---------------------------|
| `EvaluateConfigurationAtFrame()` | `EvaluateConfigurationAtFrame()` |
| `EvaluateConfigurationHolistic()` | `EvaluateConfigurationHolistic()` |
| `FindOptimalDistance()` | `FindOptimalDistance()`, `FindOptimalDistanceFast()` |
| `FindOptimalRotation()` | `FindOptimalRotation()`, `FindOptimalRotationFast()` |
| `InferSpatialRelationship()` | `InferSpatialRelationship()` |

## Strategy: 2-Phase Incremental Refactor

### Phase 1: Model Extraction (COMPLETE)

Created `FPairedAnimationPreviewModel` struct. Wired widget to use subsystem for analysis. Removed ~1,400 lines of duplicate code.

**Commits**: d2908b9, 3b111cf, 3707cee, 0fa97df, c5d9346

Steps completed:
1. Create `FPairedAnimationPreviewModel` struct
2. Move state members from widget to model
3. Wire optimization to `UPairedAnimationAnalysisSubsystem`
4. Move cache rebuild logic to model
5. Remove duplicate internal methods

### Phase 2: View Extraction (COMPLETE)

Extracted viewport and timeline into reusable Slate widgets.

**Commits**: e494da2, 7295922

Steps completed:
1. Extract `FPairedPreviewViewportClient` + `SPairedPreviewViewport`
2. Extract `SPairedAnimTimelineView`

## File Size Results

| File | Before | After |
|------|--------|-------|
| PairedAnimationPreview.h | 508 | ~200 |
| PairedAnimationPreview.cpp | 6,177 | ~2,800 |
| PairedAnimationPreviewModel.h/cpp | N/A | ~1,000 |
| SPairedAnimPreviewViewport.h/cpp | N/A | ~430 |
| SPairedAnimTimelineView.h/cpp | N/A | ~260 |

## Critical Files

| File | Purpose |
|------|---------|
| `PairedAnimationPreview.h/.cpp` | Main widget (refactored) |
| `PairedAnimationPreviewModel.h/.cpp` | State management |
| `SPairedAnimPreviewViewport.h/.cpp` | 3D viewport widget |
| `SPairedAnimTimelineView.h/.cpp` | Timeline scrubber |
| `PairedAnimationAnalysisSubsystem.h/.cpp` | Existing subsystem (delegate target) |
| `PairedAnimationEditorTypes.h` | Shared types |

## Status

This refactor is complete on the `feature/pt-11-preview-tool-refactor` branch. Latest commit: 7295922. Ready for merge to main when desired.
