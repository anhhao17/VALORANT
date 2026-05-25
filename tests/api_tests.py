"""
FastAPI Test Suite for Jetson BMCweb API Endpoints

This module provides comprehensive API tests for all documented endpoints
using FastAPI and pytest for testing the C++ backend.
"""

import pytest
import requests
import json
from typing import Dict, Any, Optional
import time

# Base URL for the C++ backend
BASE_URL = "http://localhost:8080"

# Test credentials
ADMIN_USERNAME = "admin"
ADMIN_PASSWORD = "admin"
TEST_USERNAME = "testuser"
TEST_PASSWORD = "testpass"

class TestAuth:
    """Authentication and session management tests"""
    
    def test_login_success(self):
        """Test successful login with valid credentials"""
        response = requests.post(f"{BASE_URL}/api/login", json={
            "username": ADMIN_USERNAME,
            "password": ADMIN_PASSWORD
        })
        assert response.status_code == 200
        data = response.json()
        assert "sessionToken" in data
        assert "csrfToken" in data
        assert "username" in data
        assert "role" in data
    
    def test_login_invalid_credentials(self):
        """Test login with invalid credentials"""
        response = requests.post(f"{BASE_URL}/api/login", json={
            "username": "invalid",
            "password": "invalid"
        })
        assert response.status_code == 401
    
    def test_logout(self):
        """Test logout functionality"""
        # First login
        login_response = requests.post(f"{BASE_URL}/api/login", json={
            "username": ADMIN_USERNAME,
            "password": ADMIN_PASSWORD
        })
        session_token = login_response.json()["sessionToken"]
        
        # Then logout
        response = requests.post(f"{BASE_URL}/api/logout", 
                               cookies={"SESSION": session_token})
        assert response.status_code == 200
    
    def test_session_info(self):
        """Test getting current session information"""
        # Login first
        login_response = requests.post(f"{BASE_URL}/api/login", json={
            "username": ADMIN_USERNAME,
            "password": ADMIN_PASSWORD
        })
        session_token = login_response.json()["sessionToken"]
        
        # Get session info
        response = requests.get(f"{BASE_URL}/api/session",
                               cookies={"SESSION": session_token})
        assert response.status_code == 200
        data = response.json()
        assert "username" in data
        assert "role" in data

class TestSystem:
    """System management tests"""
    
    @pytest.fixture
    def auth_token(self):
        """Fixture to get authentication token"""
        response = requests.post(f"{BASE_URL}/api/login", json={
            "username": ADMIN_USERNAME,
            "password": ADMIN_PASSWORD
        })
        return response.json()["sessionToken"]
    
    def test_system_info(self, auth_token):
        """Test getting system information"""
        response = requests.get(f"{BASE_URL}/api/system/info",
                               cookies={"SESSION": auth_token})
        assert response.status_code == 200
        data = response.json()
        assert "hostname" in data
        assert "version" in data
        assert "model" in data
        assert "uptime" in data
    
    def test_system_status(self, auth_token):
        """Test getting system status"""
        response = requests.get(f"{BASE_URL}/api/system/status",
                               cookies={"SESSION": auth_token})
        assert response.status_code == 200
        data = response.json()
        assert "health" in data
        assert "temperature" in data
        assert "power" in data
    
    def test_system_sessions(self, auth_token):
        """Test getting active sessions"""
        response = requests.get(f"{BASE_URL}/api/system/sessions",
                               cookies={"SESSION": auth_token})
        assert response.status_code == 200
        data = response.json()
        assert "active_sessions" in data

class TestHardware:
    """Hardware monitoring tests"""
    
    @pytest.fixture
    def auth_token(self):
        """Fixture to get authentication token"""
        response = requests.post(f"{BASE_URL}/api/login", json={
            "username": ADMIN_USERNAME,
            "password": ADMIN_PASSWORD
        })
        return response.json()["sessionToken"]
    
    def test_temperature(self, auth_token):
        """Test temperature readings"""
        response = requests.get(f"{BASE_URL}/api/hwmon/temperature",
                               cookies={"SESSION": auth_token})
        assert response.status_code == 200
    
    def test_power(self, auth_token):
        """Test power consumption data"""
        response = requests.get(f"{BASE_URL}/api/hwmon/power",
                               cookies={"SESSION": auth_token})
        assert response.status_code == 200
    
    def test_fans(self, auth_token):
        """Test fan status"""
        response = requests.get(f"{BASE_URL}/api/hwmon/fans",
                               cookies={"SESSION": auth_token})
        assert response.status_code == 200
    
    def test_voltage(self, auth_token):
        """Test voltage readings"""
        response = requests.get(f"{BASE_URL}/api/hwmon/voltage",
                               cookies={"SESSION": auth_token})
        assert response.status_code == 200

