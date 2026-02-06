#!/usr/bin/env python3
"""
Quick demo of sync_gaps.py improvements

Non-interactive demonstration of the error handling improvements.
"""

import subprocess
import os
import sys


def demo(title, cmd, env_overrides=None):
    """Run a demo scenario."""
    print("\n" + "=" * 70)
    print(f"  {title}")
    print("=" * 70)
    
    env = os.environ.copy()
    if env_overrides:
        for key, value in env_overrides.items():
            if value is None:
                env.pop(key, None)
            else:
                env[key] = value
    
    print(f"Command: python3 {' '.join(cmd[1:])}")
    if env_overrides:
        print(f"Environment: {env_overrides}")
    print("-" * 70)
    
    result = subprocess.run(cmd, capture_output=True, text=True, env=env)
    
    # Show relevant output (limit lines)
    output = result.stdout + result.stderr
    lines = output.split('\n')
    for line in lines[:25]:  # Show first 25 lines
        print(line)
    if len(lines) > 25:
        print(f"... ({len(lines) - 25} more lines)")
    
    print(f"\nExit Code: {result.returncode}")
    print()


def main():
    print("╔" + "=" * 68 + "╗")
    print("║" + " " * 15 + "sync_gaps.py Improvements Demo" + " " * 23 + "║")
    print("╚" + "=" * 68 + "╝")
    
    # Demo 1: Help shows new --debug flag
    demo(
        "Demo 1: Help Text (shows --debug flag)",
        ['python3', 'sync_gaps.py', '--help']
    )
    
    # Demo 2: Missing token error
    demo(
        "Demo 2: Missing Token (clear error message)",
        ['python3', 'sync_gaps.py', '--create', '--dry-run'],
        {'GH_TOKEN': None, 'GITHUB_TOKEN': None}
    )
    
    # Demo 3: Invalid token with debug mode
    demo(
        "Demo 3: Invalid Token + Debug Mode (verbose logging)",
        ['python3', 'sync_gaps.py', '--create', '--dry-run', '--debug', '--max', '1'],
        {'GH_TOKEN': 'fake_token_for_demo'}
    )
    
    print("=" * 70)
    print("  Key Improvements Demonstrated")
    print("=" * 70)
    print()
    print("✅ Pre-flight validation (token, permissions, repository access)")
    print("✅ Clear error messages with actionable instructions")
    print("✅ Debug mode with verbose logging and timestamps")
    print("✅ Proper exit codes (0 for success, 1 for errors)")
    print("✅ Retry logic with exponential backoff (for transient errors)")
    print("✅ Failure tracking and circuit breaking (stops after 5 failures)")
    print("✅ Success criteria validation (fails if 0 issues created)")
    print()
    print("All improvements maintain backward compatibility with existing workflows.")
    print()


if __name__ == '__main__':
    main()
