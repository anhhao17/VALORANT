# Build Scripts

This directory contains build and test scripts for the Jetson BMCweb project.

## Available Scripts

### `build.sh` - Build the Application
Builds the Jetson BMCweb C++ application.

```bash
# Build in Release mode (default)
./scripts/build.sh

# Build in Debug mode
./scripts/build.sh Debug
```

**What it does:**
1. Creates build directory if needed
2. Runs CMake configuration
3. Builds the C++ application using CMake
4. Outputs binary to `build/jetson`

### `test-api.py` - Test API Endpoints
Tests all API endpoints to verify functionality.

```bash
./scripts/test-api.py
```

**What it does:**
1. Waits for server to be ready
2. Tests authentication (with and without credentials)
3. Tests system endpoints (info, status, reboot)
4. Tests hardware monitoring endpoints (temperature, power, fans, voltage)
5. Tests error cases (404, 401)
6. Reports test results with pass/fail counts

See `TEST_API_README.md` for detailed testing instructions.

## Running the Application

After building:

```bash
# Run the application
./build/jetson
```

The server will start on `http://localhost:8080` with default credentials:
- Username: `admin`
- Password: `password`

## Development Workflow

### Build and Run
```bash
# Build the application
./scripts/build.sh

# Run the application
./build/jetson

# In another terminal, test the API
./scripts/test-api.py
```

### Debug Build
```bash
# Build in debug mode
./scripts/build.sh Debug

# Run the debug binary
./build/jetson
```

## Requirements

- CMake 3.15+
- C++20 compatible compiler
- Boost 1.83+ (system, filesystem, beast, asio)
- nlohmann/json 3.11+
- Python 3.6+ (for testing)
- requests library (for testing): `pip install requests`
