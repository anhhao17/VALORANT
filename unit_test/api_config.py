"""
Configuration API test cases
"""

from api_functions import login, assert_response_status, get_config, update_config, assert_json_field

if __name__ == "__main__":
    # Login
    token = login("admin", "admin")
    print("Login Positive - PASS.")

    # Test get config with authentication
    response = get_config(token)
    assert_response_status(response, 200)
    data = response.json()
    assert "server" in data
    assert "hardware" in data
    assert "security" in data
    print("Get Config Positive - PASS.")

    # Test get config without authentication
    response = get_config()
    assert_response_status(response, 401)
    print("Get Config Negative (no auth) - PASS.")

    # Test update config with authentication
    original_config = response.json()
    
    # Modify server config
    update_data = {
        "server": {
            "host": original_config["server"]["host"],
            "port": 8081,  # Change port
            "enable_ssl": original_config["server"]["enable_ssl"],
            "max_connections": original_config["server"]["max_connections"],
            "session_timeout": original_config["server"]["session_timeout"],
            "log_level": original_config["server"]["log_level"]
        }
    }
    
    response = update_config(token, update_data)
    assert_response_status(response, 200)
    print("Update Config Positive - PASS.")

    # Verify config was updated
    response = get_config(token)
    assert_response_status(response, 200)
    data = response.json()
    assert data["server"]["port"] == 8081
    print("Verify Config Update - PASS.")

    # Restore original config
    update_data["server"]["port"] = original_config["server"]["port"]
    response = update_config(token, update_data)
    assert_response_status(response, 200)
    print("Restore Config - PASS.")

    # Test update config without authentication
    response = update_config(None, update_data)
    assert_response_status(response, 401)
    print("Update Config Negative (no auth) - PASS.")

    exit(0)
