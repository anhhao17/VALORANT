# Jetson BMCweb Architecture

## Overview

This document describes the architecture of the Jetson BMCweb minimal implementation, following proven patterns from the OpenBMC bmcweb project.

## System Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                     Application Layer                        │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │   App Class  │  │   Config     │  │   Lifecycle  │      │
│  │              │  │   Manager    │  │   Manager    │      │
│  └──────────────┘  └──────────────┘  └──────────────┘      │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│                      Routing Layer                          │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │    Router    │  │     Trie     │  │   Rules      │      │
│  │              │  │  (URL Match) │  │              │      │
│  └──────────────┘  └──────────────┘  └──────────────┘      │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│                    Middleware Layer                         │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │     Auth     │  │   Logging    │  │     CORS     │      │
│  │  Middleware  │  │  Middleware  │  │  Middleware  │      │
│  └──────────────┘  └──────────────┘  └──────────────┘      │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│                      HTTP Layer                             │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │   AsyncResp  │  │   Request    │  │   Response   │      │
│  │              │  │   Wrapper    │  │   Wrapper    │      │
│  └──────────────┘  └──────────────┘  └──────────────┘      │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│                      Server Layer                            │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │   HTTP       │  │ Connection   │  │   WebSocket  │      │
│  │   Server     │  │  Manager     │  │   Support    │      │
│  └──────────────┘  └──────────────┘  └──────────────┘      │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│                       I/O Layer                              │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │  Boost::Asio │  │   Sockets    │  │     SSL      │      │
│  │  Event Loop  │  │   Manager    │  │   Support    │      │
│  └──────────────┘  └──────────────┘  └──────────────┘      │
└─────────────────────────────────────────────────────────────┘
```

## Layer Responsibilities

### Application Layer
- **Purpose**: Main application coordination and lifecycle management
- **Responsibilities**:
  - Application initialization and shutdown
  - Configuration management
  - Component coordination
  - Route registration orchestration

### Routing Layer
- **Purpose**: URL matching and request dispatching
- **Responsibilities**:
  - URL pattern matching using trie data structure
  - Route handler registration
  - Path parameter extraction
  - HTTP method routing (GET, POST, PUT, DELETE, etc.)

### Middleware Layer
- **Purpose**: Cross-cutting concerns and request processing pipeline
- **Responsibilities**:
  - Authentication and authorization
  - Request/response logging
  - CORS handling
  - Rate limiting
  - Request validation

### HTTP Layer
- **Purpose**: HTTP abstraction and async response handling
- **Responsibilities**:
  - Request/response wrapping
  - Async operation coordination
  - JSON serialization/deserialization
  - Header management
  - Status code handling

### Server Layer
- **Purpose**: Network communication and connection management
- **Responsibilities**:
  - HTTP server implementation
  - Connection lifecycle management
  - WebSocket upgrade handling
  - SSL/TLS support
  - Connection pooling

### I/O Layer
- **Purpose**: Low-level I/O operations and event loop
- **Responsibilities**:
  - Async I/O operations
  - Socket management
  - Event loop coordination
  - SSL/TLS encryption
  - Network protocol handling

## Request Flow

```
┌─────────────┐
│   Client    │
└──────┬──────┘
       │ HTTP Request
       ↓
┌─────────────────────────────────────────────────────────────┐
│                     I/O Layer                               │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  Socket receives data → Boost::Asio event loop      │  │
│  └──────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
       ↓
┌─────────────────────────────────────────────────────────────┐
│                   Server Layer                              │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  Connection manager parses HTTP request              │  │
│  └──────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
       ↓
┌─────────────────────────────────────────────────────────────┐
│                   HTTP Layer                                │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  Request wrapper created                              │  │
│  │  AsyncResp wrapper initialized                        │  │
│  └──────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
       ↓
┌─────────────────────────────────────────────────────────────┐
│                Middleware Layer                            │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  Auth middleware → Logging middleware → CORS         │  │
│  │  (Request processing pipeline)                        │  │
│  └──────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
       ↓
┌─────────────────────────────────────────────────────────────┐
│                  Routing Layer                              │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  Trie matches URL pattern → Route handler selected   │  │
│  │  Path parameters extracted                           │  │
│  └──────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
       ↓
┌─────────────────────────────────────────────────────────────┐
│                Application Layer                            │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  Route handler executes business logic               │  │
│  │  Async operations performed (DBus, file I/O, etc.)   │  │
│  └──────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
       ↓
┌─────────────────────────────────────────────────────────────┐
│                   HTTP Layer (Return)                        │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  Response populated with data/status                 │  │
│  │  AsyncResp completion handler triggered              │  │
│  └──────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
       ↓
┌─────────────────────────────────────────────────────────────┐
│                  Middleware Layer (Return)                   │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  Response logging → Header modification              │  │
│  │  (Response processing pipeline)                      │  │
│  └──────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
       ↓
┌─────────────────────────────────────────────────────────────┐
│                   Server Layer (Return)                      │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  Response serialized and sent over socket            │  │
│  └──────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
       ↓
┌─────────────────────────────────────────────────────────────┐
│                     I/O Layer (Return)                      │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  Data written to socket via Boost::Asio               │  │
│  └──────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
       ↓
