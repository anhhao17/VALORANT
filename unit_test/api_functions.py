"""
Common API test functions for Jetson BMCweb unit tests
Adapted from reference project structure
"""

import requests
import inspect
import os
import time
import json
from requests.packages.urllib3.exceptions import InsecureRequestWarning
requests.packages.urllib3.disable_warnings(category=InsecureRequestWarning)

# Configuration
TimeoutS = 60
BaseURL = "http://127.0.0.1:8080"
SecureVerify = False
DefaultAdmin = "admin"
DefaultAdminPass = "admin"


def log_fmt(message):
    """Format log message with file, line, and function info"""
    caller_frame = inspect.currentframe().f_back
    raw_filename = caller_frame.f_code.co_filename
    filename = os.path.basename(raw_filename)
    line_number = caller_frame.f_lineno
    function_name = caller_frame.f_code.co_name
    return f"{filename}:{line_number}:{function_name}() {message}"


def log(message=None):
    """Print log message with file, line, and function info"""
    caller_frame = inspect.currentframe().f_back
    raw_filename = caller_frame.f_code.co_filename
    filename = os.path.basename(raw_filename)
    line_number = caller_frame.f_lineno
    function_name = caller_frame.f_code.co_name
    if message is not None:
        print(f"{filename}:{line_number}:{function_name}(): {message}")
    else:
        print(f"{filename}:{line_number}:{function_name}()")


def assert_response_status(response, status):
    """Assert response has expected status code"""
    assert response.status_code == status, f"Response {response.status_code}, expected {status}."


def assert_json_field(response, field_name, expected_value=None):
    """Assert JSON response contains expected field"""
    try:
        data = response.json()
        assert field_name in data, f"Field '{field_name}' not found in response"
        if expected_value is not None:
            assert data[field_name] == expected_value, f"Field '{field_name}' value mismatch: {data[field_name]} != {expected_value}"
        return data[field_name]
    except json.JSONDecodeError:
        assert False, f"Response is not valid JSON: {response.text}"


def assert_json_response(response):
    """Assert response is valid JSON"""
    try:
        return response.json()
    except json.JSONDecodeError:
        assert False, f"Response is not valid JSON: {response.text}"


def login(username=DefaultAdmin, password=DefaultAdminPass):
    """Login and return session token"""
    response = login_response(username, password)
    assert_response_status(response, 200)
    return read_token(response)


def login_response(username, password):
    """Login and return response"""
    data = {
        "username": username,
        "password": password
    }
    response = requests.post(f"{BaseURL}/api/login", json=data, timeout=TimeoutS, verify=SecureVerify)
    return response


def read_token(response_text):
    """Extract session token from login response"""
    try:
        data = json.loads(response_text)
        assert "sessionToken" in data, "sessionToken not found in response"
        log(f"Token: {data['sessionToken']}")
        return data["sessionToken"]
    except json.JSONDecodeError:
        assert False, f"Response is not valid JSON: {response_text}"


def logout(token):
    """Logout with session token"""
    cookies = {"SESSION": token}
    response = requests.post(f"{BaseURL}/api/logout", cookies=cookies, timeout=TimeoutS, verify=SecureVerify)
    assert_response_status(response, 200)
    log("Logout - PASS.")


def get_system_info(token=None):
    """Get system information"""
    cookies = {"SESSION": token} if token else None
    response = requests.get(f"{BaseURL}/api/system/info", cookies=cookies, timeout=TimeoutS, verify=SecureVerify)
    return response


def get_system_status(token=None):
    """Get system status"""
    cookies = {"SESSION": token} if token else None
    response = requests.get(f"{BaseURL}/api/system/status", cookies=cookies, timeout=TimeoutS, verify=SecureVerify)
    return response


def get_config(token=None):
    """Get system configuration"""
    cookies = {"SESSION": token} if token else None
    response = requests.get(f"{BaseURL}/api/config", cookies=cookies, timeout=TimeoutS, verify=SecureVerify)
    return response


def update_config(token, config_data):
    """Update system configuration"""
    cookies = {"SESSION": token}
    response = requests.put(f"{BaseURL}/api/config", json=config_data, cookies=cookies, timeout=TimeoutS, verify=SecureVerify)
    return response


