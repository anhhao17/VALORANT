"""
Hardware monitoring API test cases
"""

from api_functions import login, assert_response_status, get_hardware_monitoring, assert_json_field

if __name__ == "__main__":
    # Login
    token = login("admin", "admin")
    print("Login Positive - PASS.")

    # Test temperature monitoring
    response = get_hardware_monitoring(token, "temperature")
    assert_response_status(response, 200)
    data = response.json()
    assert "temperature" in data
    assert "unit" in data
    print("Get Temperature Positive - PASS.")

    # Test temperature without authentication
    response = get_hardware_monitoring(None, "temperature")
    assert_response_status(response, 401)
    print("Get Temperature Negative (no auth) - PASS.")

    # Test power monitoring
    response = get_hardware_monitoring(token, "power")
    assert_response_status(response, 200)
    data = response.json()
    assert "power" in data
    assert "unit" in data
    print("Get Power Positive - PASS.")

    # Test power without authentication
    response = get_hardware_monitoring(None, "power")
    assert_response_status(response, 401)
    print("Get Power Negative (no auth) - PASS.")

    # Test fans monitoring
    response = get_hardware_monitoring(token, "fans")
    assert_response_status(response, 200)
    data = response.json()
    assert "fans" in data
    print("Get Fans Positive - PASS.")

    # Test fans without authentication
    response = get_hardware_monitoring(None, "fans")
    assert_response_status(response, 401)
    print("Get Fans Negative (no auth) - PASS.")

    # Test voltage monitoring
    response = get_hardware_monitoring(token, "voltage")
    assert_response_status(response, 200)
    data = response.json()
    assert "voltage" in data
    assert "unit" in data
    print("Get Voltage Positive - PASS.")

    # Test voltage without authentication
    response = get_hardware_monitoring(None, "voltage")
    assert_response_status(response, 401)
    print("Get Voltage Negative (no auth) - PASS.")

    exit(0)
