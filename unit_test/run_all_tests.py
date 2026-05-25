"""
Run all unit tests for Jetson BMCweb
"""

import subprocess
import sys
import os

# List of test scripts to run
test_scripts = [
    "api_login.py",
    "api_system.py", 
    "api_config.py",
    "api_users.py",
    "api_hardware.py",
    "api_streaming.py"
]

def run_test(script_name):
    """Run a single test script and return success status"""
    print(f"\n{'='*60}")
    print(f"Running: {script_name}")
    print(f"{'='*60}")
    
    try:
        result = subprocess.run(
            [sys.executable, script_name],
            cwd=os.path.dirname(os.path.abspath(__file__)),
            capture_output=True,
            text=True,
            timeout=120
        )
        
        # Print output
        if result.stdout:
            print(result.stdout)
        if result.stderr:
            print("STDERR:", result.stderr)
        
        if result.returncode == 0:
            print(f"✅ {script_name} - PASSED")
            return True
        else:
            print(f"❌ {script_name} - FAILED (exit code: {result.returncode})")
            return False
    except subprocess.TimeoutExpired:
        print(f"❌ {script_name} - TIMEOUT")
        return False
    except Exception as e:
        print(f"❌ {script_name} - ERROR: {e}")
        return False

def main():
    """Main test runner"""
    print("Jetson BMCweb Unit Test Suite")
    print("="*60)
    
    passed = 0
    failed = 0
    
    for script in test_scripts:
        if run_test(script):
            passed += 1
        else:
            failed += 1
    
    # Print summary
    print(f"\n{'='*60}")
    print("TEST SUMMARY")
    print(f"{'='*60}")
    print(f"Total tests: {len(test_scripts)}")
    print(f"Passed: {passed}")
    print(f"Failed: {failed}")
    print(f"Success rate: {passed/len(test_scripts)*100:.1f}%")
    print(f"{'='*60}")
    
    if failed > 0:
        sys.exit(1)
    else:
        print("✅ All tests passed!")
        sys.exit(0)

if __name__ == "__main__":
    main()
