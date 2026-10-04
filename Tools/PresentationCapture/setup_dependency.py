"""Install the pinned PresentationCapture recorder and build its workers; never follow a branch tip."""
import argparse
import json
import subprocess

from presentation_capture_dependency import install


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repository", help="Override the lock's repository with a checkout containing the pinned commit")
    args = parser.parse_args()
    try:
        print(json.dumps(install(args.repository), indent=2))
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"Dependency setup failed: {error}\n")
