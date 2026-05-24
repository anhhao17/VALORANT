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

# Configuration
HOST = "localhost"
PORT = 8080
BASE_URL = f"http://{HOST}:{PORT}"
AUTH = ("admin", "password")

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
    
    # Test new authentication endpoints
    print("Testing new authentication endpoints...")
    
    # Test login with valid credentials
    login_response = test_endpoint(
        "Login with valid credentials",
        "/api/login",
        200,
        use_auth=False,
        method="POST",
        data={"username": "admin", "password": "password"}
    )
    
    session_token = None
    csrf_token = None
    
    if login_response and "sessionToken" in login_response:
        session_token = login_response["sessionToken"]
        csrf_token = login_response.get("csrfToken", "")
        print_result("Login response contains sessionToken", "PASS")
    else:
        print_result("Login response contains sessionToken", "FAIL")
    
    # Test login with invalid credentials
    test_endpoint(
        "Login with invalid credentials",
        "/api/login",
        401,
        use_auth=False,
        method="POST",
        data={"username": "admin", "password": "wrongpassword"}
    )
    
    # Test session info with valid token
    if session_token:
        headers = {"Authorization": f"Token {session_token}"}
        try:
            response = requests.get(f"{BASE_URL}/api/session", headers=headers)
            if response.status_code == 200:
                print_result("Get session info with valid token", "PASS")
            else:
                print_result("Get session info with valid token", "FAIL", f"HTTP {response.status_code}")
        except requests.exceptions.RequestException as e:
            print_result("Get session info with valid token", "FAIL", str(e))
    
    # Test logout with valid token
    if session_token:
        headers = {"Authorization": f"Token {session_token}"}
        try:
            response = requests.post(f"{BASE_URL}/api/logout", headers=headers)
            if response.status_code == 200:
                print_result("Logout with valid token", "PASS")
            else:
                print_result("Logout with valid token", "FAIL", f"HTTP {response.status_code}")
        except requests.exceptions.RequestException as e:
            print_result("Logout with valid token", "FAIL", str(e))
    
    # Test session info after logout (should fail)
    if session_token:
        headers = {"Authorization": f"Token {session_token}"}
        try:
            response = requests.get(f"{BASE_URL}/api/session", headers=headers)
            if response.status_code == 401:
                print_result("Session info after logout (should fail)", "PASS")
            else:
                print_result("Session info after logout (should fail)", "FAIL", f"Expected 401, got {response.status_code}")
        except requests.exceptions.RequestException as e:
            print_result("Session info after logout (should fail)", "FAIL", str(e))
    
    # Test CSRF protection (POST without CSRF token should fail)
    if session_token and csrf_token:
        # First login again to get a fresh session
        login_response = test_endpoint(
            "Login for CSRF test",
            "/api/login",
            200,
            use_auth=False,
            method="POST",
            data={"username": "admin", "password": "password"}
        )
        if login_response:
            session_token = login_response.get("sessionToken")
            csrf_token = login_response.get("csrfToken", "")
            
            # Try POST without CSRF token
            headers = {"Authorization": f"Token {session_token}"}
            try:
                response = requests.post(f"{BASE_URL}/api/system/reboot", headers=headers)
                if response.status_code == 403:
                    print_result("CSRF protection (POST without token)", "PASS")
                else:
                    print_result("CSRF protection (POST without token)", "FAIL", f"Expected 403, got {response.status_code}")
            except requests.exceptions.RequestException as e:
                print_result("CSRF protection (POST without token)", "FAIL", str(e))
    
    print()
    
    # Test authentication
    print("Testing authentication...")
    test_endpoint("Test endpoint without auth", "/api/test", 401, use_auth=False)
    test_endpoint_with_json_validation(
        "Test endpoint with auth", "/api/test", 200, "message", use_auth=True
    )
    
    print()
    print("Testing system endpoints...")
    
    # System endpoints
    test_endpoint_with_json_validation(
        "System info", "/api/system/info", 200, "hostname"
    )
    test_endpoint_with_json_validation(
        "System status", "/api/system/status", 200, "health"
    )
    test_endpoint_with_json_validation(
        "System reboot", "/api/system/reboot", 202, "message"
    )
    
    print()
    print("Testing hardware monitoring endpoints...")
    
    # Hardware monitoring endpoints
    test_endpoint_with_json_validation(
        "Temperature sensors", "/api/hwmon/temperature", 200, "cpu"
    )
    test_endpoint_with_json_validation(
        "Power sensors", "/api/hwmon/power", 200, "total"
    )
    test_endpoint_with_json_validation(
        "Fan speeds", "/api/hwmon/fans", 200, "fan1"
    )
    test_endpoint_with_json_validation(
        "Voltage sensors", "/api/hwmon/voltage", 200, "vdd_cpu"
    )
    
    print()
    print("Testing error cases...")
    
    # Error cases
    test_endpoint("Non-existent endpoint", "/api/nonexistent", 404)
    test_endpoint("Invalid auth header", "/api/test", 401, use_auth=False)
    
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