class TestConfig:
    """Configuration management tests"""
    
    @pytest.fixture
    def auth_token(self):
        """Fixture to get authentication token"""
        response = requests.post(f"{BASE_URL}/api/login", json={
            "username": ADMIN_USERNAME,
            "password": ADMIN_PASSWORD
        })
        return response.json()["sessionToken"]
    
    def test_get_config(self, auth_token):
        """Test getting current configuration"""
        response = requests.get(f"{BASE_URL}/api/config",
                               cookies={"SESSION": auth_token})
        assert response.status_code == 200
        data = response.json()
        assert "server" in data
        assert "hardware" in data
        assert "security" in data
        assert "streaming" in data
    
    def test_update_config(self, auth_token):
        """Test updating configuration"""
        update_data = {
            "server": {
                "log_level": "debug"
            }
        }
        response = requests.put(f"{BASE_URL}/api/config",
                                json=update_data,
                                cookies={"SESSION": auth_token})
        assert response.status_code == 200
    
    def test_get_network_config(self, auth_token):
        """Test getting network configuration"""
        response = requests.get(f"{BASE_URL}/api/config/network",
                               cookies={"SESSION": auth_token})
        assert response.status_code == 200
    
    def test_get_hardware_config(self, auth_token):
        """Test getting hardware configuration"""
        response = requests.get(f"{BASE_URL}/api/config/hardware",
                               cookies={"SESSION": auth_token})
        assert response.status_code == 200
    
    def test_get_security_config(self, auth_token):
        """Test getting security configuration"""
        response = requests.get(f"{BASE_URL}/api/config/security",
                               cookies={"SESSION": auth_token})
        assert response.status_code == 200

class TestUsers:
    """User management tests"""
    
    @pytest.fixture
    def admin_token(self):
        """Fixture to get admin authentication token"""
        response = requests.post(f"{BASE_URL}/api/login", json={
            "username": ADMIN_USERNAME,
            "password": ADMIN_PASSWORD
        })
        return response.json()["sessionToken"]
    
    def test_list_users(self, admin_token):
        """Test listing all users (admin only)"""
        response = requests.get(f"{BASE_URL}/api/users",
                               cookies={"SESSION": admin_token})
        assert response.status_code == 200
        data = response.json()
        assert isinstance(data, list)
    
    def test_create_user(self, admin_token):
        """Test creating a new user (admin only)"""
        user_data = {
            "username": "testuser_api",
            "password": "testpass123",
            "role": "user",
            "email": "test@example.com"
        }
        response = requests.post(f"{BASE_URL}/api/users",
                                json=user_data,
                                cookies={"SESSION": admin_token})
        assert response.status_code in [201, 409]  # 409 if user already exists
    
    def test_get_user(self, admin_token):
        """Test getting specific user info"""
        response = requests.get(f"{BASE_URL}/api/users/admin",
                               cookies={"SESSION": admin_token})
        assert response.status_code == 200
        data = response.json()
        assert "username" in data
        assert "role" in data
    
    def test_update_user(self, admin_token):
        """Test updating user information"""
        update_data = {
            "email": "updated@example.com",
            "role": "user"
        }
        response = requests.put(f"{BASE_URL}/api/users/testuser_api",
                               json=update_data,
                               cookies={"SESSION": admin_token})
        assert response.status_code in [200, 404]  # 404 if user doesn't exist
    
    def test_change_password(self, admin_token):
        """Test changing user password"""
        password_data = {
            "oldPassword": "testpass123",
            "newPassword": "newpass123"
        }
        response = requests.post(f"{BASE_URL}/api/users/testuser_api/password",
                                json=password_data,
                                cookies={"SESSION": admin_token})
        assert response.status_code in [200, 400, 404]  # Various valid responses
    
    def test_enable_user(self, admin_token):
        """Test enabling/disabling user"""
        enable_data = {
            "enabled": True
        }
        response = requests.put(f"{BASE_URL}/api/users/testuser_api/enable",
                               json=enable_data,
                               cookies={"SESSION": admin_token})
        assert response.status_code in [200, 404]

