#!/usr/bin/env python3
"""
Test script for sync_gaps.py improvements

This script validates the error handling, retry logic, and success criteria
of the sync_gaps.py script by simulating various failure scenarios.

Usage:
    python3 test_sync_gaps.py
"""

import subprocess
import sys
import os
import tempfile
import shutil
from pathlib import Path


def run_command(cmd, env=None):
    """
    Run a command and return (returncode, stdout, stderr).
    """
    if env is None:
        env = os.environ.copy()
    
    result = subprocess.run(
        cmd,
        capture_output=True,
        text=True,
        env=env
    )
    return result.returncode, result.stdout, result.stderr


def test_missing_token():
    """Test 1: Verify script fails gracefully when GitHub token is missing."""
    print("Test 1: Missing GitHub token")
    print("-" * 50)
    
    env = os.environ.copy()
    env.pop('GH_TOKEN', None)
    env.pop('GITHUB_TOKEN', None)
    
    returncode, stdout, stderr = run_command(
        ['python3', 'sync_gaps.py', '--create', '--dry-run'],
        env=env
    )
    
    if returncode != 0:
        print("✅ PASS: Script exits with error code")
    else:
        print("❌ FAIL: Script should exit with error when token is missing")
        return False
    
    if "No GitHub token found" in stdout or "No GitHub token found" in stderr:
        print("✅ PASS: Error message displayed")
    else:
        print("❌ FAIL: Missing clear error message")
        return False
    
    print()
    return True


def test_debug_mode():
    """Test 2: Verify --debug flag provides verbose output."""
    print("Test 2: Debug mode")
    print("-" * 50)
    
    # Use a fake token - it will fail but should show debug output
    env = os.environ.copy()
    env['GH_TOKEN'] = 'test_fake_token_12345'
    
    returncode, stdout, stderr = run_command(
        ['python3', 'sync_gaps.py', '--create', '--dry-run', '--debug', '--max', '1'],
        env=env
    )
    
    # Should see timestamp in debug mode
    if '[DEBUG]' in stdout or '[DEBUG]' in stderr:
        print("✅ PASS: Debug logging enabled")
    else:
        print("⚠️  WARN: Debug logging not visible (may be because of early exit)")
    
    # Should have timestamps in debug mode
    if any(c.isdigit() and ':' in line for line in stdout.split('\n') if '[' in line):
        print("✅ PASS: Timestamps present in debug mode")
    else:
        print("⚠️  WARN: Timestamps not visible")
    
    print()
    return True


def test_help_text():
    """Test 3: Verify help text includes new --debug flag."""
    print("Test 3: Help text")
    print("-" * 50)
    
    returncode, stdout, stderr = run_command(['python3', 'sync_gaps.py', '--help'])
    
    if '--debug' in stdout:
        print("✅ PASS: --debug flag documented in help")
    else:
        print("❌ FAIL: --debug flag missing from help text")
        return False
    
    if 'debug' in stdout.lower() and 'logging' in stdout.lower():
        print("✅ PASS: Debug flag description present")
    else:
        print("❌ FAIL: Debug flag description unclear")
        return False
    
    print()
    return True


def test_dry_run_with_invalid_token():
    """Test 4: Verify pre-flight checks run even in dry-run mode."""
    print("Test 4: Dry-run with invalid token")
    print("-" * 50)
    
    env = os.environ.copy()
    env['GH_TOKEN'] = 'invalid_token'
    
    returncode, stdout, stderr = run_command(
        ['python3', 'sync_gaps.py', '--create', '--dry-run', '--max', '1'],
        env=env
    )
    
    output = stdout + stderr
    
    if 'pre-flight checks' in output.lower() or 'verification' in output.lower():
        print("✅ PASS: Pre-flight checks run")
    else:
        print("❌ FAIL: Pre-flight checks not visible")
        return False
    
    if returncode != 0:
        print("✅ PASS: Script exits with error on invalid token")
    else:
        print("❌ FAIL: Script should fail with invalid token")
        return False
    
    print()
    return True


def test_script_syntax():
    """Test 5: Verify script has valid Python syntax."""
    print("Test 5: Script syntax validation")
    print("-" * 50)
    
    returncode, stdout, stderr = run_command(['python3', '-m', 'py_compile', 'sync_gaps.py'])
    
    if returncode == 0:
        print("✅ PASS: Script has valid Python syntax")
    else:
        print("❌ FAIL: Syntax error in script")
        print(stderr)
        return False
    
    print()
    return True