┌─────────────┐
│   Client    │
└─────────────┘
```

## Component Interactions

### Route Registration Flow

```
┌──────────────┐
│ Application  │
│   Startup    │
└──────┬───────┘
       │
       ↓
┌─────────────────────────────────────────────────────────────┐
│                    Router                                  │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  Route registration (URL pattern + handler)          │  │
│  └──────────────────────────────────────────────────────┘  │
       ↓
┌──────────────────────────────────────────────────────┐
│            Trie (URL Pattern Storage)                 │
│  ┌────────────────────────────────────────────────┐  │
│  │  Pattern added to trie structure               │  │
│  │  O(1) lookup capability established          │  │
│  └────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────┘
```

### Async Operation Flow

```
┌──────────────┐
│   Handler    │
└──────┬───────┘
       │
       ↓
┌─────────────────────────────────────────────────────────────┐
│                  AsyncResp                                 │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  Completion handler registered                       │  │
│  │  Async operation initiated (DBus, file, etc.)         │  │
│  └──────────────────────────────────────────────────────┘  │
       ↓
┌──────────────────────────────────────────────────────┐
│         External Async Operation                      │
│  ┌────────────────────────────────────────────────┐  │
│  │  Operation completes → Callback invoked          │  │
│  └────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────┘
       ↓
┌─────────────────────────────────────────────────────────────┐
│                  AsyncResp (Complete)                       │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  Response populated                                  │  │
│  │  Completion handler triggered                        │  │
│  └──────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

### Middleware Pipeline Flow

```
┌──────────────┐
│   Request    │
└──────┬───────┘
       ↓
┌─────────────────────────────────────────────────────────────┐
│              Middleware Chain (Before)                       │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐  │
│  │   Auth   │→ │  Logging │→ │   CORS   │→ │  Validate│  │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘  │
└─────────────────────────────────────────────────────────────┘
       ↓
┌──────────────┐
│   Handler    │
└──────┬───────┘
       ↓
┌─────────────────────────────────────────────────────────────┐
│              Middleware Chain (After)                        │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐  │
│  │ Validate │→ │   CORS   │→ │  Logging │→ │   Auth   │  │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘  │
└─────────────────────────────────────────────────────────────┘
       ↓
┌──────────────┐
│  Response    │
└──────────────┘
```

## Data Flow Patterns

### Synchronous Request Flow
```
Client → I/O → Server → HTTP → Middleware → Routing → Handler → 
Middleware → HTTP → Server → I/O → Client
```

### Asynchronous Request Flow
```
Client → I/O → Server → HTTP → Middleware → Routing → Handler → 
AsyncResp → External Operation → Callback → AsyncResp → 
Middleware → HTTP → Server → I/O → Client
```

### WebSocket Upgrade Flow
```
Client → I/O → Server → HTTP → Middleware → Routing → 
WebSocket Handler → WebSocket Connection → 
Persistent WebSocket Communication
```

## Security Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                   Security Layers                           │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  Application: Role-based access control               │  │
│  └──────────────────────────────────────────────────────┘  │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  Middleware: JWT token validation                     │  │
│  └──────────────────────────────────────────────────────┘  │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  Server: SSL/TLS encryption                          │  │
│  └──────────────────────────────────────────────────────┘  │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  I/O: Secure socket handling                         │  │
│  └──────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

## Performance Considerations

### Async-First Design
- All I/O operations are non-blocking
- Event loop prevents thread blocking
- High concurrency with minimal threads

### Efficient Routing
- Trie-based O(1) URL matching
- No regex overhead for static routes
- Path parameter extraction without parsing

### Memory Management
- Smart pointers for resource management
- Connection pooling
- Efficient buffer management

## Scalability Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                   Horizontal Scaling                         │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │  Instance 1  │  │  Instance 2  │  │  Instance N  │      │
│  │  (Port 8080) │  │  (Port 8081) │  │  (Port 808N) │      │
│  └──────────────┘  └──────────────┘  └──────────────┘      │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│                   Load Balancer                              │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  Round-robin or health-based distribution             │  │
│  └──────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

## Error Handling Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                   Error Handling Flow                        │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │   Handler    │→ │   AsyncResp  │→ │   Response   │      │
│  │   Exception  │  │   Error      │  │   Status      │      │
│  └──────────────┘  └──────────────┘  └──────────────┘      │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│                   Error Logging                              │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  Structured error logging with context               │  │
│  └──────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

## Configuration Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                   Configuration Sources                       │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │   Config     │  │ Environment  │  │   Command    │      │
│  │   File       │  │   Variables  │  │   Line Args  │      │
│  └──────────────┘  └──────────────┘  └──────────────┘      │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│                   Configuration Manager                      │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  Validation → Merging → Runtime Access               │  │
│  └──────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

## Monitoring and Observability

```
┌─────────────────────────────────────────────────────────────┐
│                   Monitoring Layers                           │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  Application: Business metrics, custom events         │  │
│  └──────────────────────────────────────────────────────┘  │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  HTTP: Request/response metrics, error rates         │  │
│  └──────────────────────────────────────────────────────┘  │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  Server: Connection metrics, resource usage          │  │
│  └──────────────────────────────────────────────────────┘  │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  I/O: Socket metrics, async operation timing        │  │
│  └──────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```