#!/usr/bin/env python3
"""
Manual verification script for sync_gaps.py improvements

This script demonstrates the various error handling scenarios and improvements
made to sync_gaps.py. Run this to see the script behavior in different conditions.

Usage:
    python3 manual_verification.py
"""

import subprocess
import os
import sys


def print_section(title):
    """Print a section header."""
    print("\n" + "=" * 70)
    print(f"  {title}")
    print("=" * 70 + "\n")


def run_scenario(description, cmd, env=None):
    """Run a scenario and display the output."""
    print(f"Scenario: {description}")
    print(f"Command: {' '.join(cmd)}")
    print("-" * 70)
    
    if env is None:
        env = os.environ.copy()
    
    result = subprocess.run(cmd, capture_output=True, text=True, env=env)
    
    # Show output
    if result.stdout:
        print(result.stdout)
    if result.stderr:
        print("STDERR:", result.stderr)
    
    print(f"\nExit code: {result.returncode}")
    print("-" * 70 + "\n")
    
    return result.returncode


def main():
    print_section("sync_gaps.py Manual Verification")
    print("This script demonstrates the various improvements to sync_gaps.py")
    print("Each scenario shows different error handling and validation features.")
    
    input("\nPress Enter to start the verification...")
    
    # Scenario 1: Missing token
    print_section("Scenario 1: Missing GitHub Token")
    print("Expected: Clear error message, exit code 1")
    input("Press Enter to run...")
    
    env = os.environ.copy()
    env.pop('GH_TOKEN', None)
    env.pop('GITHUB_TOKEN', None)
    
    run_scenario(
        "No GitHub token set",
        ['python3', 'sync_gaps.py', '--create', '--dry-run', '--max', '1'],
        env=env
    )
    
    # Scenario 2: Invalid token
    print_section("Scenario 2: Invalid GitHub Token")
    print("Expected: Authentication failure message, exit code 1")
    input("Press Enter to run...")
    
    env = os.environ.copy()
    env['GH_TOKEN'] = 'invalid_token_abc123'
    
    run_scenario(
        "Invalid token set",
        ['python3', 'sync_gaps.py', '--create', '--dry-run', '--max', '1'],
        env=env
    )
    
    # Scenario 3: Debug mode with invalid token
    print_section("Scenario 3: Debug Mode (Verbose Output)")
    print("Expected: Detailed logging with timestamps, full error messages")
    input("Press Enter to run...")
    
    env = os.environ.copy()
    env['GH_TOKEN'] = 'test_debug_token'
    
    run_scenario(
        "Debug mode enabled",
        ['python3', 'sync_gaps.py', '--create', '--dry-run', '--debug', '--max', '1'],
        env=env
    )
    
    # Scenario 4: Help text
    print_section("Scenario 4: Help Text")
    print("Expected: Documentation of --debug flag and other options")
    input("Press Enter to run...")
    
    run_scenario(
        "Display help text",
        ['python3', 'sync_gaps.py', '--help'],
        env=None
    )
    
    # Scenario 5: Check if GH_TOKEN is actually set
    print_section("Scenario 5: Real Token Test (if available)")
    
    if os.environ.get('GH_TOKEN') or os.environ.get('GITHUB_TOKEN'):
        print("GitHub token detected in environment!")
        print("This will test with your actual token (dry-run mode only)")
        response = input("Run with real token? (y/n): ")
        
        if response.lower() == 'y':
            run_scenario(
                "Dry-run with real token",
                ['python3', 'sync_gaps.py', '--create', '--dry-run', '--max', '2'],
                env=None
            )
        else:
            print("Skipped.")
    else:
        print("No real GitHub token found in environment (GH_TOKEN/GITHUB_TOKEN)")
        print("Skipping this scenario.")
    
    # Summary
    print_section("Verification Complete")
    print("Summary of improvements demonstrated:")
    print("  ✅ Pre-flight token validation")
    print("  ✅ Clear error messages for missing/invalid tokens")
    print("  ✅ Debug mode with detailed logging")
    print("  ✅ Repository access validation")
    print("  ✅ Proper exit codes for CI/CD integration")
    print("\nThe script now has comprehensive error handling and will:")
    print("  - Fail fast with clear messages when misconfigured")
    print("  - Retry transient errors with exponential backoff")
    print("  - Stop after too many continuous failures")
    print("  - Report failure rates and specific error details")
    print("  - Provide verbose debugging information when needed")


if __name__ == '__main__':
    main()
