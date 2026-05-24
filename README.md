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
- WebSocket support for real-time sensor streaming
- SSL/TLS support for secure connections (HTTPS/WSS)
- Session-based authentication with cookies
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

**Phase 3: WebSocket & Security** ✅
- WebSocket support for real-time sensor streaming
- WebSocket upgrade on HTTP (same port)
- SSL/TLS support for HTTPS/WSS
- WebSocket security validation (headers, protocol, token)
- Session-based authentication with cookies
- Integration test suite

**Next Steps:**
- Add comprehensive hardware monitoring integration
- Implement JWT-based authentication
- Add more WebSocket data streaming features

## Building

```bash
# Build backend
./scripts/build.sh

# Build frontend
cd webui
npm install
npm run build
```

## Running

```bash
# Terminal 1: Start backend (HTTP)
./build/jetson

# Terminal 1: Start backend (HTTPS with SSL)
./build/jetson --ssl --cert cert.pem --key key.pem

# Terminal 1: Start backend on custom port
./build/jetson --port 9000

# Terminal 2: Start frontend (development mode)
cd webui
npm run dev
```

The backend server will start on `http://localhost:8080` (or `https://localhost:8443` with SSL) with the following default credentials:
- Username: `admin`
- Password: `password`

### SSL/TLS Configuration

To enable SSL/TLS for secure connections:
1. Generate SSL certificates:
```bash
openssl req -x509 -newkey rsa:4096 -keyout key.pem -out cert.pem -days 365 -nodes -subj "/CN=localhost"
```

2. Start the server with SSL:
```bash
./build/jetson --ssl --cert cert.pem --key key.pem
```

### Command Line Options

- `--ssl, -s` - Enable SSL/TLS (default port: 8443)
- `--cert <file>` - SSL certificate file path
- `--key <file>` - SSL private key file path
- `--port <port>` - Server port (default: 8080, 8443 with SSL)
- `--help, -h` - Show help message

The frontend development server will start on `http://localhost:5173` with hot reload enabled.

The application will create a log file `jetson.log` in the current directory with detailed logging information.

## Testing

The project includes both unit tests and integration tests:

### Unit Tests (C++)
```bash
# Build with tests enabled
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON
cmake --build build

# Run unit tests
cd build
ctest --output-on-failure
```

### Integration Tests (Python)
```bash
# Start the server
./build/jetson

# Run integration tests in another terminal
./tests/integration/test-api.py
./tests/integration/test_auth.py
./tests/integration/test_websocket.py
```

See `tests/integration/README.md` for detailed testing instructions.

## API Endpoints

### Authentication Endpoints
- `POST /api/login` - User login with session creation
- `POST /api/logout` - User logout with session cleanup
- `GET /api/session` - Get current session information

### System Endpoints
- `GET /api/system/info` - System information
- `GET /api/system/status` - System status
- `POST /api/system/reboot` - System reboot

### Hardware Monitoring Endpoints
- `GET /api/hwmon/temperature` - Temperature sensors
- `GET /api/hwmon/power` - Power sensors
- `GET /api/hwmon/fans` - Fan speeds
- `GET /api/hwmon/voltage` - Voltage sensors

### WebSocket Endpoints
- `GET /ws` - WebSocket endpoint for real-time sensor streaming
  - Requires WebSocket upgrade headers
  - Supports protocol negotiation (view types, tokens)
  - Validates authentication before connection

## Architecture

Following bmcweb patterns:
- **App**: Main application class with route registration
- **Router**: Trie-based URL matching
- **Middleware**: Chain-based middleware system
- **Logging**: Comprehensive logging with spdlog (console + file)
- **AsyncResp**: Async response handling
- **Request/Response**: HTTP wrappers around Boost.Beast
- **Server**: Multi-threaded HTTP server with Boost.Beast
- **WebSocket**: Real-time sensor streaming with security validation
- **Session**: Session-based authentication with cookie management
- **SSL/TLS**: Secure connections support (HTTPS/WSS)

## Project Structure

```
src/
├── bmcweb/
│   ├── app.hpp              # Main application class
│   ├── async_resp.hpp       # Async response handling
│   ├── logging.hpp          # Logging system interface
│   ├── logging.cpp          # Logging system implementation
│   ├── server.hpp           # HTTP server implementation
│   ├── server.cpp           # Server implementation with SSL/WebSocket
│   ├── session.hpp          # Session management
│   ├── session.cpp          # Session implementation
│   ├── websocket.hpp        # WebSocket implementation
│   ├── websocket.cpp        # WebSocket implementation
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
│       ├── system.cpp       # System implementation
│       ├── hwmon.hpp        # Hardware monitoring routes
│       ├── hwmon.cpp        # Hardware monitoring implementation
│       ├── auth.hpp         # Authentication routes
│       ├── auth.cpp         # Authentication implementation
│       └── websocket.hpp    # WebSocket routes
└── core/
    └── main.cpp             # Application entry with SSL options

tests/
├── integration/             # Python integration tests
│   ├── test-api.py         # API endpoint tests
│   ├── test_auth.py        # Authentication tests
│   ├── test_websocket.py   # WebSocket tests
│   └── README.md           # Integration test documentation
├── CMakeLists.txt          # C++ test build configuration
├── test_routing.cpp        # C++ unit tests
├── test_http_wrappers.cpp  # C++ unit tests
├── test_middleware.cpp     # C++ unit tests
└── test_api_endpoints.cpp  # C++ unit tests
```

## Dependencies

- C++20
- Boost 1.83+ (system, filesystem, beast, asio, iostreams)
- OpenSSL (SSL/TLS support)
- nlohmann/json 3.11+
- spdlog 1.10+ (logging)
- CMake 3.15+
- pthread (threading)
- Python 3.6+ (for integration tests)
- requests (Python library for testing)
- websockets (Python library for testing)

## CI/CD

The project includes GitHub Actions CI pipeline that:
- Builds the application on Ubuntu
- Runs C++ unit tests via CTest
- Runs Python integration tests
- Builds Vue UI
- Creates deployment packages
- Runs on push to main/develop branches and pull requests

## Security Features

- **SSL/TLS Support**: Secure connections for HTTPS/WSS
- **Session-based Authentication**: Cookie-based session management
- **WebSocket Security**: 
  - Header validation (Upgrade, Connection, Sec-WebSocket-Key, Sec-WebSocket-Version)
  - Protocol validation (view types, token requirements)
  - Token-based authentication before connection
- **CORS Middleware**: Configurable cross-origin resource sharing
- **Authentication Middleware**: Route-level access control

## License

TBD