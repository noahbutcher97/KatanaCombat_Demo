"""Compatibility entry point for the pinned suite's independent Unreal host."""
import subprocess
import sys
from animation_analysis_dependency import checkout

if __name__ == "__main__":
    raise SystemExit(subprocess.call([sys.executable, str(checkout() / "Python/verify_unreal_host.py"), *sys.argv[1:]]))
