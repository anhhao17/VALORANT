"""
User management API test cases
"""

from api_functions import (
    login, assert_response_status, get_users, create_user, 
    get_user, update_user, delete_user, assert_json_field
)

if __name__ == "__main__":
    # Login as admin
    admin_token = login("admin", "admin")
    print("Login Admin Positive - PASS.")

    # Test get users with authentication
    response = get_users(admin_token)
    assert_response_status(response, 200)
    users = response.json()
    assert isinstance(users, list)
    print("Get Users Positive - PASS.")

    # Test get users without authentication
    response = get_users()
    assert_response_status(response, 401)
    print("Get Users Negative (no auth) - PASS.")

    # Create test user
    test_username = "testuser123"
    test_password = "testpass123"
    test_email = "test@example.com"
    
    response = create_user(admin_token, test_username, test_password, test_email, "user")
    assert_response_status(response, 200)
    print(f"Create User ({test_username}) Positive - PASS.")

    # Login as test user
    test_token = login(test_username, test_password)
    print(f"Login ({test_username}) Positive - PASS.")

    # Get specific user info
    response = get_user(admin_token, test_username)
    assert_response_status(response, 200)
    user_data = response.json()
    assert user_data["username"] == test_username
    assert user_data["email"] == test_email
    assert user_data["role"] == "user"
    print(f"Get User ({test_username}) Positive - PASS.")

    # Update user
    new_email = "updated@example.com"
    response = update_user(admin_token, test_username, email=new_email)
    assert_response_status(response, 200)
    print(f"Update User ({test_username}) Positive - PASS.")

    # Verify update
    response = get_user(admin_token, test_username)
    assert_response_status(response, 200)
    user_data = response.json()
    assert user_data["email"] == new_email
    print("Verify User Update - PASS.")

    # Test update user without authentication
    response = update_user(None, test_username, email="test@test.com")
    assert_response_status(response, 401)
    print("Update User Negative (no auth) - PASS.")

    # Delete test user
    response = delete_user(admin_token, test_username)
    assert_response_status(response, 200)
    print(f"Delete User ({test_username}) Positive - PASS.")

    # Verify deletion
    response = get_user(admin_token, test_username)
    assert_response_status(response, 404)
    print("Verify User Deletion - PASS.")

    # Test delete user without authentication
    response = delete_user(None, "admin")
    assert_response_status(response, 401)
    print("Delete User Negative (no auth) - PASS.")

    exit(0)
