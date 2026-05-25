#!/usr/bin/env python3
"""
Authentication Test Module for Jetson BMCweb
Tests authentication endpoints and session management
Can be imported and used by other test scripts
"""

import requests
import json
from typing import Dict, Any, Optional, Tuple


class AuthTester:
    """Reusable authentication test class"""
    
    def __init__(self, base_url: str = "http://localhost:8080"):
        self.base_url = base_url
        self.session_token: Optional[str] = None
        self.csrf_token: Optional[str] = None
        self.username: Optional[str] = None
    
    def login(self, username: str = "admin", password: str = "admin") -> Tuple[bool, Optional[Dict[str, Any]]]:
        """
        Login and store session tokens
        
        Returns:
            Tuple of (success, response_data)
        """
        url = f"{self.base_url}/api/login"
        
        try:
            response = requests.post(url, json={"username": username, "password": password})
            
            if response.status_code == 200:
                data = response.json()
                self.session_token = data.get("sessionToken")
                self.csrf_token = data.get("csrfToken")
                self.username = data.get("username")
                return True, data
            else:
                return False, {"error": f"Login failed with status {response.status_code}"}
                
        except requests.exceptions.RequestException as e:
            return False, {"error": str(e)}
    
    def logout(self) -> Tuple[bool, Optional[Dict[str, Any]]]:
        """
        Logout and clear session tokens
        
        Returns:
            Tuple of (success, response_data)
        """
        if not self.session_token:
            return False, {"error": "No active session"}
        
        url = f"{self.base_url}/api/logout"
        headers = {"Authorization": f"Token {self.session_token}"}
        
        try:
            response = requests.post(url, headers=headers)
            
            if response.status_code == 200:
                self.session_token = None
                self.csrf_token = None
                self.username = None
                return True, response.json()
            else:
                return False, {"error": f"Logout failed with status {response.status_code}"}
                
        except requests.exceptions.RequestException as e:
            return False, {"error": str(e)}
    
    def get_session_info(self) -> Tuple[bool, Optional[Dict[str, Any]]]:
        """
        Get current session information
        
        Returns:
            Tuple of (success, response_data)
        """
        if not self.session_token:
            return False, {"error": "No active session"}
        
        url = f"{self.base_url}/api/session"
        headers = {"Authorization": f"Token {self.session_token}"}
        
        try:
            response = requests.get(url, headers=headers)
            
            if response.status_code == 200:
                return True, response.json()
            else:
                return False, {"error": f"Session check failed with status {response.status_code}"}
                
        except requests.exceptions.RequestException as e:
            return False, {"error": str(e)}
    
    def make_authenticated_request(
        self,
        method: str,
        endpoint: str,
        data: Optional[Dict[str, Any]] = None,
        include_csrf: bool = True
    ) -> Tuple[bool, Optional[Dict[str, Any]]]:
        """
        Make an authenticated request with session token
        
        Args:
            method: HTTP method (GET, POST, PUT, DELETE)
            endpoint: API endpoint path
            data: Request body data
            include_csrf: Whether to include CSRF token for non-GET requests
            
        Returns:
            Tuple of (success, response_data)
        """
        if not self.session_token:
            return False, {"error": "No active session"}
        
        url = f"{self.base_url}{endpoint}"
        headers = {"Authorization": f"Token {self.session_token}"}
        
        # Add CSRF token for non-GET requests
        if method.upper() != "GET" and include_csrf and self.csrf_token:
            headers["X-CSRF-Token"] = self.csrf_token
        
        try:
            if method.upper() == "GET":
                response = requests.get(url, headers=headers)
            elif method.upper() == "POST":
                response = requests.post(url, json=data, headers=headers)
            elif method.upper() == "PUT":
                response = requests.put(url, json=data, headers=headers)
            elif method.upper() == "DELETE":
                response = requests.delete(url, headers=headers)
            else:
                return False, {"error": f"Unsupported method: {method}"}
            
            if response.status_code in [200, 201, 202]:
                try:
                    return True, response.json()
                except json.JSONDecodeError:
                    return True, {"status": "success"}
            else:
                return False, {"error": f"Request failed with status {response.status_code}"}
                
        except requests.exceptions.RequestException as e:
            return False, {"error": str(e)}
    
    def is_authenticated(self) -> bool:
        """Check if currently authenticated"""
        return self.session_token is not None
    
    def get_auth_headers(self) -> Dict[str, str]:
        """Get authentication headers for use with requests"""
        if not self.session_token:
            return {}
        
        headers = {"Authorization": f"Token {self.session_token}"}
        if self.csrf_token:
            headers["X-CSRF-Token"] = self.csrf_token
        
        return headers


def run_auth_tests(base_url: str = "http://localhost:8080") -> Dict[str, bool]:
    """
    Run standard authentication tests
    
    Args:
        base_url: Base URL for the API
        
    Returns:
        Dictionary of test names and their pass/fail status
    """
    results = {}
    auth = AuthTester(base_url)
    
    # Test 1: Login with valid credentials
    success, data = auth.login("admin", "admin")
    results["Login with valid credentials"] = success and "sessionToken" in data
    
    # Test 2: Login with invalid credentials
    success, _ = auth.login("admin", "wrongpassword")
    results["Login with invalid credentials"] = not success  # Should fail
    
    # Login again for subsequent tests
    auth.login("admin", "admin")
    
    # Test 3: Get session info
    success, data = auth.get_session_info()
    results["Get session info"] = success and "username" in data
    
    # Test 4: Logout
    success, _ = auth.logout()
    results["Logout"] = success
    
    # Test 5: Session invalid after logout
    success, _ = auth.get_session_info()
    results["Session invalid after logout"] = not success  # Should fail
    
    # Test 6: CSRF protection
    # For CSRF test, we need to use cookie-based authentication
    # Create a session with cookies
    session = requests.Session()
    login_response = session.post(f"{base_url}/api/login", json={"username": "admin", "password": "admin"})
    
    if login_response.status_code == 200:
        # Try POST without CSRF token using the session (which has the cookie)
        response = session.post(f"{base_url}/api/system/reboot")
        results["CSRF protection"] = response.status_code == 403  # Should fail without CSRF token
    else:
        results["CSRF protection"] = False
    
    # Cleanup
    auth.logout()
    
    return results


if __name__ == "__main__":
    """Run authentication tests when executed directly"""
    import sys
    
    print("=" * 50)
    print("Authentication Test Module")
    print("=" * 50)
    
    results = run_auth_tests()
    
    print("\nTest Results:")
    for test_name, passed in results.items():
        status = "✓ PASS" if passed else "✗ FAIL"
        print(f"{status}: {test_name}")
    
    total = len(results)
    passed = sum(1 for v in results.values() if v)
    failed = total - passed
    
    print(f"\nTotal: {total}, Passed: {passed}, Failed: {failed}")
    
    sys.exit(0 if failed == 0 else 1)
