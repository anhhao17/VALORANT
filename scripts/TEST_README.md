# API Tests for Jetson BMCweb

This directory contains comprehensive API tests for the Jetson BMCweb system using Python pytest and requests.

## Test Structure

### Main Test File: `api_tests.py`

The test suite is organized into the following test classes:

1. **TestAuth** - Authentication and session management
   - Login/logout functionality
   - Session validation
   - Token management

2. **TestSystem** - System management
   - System information
   - System status
   - Active sessions

3. **TestHardware** - Hardware monitoring
   - Temperature readings
   - Power consumption
   - Fan status
   - Voltage readings

4. **TestConfig** - Configuration management
   - Get/update configuration
   - Network, hardware, security config
   - Stream configuration management

5. **TestUsers** - User management
   - CRUD operations for users
   - Password management
   - Role-based access control

6. **TestStreaming** - Streaming management
   - Stream listing and detection
   - Start/stop streaming
   - Stream statistics and status
   - Thumbnail generation

7. **TestRecordings** - Recording management
   - Recording listing and management

8. **TestStaticFiles** - Static file serving
   - Frontend asset serving
   - Route fallbacks

9. **TestAPIConnectivity** - Basic connectivity
   - Server reachability
   - CORS headers

## Requirements

Install Python dependencies:

```bash
pip3 install -r requirements.txt
```

Required packages:
- `requests>=2.31.0` - HTTP client library
- `pytest>=7.4.0` - Testing framework
- `pytest-asyncio>=0.21.0` - Async testing support

## Running Tests

### Quick Start

The easiest way to run tests is using the provided script:

```bash
./run_api_tests.sh
```

This script will:
1. Check if the backend is running
2. Start the backend if needed
3. Install Python dependencies if needed
4. Run all API tests
5. Clean up background processes

### Manual Testing

1. Start the backend:
```bash
cd /home/hao/app/jetson
./build/jetson --config config.yml
```

2. In another terminal, run tests:
```bash
cd /home/hao/app/jetson/tests
python3 -m pytest api_tests.py -v
```

### Test Options

- **Verbose output**: `python3 -m pytest api_tests.py -v`
- **Very verbose**: `python3 -m pytest api_tests.py -vv`
- **Show local variables**: `python3 -m pytest api_tests.py -l`
- **Stop on first failure**: `python3 -m pytest api_tests.py -x`
- **Run specific test class**: `python3 -m pytest api_tests.py::TestAuth -v`
- **Run specific test**: `python3 -m pytest api_tests.py::TestAuth::test_login_success -v`

### Test Markers

Tests can be marked with custom markers:
- `@pytest.mark.slow` - Slow tests (can be skipped with `-m "not slow"`)
- `@pytest.mark.integration` - Integration tests

Example:
```bash
# Skip slow tests
python3 -m pytest api_tests.py -m "not slow" -v

# Run only integration tests
python3 -m pytest api_tests.py -m integration -v
```

## Test Configuration

### Backend URL

By default, tests connect to `http://localhost:8080`. To change this:

1. Edit `api_tests.py`
2. Change the `BASE_URL` variable at the top of the file

### Test Credentials

Default test credentials:
- Admin: `admin` / `admin`
- Test user: `testuser` / `testpass`

These can be modified in the test class constants.

## Test Coverage

The test suite covers all documented API endpoints:

- **41+ API endpoints** across 8 categories
- **Authentication flows** (login, logout, session management)
- **Configuration management** (including new YAML config API)
- **User management** (CRUD operations with role-based access)
- **Streaming operations** (start, stop, status, statistics)
- **Hardware monitoring** (temperature, power, fans, voltage)
- **System management** (info, status, reboot)
- **Recording management** (list, stop, info)
- **Static file serving** (frontend assets)

## Expected Test Results

When the backend is functioning correctly:

- **Authentication tests**: Should pass (admin/admin should work)
- **System tests**: Should pass (system info/status should be available)
- **Hardware tests**: Should pass (sensor data should be available)
- **Config tests**: Should pass (new YAML config API should work)
- **User tests**: Should pass (user management should work)
- **Streaming tests**: May have mixed results depending on stream configuration
- **Static file tests**: Should pass (frontend assets should be served)

## Troubleshooting

### Connection Refused

If tests fail with connection errors:
1. Ensure the backend is running: `./build/jetson --config config.yml`
2. Check the port: Default is 8080
3. Check firewall settings

### Authentication Failures

If auth tests fail:
1. Verify default admin credentials are set up
2. Check user database: `~/.config/jetson/users.json`
3. Review backend logs for authentication errors

### Streaming Test Failures

If streaming tests fail:
1. Ensure streams are configured in `config.yml`
2. Check that video files exist at configured paths
3. Verify FFmpeg is properly installed
4. Review backend logs for streaming initialization errors

### Import Errors

If Python import errors occur:
1. Install dependencies: `pip3 install -r requirements.txt`
2. Check Python version: Requires Python 3.7+
3. Verify virtual environment if using one

## Continuous Integration

These tests can be integrated into CI/CD pipelines:

```yaml
# Example GitHub Actions workflow
- name: Start Backend
  run: ./build/jetson --config config.yml &
  
- name: Install Dependencies
  run: pip3 install -r tests/requirements.txt
  
- name: Run API Tests
  run: cd tests && python3 -m pytest api_tests.py -v
```

## Adding New Tests

To add tests for new endpoints:

1. Add a new test method to the appropriate test class
2. Use the `auth_token` fixture for authenticated endpoints
3. Follow the existing pattern for request/response validation
4. Add appropriate assertions for the expected behavior

Example:
```python
def test_new_endpoint(self, auth_token):
    """Test new API endpoint"""
    response = requests.get(f"{BASE_URL}/api/new/endpoint",
                           cookies={"SESSION": auth_token})
    assert response.status_code == 200
    data = response.json()
    assert "expected_field" in data
```

## Test Data Cleanup

The tests use a test user (`testuser_api`) that may be created during testing. To clean up:

```bash
# Manually delete test user via API
curl -X DELETE http://localhost:8080/api/users/testuser_api \
  -H "Cookie: SESSION=<your_session_token>"
```

Or restart the backend with a fresh user database.

## Contributing

When adding new API endpoints:
1. Update the API documentation in `docs/API_ENDPOINTS.md`
2. Add corresponding tests in `api_tests.py`
3. Ensure tests cover both success and failure cases
4. Update this README if new test categories are added