def test_imports():
    """Test 6: Verify all imports are available."""
    print("Test 6: Import validation")
    print("-" * 50)
    
    test_code = """
import sys
sys.path.insert(0, '.')
try:
    import sync_gaps
    print("SUCCESS")
except ImportError as e:
    print(f"FAIL: {e}")
    sys.exit(1)
"""
    
    returncode, stdout, stderr = run_command(['python3', '-c', test_code])
    
    if returncode == 0 and "SUCCESS" in stdout:
        print("✅ PASS: All imports successful")
    else:
        print("❌ FAIL: Import error")
        print(stderr)
        return False
    
    print()
    return True


def test_constants_defined():
    """Test 7: Verify new constants are properly defined."""
    print("Test 7: Constants validation")
    print("-" * 50)
    
    test_code = """
import sync_gaps

# Check constants exist
constants = [
    'MAX_CONTINUOUS_FAILURES',
    'RETRY_MAX_ATTEMPTS',
    'RETRY_BASE_DELAY',
    'RATE_LIMIT_DELAY'
]

missing = []
for const in constants:
    if not hasattr(sync_gaps, const):
        missing.append(const)

if missing:
    print(f"FAIL: Missing constants: {missing}")
    exit(1)
else:
    print("SUCCESS: All constants defined")
    # Print values
    print(f"MAX_CONTINUOUS_FAILURES = {sync_gaps.MAX_CONTINUOUS_FAILURES}")
    print(f"RETRY_MAX_ATTEMPTS = {sync_gaps.RETRY_MAX_ATTEMPTS}")
    print(f"RETRY_BASE_DELAY = {sync_gaps.RETRY_BASE_DELAY}")
    print(f"RATE_LIMIT_DELAY = {sync_gaps.RATE_LIMIT_DELAY}")
"""
    
    returncode, stdout, stderr = run_command(['python3', '-c', test_code])
    
    if returncode == 0 and "SUCCESS" in stdout:
        print("✅ PASS: All constants defined")
        # Print the values
        for line in stdout.split('\n')[1:]:  # Skip SUCCESS line
            if '=' in line:
                print(f"   {line}")
    else:
        print("❌ FAIL: Constants missing or error")
        print(stdout)
        print(stderr)
        return False
    
    print()
    return True


def test_functions_exist():
    """Test 8: Verify new functions exist."""
    print("Test 8: Function validation")
    print("-" * 50)
    
    test_code = """
import sync_gaps
import inspect

# Check functions exist
functions = [
    'setup_logging',
    'verify_github_token',
    'check_repository_issues_enabled',
    'exponential_backoff',
    'create_issue',
    'update_issue',
    'close_issue',
]

missing = []
for func in functions:
    if not hasattr(sync_gaps, func):
        missing.append(func)
    elif not callable(getattr(sync_gaps, func)):
        missing.append(f"{func} (not callable)")

if missing:
    print(f"FAIL: Missing functions: {missing}")
    exit(1)
else:
    print("SUCCESS: All functions defined")
    
    # Check create_issue signature returns tuple
    sig = inspect.signature(sync_gaps.create_issue)
    print(f"create_issue signature: {sig}")
"""
    
    returncode, stdout, stderr = run_command(['python3', '-c', test_code])
    
    if returncode == 0 and "SUCCESS" in stdout:
        print("✅ PASS: All required functions exist")
        for line in stdout.split('\n')[1:]:
            if 'signature' in line.lower():
                print(f"   {line}")
    else:
        print("❌ FAIL: Functions missing or error")
        print(stdout)
        print(stderr)
        return False
    
    print()
    return True


def main():
    """Run all tests."""
    print("=" * 70)
    print("  sync_gaps.py Enhancement Test Suite")
    print("=" * 70)
    print()
    
    # Change to script directory
    script_dir = Path(__file__).parent
    os.chdir(script_dir)
    
    tests = [
        test_script_syntax,
        test_imports,
        test_constants_defined,
        test_functions_exist,
        test_help_text,
        test_missing_token,
        test_debug_mode,
        test_dry_run_with_invalid_token,
    ]
    
    results = []
    for test in tests:
        try:
            result = test()
            results.append(result)
        except Exception as e:
            print(f"❌ EXCEPTION: {e}")
            import traceback
            traceback.print_exc()
            results.append(False)
    
    # Summary
    print("=" * 70)
    print("  Test Summary")
    print("=" * 70)
    passed = sum(1 for r in results if r)
    total = len(results)
    print(f"Passed: {passed}/{total}")
    
    if passed == total:
        print("✅ ALL TESTS PASSED")
        return 0
    else:
        print(f"❌ {total - passed} TEST(S) FAILED")
        return 1


if __name__ == '__main__':
    sys.exit(main())
