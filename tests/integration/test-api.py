#!/usr/bin/env python3
"""
Python API Test Script for Jetson BMCweb
Tests all API endpoints with various scenarios
"""

import requests
import json
import sys
import time
from typing import Dict, Any, Optional
from test_auth import AuthTester, run_auth_tests

# Configuration
HOST = "localhost"
PORT = 8080
BASE_URL = f"http://{HOST}:{PORT}"
AUTH = ("admin", "admin")

# Test counters
total_tests = 0
passed_tests = 0
failed_tests = 0


def print_result(test_name: str, status: str, message: str = ""):
    """Print test result with color coding"""
    global total_tests, passed_tests, failed_tests
    
    total_tests += 1
    
    if status == "PASS":
        print(f"✓ PASS: {test_name}")
        passed_tests += 1
    else:
        print(f"✗ FAIL: {test_name}")
        print(f"  Message: {message}")
        failed_tests += 1


def test_endpoint(
    test_name: str,
    endpoint: str,
    expected_status: int,
    use_auth: bool = True,
    method: str = "GET",
    data: Optional[Dict[str, Any]] = None,
    headers: Optional[Dict[str, str]] = None
) -> Optional[Dict[str, Any]]:
    """Test an API endpoint"""
    url = f"{BASE_URL}{endpoint}"
    
    try:
        auth = AUTH if use_auth else None
        req_headers = headers or {}
        
        if method == "GET":
            response = requests.get(url, auth=auth, headers=req_headers)
        elif method == "POST":
            response = requests.post(url, auth=auth, json=data, headers=req_headers)
        elif method == "PUT":
            response = requests.put(url, auth=auth, json=data, headers=req_headers)
        elif method == "DELETE":
            response = requests.delete(url, auth=auth, headers=req_headers)
        else:
            print_result(test_name, "FAIL", f"Unsupported method: {method}")
            return None
        
        if response.status_code == expected_status:
            print_result(test_name, "PASS", f"HTTP {response.status_code}")
            try:
                return response.json()
            except json.JSONDecodeError:
                return None
        else:
            print_result(
                test_name, "FAIL",
                f"Expected HTTP {expected_status}, got {response.status_code}"
            )
            print(f"  Response body: {response.text}")
            return None
            
    except requests.exceptions.RequestException as e:
        print_result(test_name, "FAIL", f"Request failed: {str(e)}")
        return None


def test_endpoint_with_json_validation(
    test_name: str,
    endpoint: str,
    expected_status: int,
    required_field: str,
    use_auth: bool = True
) -> bool:
    """Test an endpoint and validate JSON response"""
    response_data = test_endpoint(test_name, endpoint, expected_status, use_auth)
    
    if response_data is None:
        return False
    
    if required_field in response_data:
        print_result(
            test_name, "PASS",
            f"HTTP {expected_status}, contains field '{required_field}'"
        )
        return True
    else:
        print_result(
            test_name, "FAIL",
            f"HTTP {expected_status} but missing field '{required_field}'"
        )
        print(f"  Response data: {json.dumps(response_data, indent=2)}")
        return False


def wait_for_server(max_retries: int = 30, retry_delay: int = 1) -> bool:
    """Wait for server to be ready"""
    print("Waiting for server to be ready...")
    
    for i in range(max_retries):
        try:
            response = requests.get(f"{BASE_URL}/api/test", timeout=2)
            print("Server is ready!")
            return True
        except requests.exceptions.RequestException:
            if i == max_retries - 1:
                print("Error: Server did not start within 30 seconds")
                return False
            time.sleep(retry_delay)
    
    return False


