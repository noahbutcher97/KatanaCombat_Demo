"""Compatibility entry point for the portable analysis package."""
import visual_analysis  # Resolve the checkout's package without a global install.
from animation_analysis.jobs.visual_review import publish, main

if __name__ == "__main__":
    raise SystemExit(main())