def get_users(token):
    """Get all users"""
    cookies = {"SESSION": token}
    response = requests.get(f"{BaseURL}/api/users", cookies=cookies, timeout=TimeoutS, verify=SecureVerify)
    return response


def create_user(token, username, password, email=None, role="user"):
    """Create new user"""
    cookies = {"SESSION": token}
    data = {
        "username": username,
        "password": password,
        "role": role
    }
    if email:
        data["email"] = email
    response = requests.post(f"{BASE_URL}/api/users", json=data, cookies=cookies, timeout=TimeoutS, verify=SecureVerify)
    return response


def get_user(token, username):
    """Get specific user"""
    cookies = {"SESSION": token}
    response = requests.get(f"{BaseURL}/api/users/{username}", cookies=cookies, timeout=TimeoutS, verify=SecureVerify)
    return response


def update_user(token, username, email=None, role=None):
    """Update user"""
    cookies = {"SESSION": token}
    data = {}
    if email:
        data["email"] = email
    if role:
        data["role"] = role
    response = requests.put(f"{BaseURL}/api/users/{username}", json=data, cookies=cookies, timeout=TimeoutS, verify=SecureVerify)
    return response


def delete_user(token, username):
    """Delete user"""
    cookies = {"SESSION": token}
    response = requests.delete(f"{BaseURL}/api/users/{username}", cookies=cookies, timeout=TimeoutS, verify=SecureVerify)
    return response


def get_hardware_monitoring(token, endpoint):
    """Get hardware monitoring data (temperature, power, fans, voltage)"""
    cookies = {"SESSION": token}
    response = requests.get(f"{BaseURL}/api/hwmon/{endpoint}", cookies=cookies, timeout=TimeoutS, verify=SecureVerify)
    return response


def get_streams(token):
    """Get all streams"""
    cookies = {"SESSION": token}
    response = requests.get(f"{BaseURL}/api/streams", cookies=cookies, timeout=TimeoutS, verify=SecureVerify)
    return response


def start_stream(token, stream_id):
    """Start streaming"""
    cookies = {"SESSION": token}
    response = requests.post(f"{BaseURL}/api/streams/{stream_id}/start", cookies=cookies, timeout=TimeoutS, verify=SecureVerify)
    return response


def stop_stream(token, stream_id):
    """Stop streaming"""
    cookies = {"SESSION": token}
    response = requests.post(f"{BaseURL}/api/streams/{stream_id}/stop", cookies=cookies, timeout=TimeoutS, verify=SecureVerify)
    return response


def get_stream_status(token, stream_id):
    """Get stream status"""
    cookies = {"SESSION": token}
    response = requests.get(f"{BaseURL}/api/streams/{stream_id}/status", cookies=cookies, timeout=TimeoutS, verify=SecureVerify)
    return response


def get_stream_statistics(token, stream_id):
    """Get stream statistics"""
    cookies = {"SESSION": token}
    response = requests.get(f"{Base_URL}/api/streams/{stream_id}/statistics", cookies=cookies, timeout=TimeoutS, verify=SecureVerify)
    return response


def wait_services_started():
    """Wait for services to be ready"""
    max_retries = 30
    retry_delay = 2
    for i in range(max_retries):
        try:
            response = requests.get(f"{BaseURL}/api/system/info", timeout=5, verify=SecureVerify)
            if response.status_code == 200:
                log(f"Services started after {i * retry_delay} seconds")
                return True
        except requests.exceptions.RequestException:
            log(f"Waiting for services... ({i + 1}/{max_retries})")
            time.sleep(retry_delay)
    assert False, "Services failed to start within timeout period"


def wait_services_stopped():
    """Wait for services to stop"""
    max_retries = 30
    retry_delay = 2
    for i in range(max_retries):
        try:
            response = requests.get(f"{BaseURL}/api/system/info", timeout=5, verify=SecureVerify)
            if response.status_code != 200:
                log(f"Services stopped after {i * retry_delay} seconds")
                return True
        except requests.exceptions.RequestException:
            log(f"Services stopped... ({i + 1}/{max_retries})")
            return True
        time.sleep(retry_delay)
    assert False, "Services failed to stop within timeout period"
