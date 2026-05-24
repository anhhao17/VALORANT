# API Test Script

This directory contains a test script for testing the Jetson BMCweb API endpoints.

## Prerequisites

- Python 3.6+
- `requests` library: `pip install requests`

## Usage

### Starting the Server

First, start the Jetson BMCweb server:

```bash
cd /home/hao/app/jetson
./build/jetson
```

The server will start on `http://localhost:8080`.

### Running Tests

```bash
./scripts/test-api.py
```

Or with Python directly:
```bash
python3 scripts/test-api.py
```

## Test Coverage

The test script covers the following endpoints:

### Authentication Tests
- Test endpoint without authentication (should return 401)
- Test endpoint with valid authentication (should return 200)

### System Endpoints
- `GET /api/system/info` - System information
- `GET /api/system/status` - System status
- `POST /api/system/reboot` - System reboot

### Hardware Monitoring Endpoints
- `GET /api/hwmon/temperature` - Temperature sensors
- `GET /api/hwmon/power` - Power sensors
- `GET /api/hwmon/fans` - Fan speeds
- `GET /api/hwmon/voltage` - Voltage sensors

### Error Cases
- Non-existent endpoint (should return 404)
- Invalid authentication (should return 401)

## Configuration

The script uses the following default configuration:

- **Host**: `localhost`
- **Port**: `8080`
- **Base URL**: `http://localhost:8080`
- **Authentication**: Basic auth with `admin:password`

To modify the configuration, edit the variables at the top of the script:

```python
HOST = "localhost"
PORT = 8080
BASE_URL = f"http://{HOST}:{PORT}"
AUTH = ("admin", "password")
```

## Expected Output

A successful test run will show:

```
==========================================
Jetson BMCweb API Test Suite
==========================================
Testing endpoints on: http://localhost:8080

Waiting for server to be ready...
Server is ready!

Testing authentication...
✓ PASS: Test endpoint without auth
✓ PASS: Test endpoint with auth

Testing system endpoints...
✓ PASS: System info
✓ PASS: System status
✓ PASS: System reboot

Testing hardware monitoring endpoints...
✓ PASS: Temperature sensors
✓ PASS: Power sensors
✓ PASS: Fan speeds
✓ PASS: Voltage sensors

Testing error cases...
✓ PASS: Non-existent endpoint
✓ PASS: Invalid auth header

==========================================
Test Summary
==========================================
Total tests: 11
Passed: 11
Failed: 0

All tests passed!
```

## Adding New Tests

Add new test cases using the helper functions:

```python
# Simple endpoint test
test_endpoint("Test name", "/api/endpoint", 200, use_auth=True)

# JSON validation test
test_endpoint_with_json_validation(
    "Test name", "/api/endpoint", 200, "required_field", use_auth=True
)
```

## Troubleshooting

### Server Not Ready
If the test script reports that the server is not ready:
1. Ensure the server is running: `./build/jetson`
2. Check that the port (8080) is not in use by another application
3. Verify the server is listening on the correct interface

### Connection Refused
If you get connection errors:
1. Check that the server is actually running
2. Verify the HOST and PORT configuration in the test script
3. Check firewall settings if testing on a remote host

### Authentication Failures
If authentication tests fail:
1. Verify the credentials in the test script match the server configuration
2. Check that the authentication middleware is properly configured
3. Ensure the AUTH_HEADER (bash) or AUTH tuple (python) is correctly set

## Continuous Integration

These test scripts can be integrated into CI/CD pipelines:

```bash
# Start server in background
./build/jetson &
SERVER_PID=$!

# Wait for server to start
sleep 5

# Run tests
./scripts/test-api.sh
TEST_RESULT=$?

# Stop server
kill $SERVER_PID

# Exit with test result
exit $TEST_RESULT
```