class TestStreaming:
    """Streaming management tests"""
    
    @pytest.fixture
    def auth_token(self):
        """Fixture to get authentication token"""
        response = requests.post(f"{BASE_URL}/api/login", json={
            "username": ADMIN_USERNAME,
            "password": ADMIN_PASSWORD
        })
        return response.json()["sessionToken"]
    
    def test_list_streams(self, auth_token):
        """Test listing all available streams"""
        pytest.skip("Streaming endpoints require video frame capture - skipped for API testing")
        response = requests.get(f"{BASE_URL}/api/streams",
                               cookies={"SESSION": auth_token}, timeout=3)
        assert response.status_code == 200
        data = response.json()
        assert isinstance(data, list)
    
    def test_detect_cameras(self, auth_token):
        """Test auto-detecting cameras"""
        pytest.skip("Streaming endpoints require video frame capture - skipped for API testing")
        response = requests.get(f"{BASE_URL}/api/streams/detect",
                               cookies={"SESSION": auth_token}, timeout=3)
        assert response.status_code == 200
        data = response.json()
        assert isinstance(data, list)
    
    def test_start_stream(self, auth_token):
        """Test starting a stream"""
        pytest.skip("Streaming endpoints require video frame capture - skipped for API testing")
        response = requests.post(f"{BASE_URL}/api/streams/video_source/start",
                                cookies={"SESSION": auth_token}, timeout=3)
        # May fail if stream doesn't exist or already running
        assert response.status_code in [200, 400, 404]
    
    def test_stop_stream(self, auth_token):
        """Test stopping a stream"""
        pytest.skip("Streaming endpoints require video frame capture - skipped for API testing")
        response = requests.post(f"{BASE_URL}/api/streams/video_source/stop",
                                cookies={"SESSION": auth_token}, timeout=3)
        # May fail if stream doesn't exist or not running
        assert response.status_code in [200, 400, 404]
    
    def test_stream_status(self, auth_token):
        """Test getting stream status"""
        pytest.skip("Streaming endpoints require video frame capture - skipped for API testing")
        response = requests.get(f"{BASE_URL}/api/streams/video_source/status",
                               cookies={"SESSION": auth_token}, timeout=3)
        # May fail if stream doesn't exist
        assert response.status_code in [200, 404]
    
    def test_stream_statistics(self, auth_token):
        """Test getting stream statistics"""
        pytest.skip("Streaming endpoints require video frame capture - skipped for API testing")
        response = requests.get(f"{BASE_URL}/api/streams/video_source/statistics",
                               cookies={"SESSION": auth_token}, timeout=3)
        # May fail if stream doesn't exist
        assert response.status_code in [200, 404]
    
    def test_all_stream_statistics(self, auth_token):
        """Test getting statistics for all streams"""
        pytest.skip("Streaming endpoints require video frame capture - skipped for API testing")
        response = requests.get(f"{BASE_URL}/api/streams/statistics",
                               cookies={"SESSION": auth_token}, timeout=3)
        assert response.status_code == 200
    
    def test_stream_thumbnail(self, auth_token):
        """Test getting stream thumbnail"""
        pytest.skip("Streaming endpoints require video frame capture - skipped for API testing")
        response = requests.get(f"{BASE_URL}/api/streams/video_source/thumbnail",
                               cookies={"SESSION": auth_token}, timeout=3)
        # May fail if stream doesn't exist or no video data
        assert response.status_code in [200, 404, 500]

class TestRecordings:
    """Recording management tests"""
    
    @pytest.fixture
    def auth_token(self):
        """Fixture to get authentication token"""
        response = requests.post(f"{BASE_URL}/api/login", json={
            "username": ADMIN_USERNAME,
            "password": ADMIN_PASSWORD
        })
        return response.json()["sessionToken"]
    
    def test_list_recordings(self, auth_token):
        """Test listing all recordings"""
        pytest.skip("Recording endpoints require video frame capture - skipped for API testing")
        response = requests.get(f"{BASE_URL}/api/recordings",
                               cookies={"SESSION": auth_token}, timeout=3)
        assert response.status_code == 200
        data = response.json()
        assert isinstance(data, list) or isinstance(data, dict)

class TestStaticFiles:
    """Static file serving tests"""
    
    def test_index_html(self):
        """Test serving index.html"""
        response = requests.get(f"{BASE_URL}/index.html", timeout=3)
        assert response.status_code == 200
        assert "text/html" in response.headers.get("content-type", "")
    
    def test_css_asset(self):
        """Test serving CSS asset"""
        response = requests.get(f"{BASE_URL}/assets/index-Du7Sho3W.css", timeout=3)
        assert response.status_code == 200
        assert "text/css" in response.headers.get("content-type", "")
    
    def test_js_asset(self):
        """Test serving JavaScript asset"""
        response = requests.get(f"{BASE_URL}/assets/index-9a_1CVPS.js", timeout=3)
        assert response.status_code == 200
        assert "javascript" in response.headers.get("content-type", "")
    
    def test_root_route(self):
        """Test root route fallback"""
        response = requests.get(f"{BASE_URL}/", timeout=3)
        assert response.status_code == 200
        assert "text/html" in response.headers.get("content-type", "")

class TestAPIConnectivity:
    """Basic API connectivity tests"""
    
    def test_server_reachable(self):
        """Test if the API server is reachable"""
        try:
            response = requests.get(f"{BASE_URL}/api/system/info", timeout=5)
            # Server is reachable even if auth fails
            assert response.status_code in [200, 401, 403]
        except requests.exceptions.ConnectionError:
            pytest.skip("Server not reachable - start the backend first")
    
    def test_cors_headers(self):
        """Test CORS headers are present"""
        response = requests.options(f"{BASE_URL}/api/login", timeout=3)
        assert "Access-Control-Allow-Origin" in response.headers

# Pytest configuration
def pytest_configure(config):
    """Pytest configuration"""
    config.addinivalue_line(
        "markers", "slow: marks tests as slow (deselect with '-m \"not slow\"')"
    )
    config.addinivalue_line(
        "markers", "integration: marks tests as integration tests"
    )

if __name__ == "__main__":
    # Run tests when executed directly
    pytest.main([__file__, "-v", "--tb=short"])
