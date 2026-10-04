"""Install the pinned shared suite from an explicit repository; never follow its branch tip."""
import argparse
import json
import subprocess

from animation_analysis_dependency import PROJECT, SPEC
import pinned_plugin


def install(repository=None, project=PROJECT):
    return pinned_plugin.install(SPEC, repository, project)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repository", help="Override the lock's repository with a checkout or clone URL containing the pinned commit")
    args = parser.parse_args()
    try:
        print(json.dumps(install(args.repository), indent=2))
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"Dependency setup failed: {error}\n")
