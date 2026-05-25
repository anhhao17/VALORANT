# Jetson BMCweb API Endpoints

Complete list of all HTTP API endpoints in the Jetson BMCweb system.

## Authentication & Session Management

### POST /api/login
- **Description**: User login with username/password
- **Request Body**: `{"username": "string", "password": "string"}`
- **Response**: `{"sessionToken": "string", "csrfToken": "string", "username": "string", "role": "string"}`
- **Headers**: Sets SESSION cookie

### POST /api/logout
- **Description**: User logout
- **Response**: `{"message": "Logged out successfully"}`
- **Headers**: Clears SESSION cookie

### GET /api/session
- **Description**: Get current session information
- **Response**: `{"username": "string", "uniqueId": "string", "role": "string"}`
- **Auth**: Requires valid session token

## System Management

### GET /api/system/info
- **Description**: Get system information
- **Response**: `{"hostname": "string", "version": "string", "model": "string", "uptime": "number"}`

### GET /api/system/status
- **Description**: Get system status
- **Response**: `{"health": "string", "temperature": "number", "power": "string", "cpu_usage": "number", "active_sessions": "number"}`

### POST /api/system/reboot
- **Description**: Initiate system reboot
- **Response**: `{"message": "System reboot initiated", "status": "rebooting"}`

### GET /api/system/sessions
- **Description**: Get active session count
- **Response**: `{"active_sessions": "number", "message": "string"}`

## Hardware Monitoring

### GET /api/hwmon/temperature
- **Description**: Get temperature readings
- **Response**: Temperature sensor data

### GET /api/hwmon/power
- **Description**: Get power consumption data
- **Response**: Power sensor data

### GET /api/hwmon/fans
- **Description**: Get fan status
- **Response**: Fan sensor data

### GET /api/hwmon/voltage
- **Description**: Get voltage readings
- **Response**: Voltage sensor data

## Configuration Management

### GET /api/config
- **Description**: Get current system configuration (NEW)
- **Response**: Complete configuration object with server, hardware, security, streaming, websocket, and network settings

### PUT /api/config
- **Description**: Update system configuration (NEW)
- **Request Body**: Configuration object with settings to update
- **Response**: `{"message": "Configuration updated successfully"}`

### GET /api/config/network
- **Description**: Get network configuration
- **Response**: Network settings

### PUT /api/config/network
- **Description**: Update network configuration
- **Request Body**: Network settings

### GET /api/config/system
- **Description**: Get system configuration
- **Response**: System settings

### PUT /api/config/system
- **Description**: Update system configuration
- **Request Body**: System settings

### GET /api/config/hardware
- **Description**: Get hardware configuration
- **Response**: Hardware settings

### PUT /api/config/hardware
- **Description**: Update hardware configuration
- **Request Body**: Hardware settings

### GET /api/config/security
- **Description**: Get security configuration
- **Response**: Security settings

### PUT /api/config/security
- **Description**: Update security configuration
- **Request Body**: Security settings



## User Management

### GET /api/users
- **Description**: List all users (admin only)
- **Response**: Array of user objects with username, role, email, enabled, createdAt, lastLoginAt
- **Auth**: Admin required

### POST /api/users
- **Description**: Create new user (admin only)
- **Request Body**: `{"username": "string", "password": "string", "role": "string", "email": "string"}`
- **Response**: `{"message": "User created successfully", "username": "string", "role": "string"}`
- **Auth**: Admin required

### GET /api/users/{username}
- **Description**: Get specific user info (admin or self)
- **Response**: User object with username, role, email, enabled, createdAt, lastLoginAt
- **Auth**: Admin or self

### PUT /api/users/{username}
- **Description**: Update user (admin or self)
- **Request Body**: `{"email": "string", "role": "string"}`
- **Response**: `{"message": "User updated successfully"}`
- **Auth**: Admin or self

### DELETE /api/users/{username}
- **Description**: Delete user (admin only)
- **Response**: `{"message": "User deleted successfully"}`
- **Auth**: Admin required

### POST /api/users/{username}/password
- **Description**: Change password (self or admin)
- **Request Body**: `{"oldPassword": "string", "newPassword": "string"}` (admin can omit oldPassword)
- **Response**: `{"message": "Password changed successfully"}`
- **Auth**: Admin or self

### PUT /api/users/{username}/enable
- **Description**: Enable/disable user (admin only)
- **Request Body**: `{"enabled": "boolean"}`
- **Response**: `{"message": "User enabled/disabled successfully"}`
- **Auth**: Admin required

## Streaming Management

### GET /api/streams
- **Description**: List all available streams
- **Response**: Array of stream objects with id, name, type, protocol, sourcePath, enabled, loop, quality, streaming, supportsRecording

### GET /api/streams/detect
- **Description**: Auto-detect available cameras
- **Response**: Array of detected camera configurations

### POST /api/streams/{id}/start
- **Description**: Start streaming for specific stream
- **Response**: `{"message": "Stream started successfully"}`
- **Note**: Uses lazy initialization - frame source initialized in background

### POST /api/streams/{id}/stop
- **Description**: Stop streaming for specific stream
- **Response**: `{"message": "Stream stopped successfully"}`

### GET /api/streams/{id}/status
- **Description**: Get streaming status for specific stream
- **Response**: Stream status information

### GET /api/streams/{id}/statistics
- **Description**: Get streaming statistics for specific stream
- **Response**: Stream statistics (bytesServed, framesServed, clientConnections, etc.)

### POST /api/streams/{id}/statistics/reset
- **Description**: Reset streaming statistics for specific stream
- **Response**: `{"message": "Statistics reset successfully"}`

### GET /api/streams/statistics
- **Description**: Get statistics for all streams
- **Response**: Array of stream statistics

### GET /api/streams/{id}/thumbnail
- **Description**: Get thumbnail image for specific stream
- **Response**: JPEG image data
- **Content-Type**: image/jpeg

### POST /api/streams/{id}/record
- **Description**: Start recording for specific stream
- **Response**: Recording information

## Recording Management

### GET /api/recordings
- **Description**: List all recordings
- **Response**: Array of recording objects

### GET /api/recordings/{id}
- **Description**: Get specific recording information
- **Response**: Recording details

### POST /api/recordings/{id}/stop
- **Description**: Stop specific recording
- **Response**: `{"message": "Recording stopped successfully"}`

## Video Streaming

### GET /video/{id}
- **Description**: Stream video content
- **Response**: Video stream data
- **Content-Type**: Depends on video format

## Static Files

### GET /assets/index-Du7Sho3W.css
- **Description**: Frontend CSS bundle
- **Content-Type**: text/css;charset=UTF-8

### GET /assets/index-9a_1CVPS.js
- **Description**: Frontend JavaScript bundle
- **Content-Type**: application/javascript;charset=UTF-8

### GET /index.html
- **Description**: Frontend HTML entry point
- **Content-Type**: text/html;charset=UTF-8

### GET /
- **Description**: Frontend fallback route
- **Content-Type**: text/html;charset=UTF-8

## WebSocket

### /ws
- **Description**: WebSocket endpoint for real-time updates
- **Protocol**: WebSocket

## Summary

**Total Endpoints**: 39 API endpoints across 8 categories

**Authentication**: Session-based with cookie support (SESSION cookie) and Bearer token support

**Authorization**: Role-based access control (admin/user) for sensitive operations

**Configuration**: New YAML-based configuration system with runtime modification via HTTP API

**Streaming**: Multi-protocol video streaming with lazy initialization and background thread support

**User Management**: Complete CRUD operations for user accounts with role-based permissions
