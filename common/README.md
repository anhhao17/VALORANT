# Common Test Scripts

This directory contains common test scripts for the Jetson BMCweb project.

## Available Test Scripts

### `test-api.py` - Test API Endpoints
Tests all API endpoints to verify functionality.

```bash
./common/test-api.py
```

**What it does:**
1. Waits for server to be ready
2. Tests authentication (with and without credentials)
3. Tests system endpoints (info, status, reboot)
4. Tests hardware monitoring endpoints (temperature, power, fans, voltage)
5. Tests error cases (404, 401)
6. Reports test results with pass/fail counts

See `TEST_API_README.md` for detailed testing instructions.

### `test_auth.py` - Test Authentication
Tests authentication endpoints and session management.

```bash
./common/test_auth.py
```

**What it does:**
1. Tests login endpoint with valid credentials
2. Tests login endpoint with invalid credentials
3. Tests session token validation
4. Tests logout functionality
5. Tests protected endpoints with and without authentication

### `test_websocket.py` - Test WebSocket
Tests WebSocket connections and real-time data streaming.

```bash
./common/test_websocket.py
```

**What it does:**
1. Tests WebSocket connection establishment
2. Tests real-time sensor data streaming
3. Tests WebSocket message handling
4. Tests connection cleanup

## Requirements

- Python 3.6+
- requests library: `pip install requests`
- websockets library: `pip install websockets`

## Running Tests

Make sure the server is running before executing test scripts:

```bash
# Start the server
./build/jetson

# In another terminal, run tests
./common/test-api.py
./common/test_auth.py
./common/test_websocket.py
```

## Test Results

All test scripts provide detailed output showing:
- Test execution progress
- Pass/fail status for each test
- Summary statistics
- Error details for failed tests