# Jetson BMCweb - Minimal Implementation

A minimal bmcweb-style web server implementation for Jetson embedded platforms, following proven patterns from the OpenBMC project.

## Overview

This project implements a minimal version of the bmcweb architecture, providing:
- Trie-based routing system
- HTTP server with Boost.Beast
- Middleware chain system (CORS, authentication)
- Comprehensive logging with spdlog
- API endpoints for system and hardware monitoring
- Async response handling
- HTTP request/response wrappers
- Clean, proven architecture patterns

## Current Status

**Phase 1: Foundation** ✅
- Basic project structure
- Async response handling
- HTTP request/response wrappers
- Build system configuration

**Phase 2: Core Implementation** ✅
- Trie-based routing system
- HTTP server with Boost.Beast
- Middleware chain system
- Authentication middleware
- CORS middleware
- Comprehensive logging with spdlog
- API endpoints (system info, hardware monitoring)

**Next Steps:**
- Add Vue 3 UI
- Implement streaming functionality
- Add comprehensive hardware monitoring integration
- Implement JWT-based authentication
- Add WebSocket support

## Building

```bash
./scripts/build.sh
```

Or manually:

```bash
mkdir build && cd build
cmake ..
make
```

## Running

```bash
./build/jetson
```

The server will start on `http://localhost:8080` with the following default credentials:
- Username: `admin`
- Password: `password`

The application will create a log file `jetson.log` in the current directory with detailed logging information.

## Testing

API endpoints can be tested using the provided test script:

```bash
./scripts/test-api.py
```

See `scripts/TEST_API_README.md` for detailed testing instructions.

## API Endpoints

### System Endpoints
- `GET /api/system/info` - System information
- `GET /api/system/status` - System status
- `POST /api/system/reboot` - System reboot

### Hardware Monitoring Endpoints
- `GET /api/hwmon/temperature` - Temperature sensors
- `GET /api/hwmon/power` - Power sensors
- `GET /api/hwmon/fans` - Fan speeds
- `GET /api/hwmon/voltage` - Voltage sensors

### Test Endpoint
- `GET /api/test` - Test endpoint

## Architecture

Following bmcweb patterns:
- **App**: Main application class with route registration
- **Router**: Trie-based URL matching
- **Middleware**: Chain-based middleware system
- **Logging**: Comprehensive logging with spdlog (console + file)
- **AsyncResp**: Async response handling
- **Request/Response**: HTTP wrappers around Boost.Beast
- **Server**: Multi-threaded HTTP server with Boost.Beast

## Project Structure

```
src/
├── bmcweb/
│   ├── app.hpp              # Main application class
│   ├── async_resp.hpp       # Async response handling
│   ├── logging.hpp          # Logging system interface
│   ├── logging.cpp          # Logging system implementation
│   ├── server.hpp           # HTTP server implementation
│   ├── http/
│   │   ├── types.hpp        # HTTP type aliases
│   │   ├── request.hpp      # Request wrapper
│   │   └── response.hpp     # Response wrapper
│   ├── routing/
│   │   ├── router.hpp       # Main router
│   │   ├── baserule.hpp     # Base rule class
│   │   ├── taggedrule.hpp   # Tagged rule for parameters
│   │   └── trie.hpp         # Trie data structure
│   ├── middleware/
│   │   ├── middleware.hpp   # Middleware chain
│   │   ├── cors.hpp         # CORS middleware
│   │   └── auth.hpp         # Authentication middleware
│   └── routes/
│       ├── system.hpp       # System API routes
│       └── hwmon.hpp        # Hardware monitoring routes
└── core/
    └── main.cpp             # Application entry
```

## Dependencies

- C++20
- Boost 1.83+ (system, filesystem, beast, asio)
- nlohmann/json 3.11+
- spdlog 1.10+ (logging)
- CMake 3.15+
- pthread (threading)

## License

TBD