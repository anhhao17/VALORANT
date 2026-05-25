"""
Login API test cases
"""

from api_functions import login, login_response, assert_response_status

long_str1 = "ogjqmufkygmbpmctpbbnfrufgbwizahotarmngpurzntkxolfkzozoxjfryqztncrgtoxkfvvzwzmlgjjoggprmtrwgggcflhmhqznxpfijfhekitwslrdaiylenejcgxltdmmqjjiihanbsgdmokaogdhgj"
long_str2 = "ybrqiklvoghifmaxazqhfpzzrzgokxrefyskrwyckutoxtnhhnazhdkdcusxhynwousoxaaajivpydcftpemyqtpmlwwwaazxkhgfvkefg"

if __name__ == "__main__":
    # Positive test: valid login
    token = login("admin", "admin")
    print("Login. Positive - PASS.")

    # Negative test: invalid credentials
    response = login_response(long_str1, long_str2)
    assert_response_status(response, 401)
    print("Login. Negative 1 - PASS.")

    # Negative test: empty password
    response = login_response("admin", "")
    assert_response_status(response, 401)
    print("Login. Negative 2 - PASS.")

    # Negative test: empty username
    response = login_response("", "admin")
    assert_response_status(response, 401)
    print("Login. Negative 3 - PASS.")

    # Positive test: login again and verify token consistency
    token2 = login("admin", "admin")
    print("Login. Positive - PASS.")

    assert token2 == token, f"Token comparison. - FAIL"
    print("Login. Token comparison - PASS.")
    
    exit(0)