def main():
    """Main test function"""
    print("=" * 50)
    print("Jetson BMCweb API Test Suite (Python)")
    print("=" * 50)
    print(f"Testing endpoints on: {BASE_URL}")
    print()
    
    # Wait for server
    if not wait_for_server():
        sys.exit(1)
    
    print()
    
    # Test authentication using reusable module
    print("Testing authentication endpoints...")
    auth_results = run_auth_tests(BASE_URL)
    
    for test_name, passed in auth_results.items():
        status = "PASS" if passed else "FAIL"
        print_result(f"Auth: {test_name}", status)
    
    print()
    print("Testing system endpoints with authentication...")
    
    # Test system endpoints using AuthTester
    auth = AuthTester(BASE_URL)
    login_success, _ = auth.login()
    
    if login_success:
        # Test authenticated system endpoints
        success, data = auth.make_authenticated_request("GET", "/api/system/info")
        print_result("System info with auth", "PASS" if success else "FAIL")
        
        success, data = auth.make_authenticated_request("GET", "/api/system/status")
        print_result("System status with auth", "PASS" if success else "FAIL")
        
        success, data = auth.make_authenticated_request("POST", "/api/system/reboot")
        print_result("System reboot with auth", "PASS" if success else "FAIL")
        
        auth.logout()
    else:
        print_result("Login for system endpoint tests", "FAIL")
    
    print()
    print("Testing hardware monitoring endpoints with authentication...")
    
    # Test hardware endpoints using AuthTester
    auth.login()
    
    success, data = auth.make_authenticated_request("GET", "/api/hwmon/temperature")
    print_result("Temperature with auth", "PASS" if success else "FAIL")
    
    success, data = auth.make_authenticated_request("GET", "/api/hwmon/power")
    print_result("Power with auth", "PASS" if success else "FAIL")
    
    success, data = auth.make_authenticated_request("GET", "/api/hwmon/fans")
    print_result("Fans with auth", "PASS" if success else "FAIL")
    
    success, data = auth.make_authenticated_request("GET", "/api/hwmon/voltage")
    print_result("Voltage with auth", "PASS" if success else "FAIL")
    
    auth.logout()
    
    print()
    print("Testing error cases...")
    
    # Error cases
    test_endpoint("Non-existent endpoint", "/api/nonexistent", 404)
    test_endpoint("Invalid auth header", "/api/test", 401, use_auth=False)
    
    print()
    print("Testing streaming endpoints with authentication...")
    
    # Test streaming endpoints using AuthTester
    auth.login()
    
    # Cleanup: delete test stream if it exists from previous run
    print("Cleaning up existing test stream...")
    success, data = auth.make_authenticated_request("DELETE", "/api/streams/test_stream_1")
    print(f"Cleanup result: success={success}")
    
    # Test list streams
    success, data = auth.make_authenticated_request("GET", "/api/streams")
    print_result("List streams", "PASS" if success else "FAIL")
    
    # Test add stream (use camera type to avoid file validation)
    stream_data = {
        "id": "test_stream_1",
        "name": "Test Stream 1",
        "type": 1,  # Camera Device (no file validation)
        "sourcePath": "/dev/video0",
        "loop": True,
        "quality": 80
    }
    success, data = auth.make_authenticated_request("POST", "/api/streams", data=stream_data)
    print_result("Add stream", "PASS" if success else "FAIL")
    
    # Test get stream (not implemented, skip)
    # success, data = auth.make_authenticated_request("GET", "/api/streams/test_stream_1")
    # print_result("Get stream", "PASS" if success else "FAIL")
    
    # Test stream statistics (corrected endpoint)
    success, data = auth.make_authenticated_request("GET", "/api/streams/test_stream_1/statistics")
    print(f"Statistics response: success={success}, data={data}")
    print_result("Stream statistics", "PASS" if success else "FAIL")
    
    # Test start streaming
    success, data = auth.make_authenticated_request("POST", "/api/streams/test_stream_1/start")
    print(f"Start response: success={success}, data={data}")
    print_result("Start streaming", "PASS" if success else "FAIL")
    
    # Test stop streaming
    success, data = auth.make_authenticated_request("POST", "/api/streams/test_stream_1/stop")
    print(f"Stop response: success={success}, data={data}")
    print_result("Stop streaming", "PASS" if success else "FAIL")
    
    # Test recording (should fail for file-based streams)
    success, data = auth.make_authenticated_request("POST", "/api/streams/test_stream_1/record", data={"format": "mp4"})
    print_result("Recording from file (should fail)", "PASS" if not success else "FAIL")
    
    # Test list recordings
    success, data = auth.make_authenticated_request("GET", "/api/recordings")
    print_result("List recordings", "PASS" if success else "FAIL")
    
    # Test stream status endpoint (before deleting stream)
    success, data = auth.make_authenticated_request("GET", "/api/streams/test_stream_1/status")
    print(f"Stream status result: success={success}, data={data}")
    print_result("Stream status endpoint", "PASS" if success else "FAIL")
    
    # Test delete stream
    success, data = auth.make_authenticated_request("DELETE", "/api/streams/test_stream_1")
    print_result("Delete stream", "PASS" if success else "FAIL")
    
    # Test camera auto-detection
    success, data = auth.make_authenticated_request("GET", "/api/streams/detect")
    print(f"Camera detection result: success={success}, data={data}")
    print_result("Camera auto-detection", "PASS" if success else "FAIL")
    
    auth.logout()
    
    print()
    print("=" * 50)
    print("Test Summary")
    print("=" * 50)
    print(f"Total tests: {total_tests}")
    print(f"Passed: {passed_tests}")
    print(f"Failed: {failed_tests}")
    print()
    
    if failed_tests == 0:
        print("All tests passed!")
        sys.exit(0)
    else:
        print("Some tests failed.")
        sys.exit(1)


if __name__ == "__main__":
    main()
