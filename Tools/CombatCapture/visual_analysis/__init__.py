"""Compatibility imports for existing project commands; implementation lives in AnimationAnalysis."""
from pathlib import Path
import sys

_integration = Path(__file__).resolve().parents[2] / "AnimationAnalysis"
sys.path.insert(0, str(_integration))
from animation_analysis_dependency import package_source
_source = package_source()
sys.path.insert(0, str(_source))
import animation_analysis as _implementation
if not Path(_implementation.__file__).resolve().is_relative_to(_source.resolve()):
    raise ImportError("A different animation_analysis distribution is already loaded")

from animation_analysis.geometry import segment_gap
from animation_analysis.adapters.unreal_capture import linked_actor, project
from animation_analysis.adapters.legacy_evidence import load_evidence, render_review, validate_review

__all__ = ["linked_actor", "project", "segment_gap", "load_evidence", "render_review", "validate_review"]
