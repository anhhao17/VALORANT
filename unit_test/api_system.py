"""
System API test cases
"""

from api_functions import login, assert_response_status, get_system_info, get_system_status, assert_json_field

if __name__ == "__main__":
    # Login
    token = login("admin", "admin")
    print("Login Positive - PASS.")

    # Test system info with authentication
    response = get_system_info(token)
    assert_response_status(response, 200)
    assert_json_field(response, "hostname")
    assert_json_field(response, "version")
    assert_json_field(response, "model")
    print("Get System Info Positive - PASS.")

    # Test system info without authentication
    response = get_system_info()
    assert_response_status(response, 401)
    print("Get System Info Negative (no auth) - PASS.")

    # Test system status with authentication
    response = get_system_status(token)
    assert_response_status(response, 200)
    assert_json_field(response, "status")
    print("Get System Status Positive - PASS.")

    # Test system status without authentication
    response = get_system_status()
    assert_response_status(response, 401)
    print("Get System Status Negative (no auth) - PASS.")

    exit(0)
