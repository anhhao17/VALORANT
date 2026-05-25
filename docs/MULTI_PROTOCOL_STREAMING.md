# Multi-Protocol Streaming Architecture

## Overview

The Jetson BMCweb supports multiple streaming protocols to provide flexibility for different client types and network conditions. **Important: Each camera stream uses ONE protocol at a time to minimize CPU and memory usage.** The first client to connect determines the protocol for that stream, and all subsequent clients must use the same protocol.

## Resource Constraints

- **Single Encoder per Camera**: Each camera uses only one encoder at a time
- **Single Network Stream per Camera**: Each camera has only one active network stream
- **Protocol Locking**: Once a protocol is selected for a stream, it cannot be changed until all clients disconnect
- **Minimal CPU/Memory**: Designed for resource-constrained Jetson/Pi devices

## Supported Protocols

### 1. MJPEG (Motion JPEG)
- **Transport**: HTTP
- **Browser Support**: Native (no plugins required)
- **Latency**: Medium (200-500ms)
- **Bandwidth**: High (uncompressed frames)
- **Use Case**: Web browsers, simple viewers
- **Multi-client**: Yes (HTTP server handles multiple connections)

### 2. UDP/RTP
- **Transport**: UDP
- **Browser Support**: Limited (requires WebRTC or plugins)
- **Latency**: Very low (50-100ms)
- **Bandwidth**: Low (compressed)
- **Use Case**: Local network, low-latency applications
- **Multi-client**: Yes (via multicast)

### 3. RTSP (Real-Time Streaming Protocol)
- **Transport**: TCP/UDP
- **Browser Support**: Limited (requires plugins or WebRTC-to-RTSP gateway)
- **Latency**: Low (100-200ms)
- **Bandwidth**: Medium
- **Use Case**: IP cameras, surveillance systems
- **Multi-client**: Yes

### 4. WebRTC
- **Transport**: UDP/TCP (adaptive)
- **Browser Support**: Excellent (native in modern browsers)
- **Latency**: Very low (50-150ms)
- **Bandwidth**: Adaptive
- **Use Case**: Modern web applications, peer-to-peer
- **Multi-client**: Yes (requires server-side coordination)

### 5. HLS (HTTP Live Streaming)
- **Transport**: HTTP
- **Browser Support**: Excellent (native in Safari, supported elsewhere)
- **Latency**: High (2-10 seconds)
- **Bandwidth**: Adaptive
- **Use Case**: Internet streaming, CDN distribution
- **Multi-client**: Yes

## Architecture Diagram

### System Overview (Resource-Efficient)
```
┌─────────────────────────────────────────────────────────────────┐
│                        Jetson/Pi Device                         │
│                                                                  │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │
│  │ Camera 0     │  │ Camera 1     │  │ Camera N     │          │
│  │ /dev/video0  │  │ /dev/video1  │  │ /dev/videoN  │          │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘          │
│         │                 │                 │                    │
│         └─────────────────┴─────────────────┘                    │
│                           │                                      │
│                    ┌──────▼──────┐                               │
│                    │  Camera     │                               │
│                    │  Capture    │                               │
│                    │  Engine     │                               │
│                    └──────┬──────┘                               │
│                           │                                      │
│         ┌─────────────────┼─────────────────┐                   │
│         │                 │                 │                   │
│    ┌────▼────┐      ┌────▼────┐      ┌────▼────┐              │
│    │ Camera 0 │      │ Camera 1 │      │ Camera N │              │
│    │ Encoder  │      │ Encoder  │      │ Encoder  │              │
│    │ (Single) │      │ (Single) │      │ (Single) │              │
│    └────┬────┘      └────┬────┘      └────┬────┘              │
│         │                 │                 │                   │
│         ▼                 ▼                 ▼                   │
│    ┌────▼────┐      ┌────▼────┐      ┌────▼────┐              │
│    │ Protocol │      │ Protocol │      │ Protocol │              │
│    │ Selector │      │ Selector │      │ Selector │              │
│    │ (One per │      │ (One per │      │ (One per │              │
│    │  camera) │      │  camera) │      │  camera) │              │
│    └────┬────┘      └────┬────┘      └────┬────┘              │
│         │                 │                 │                   │
│         ▼                 ▼                 ▼                   │
│    ┌────▼────┐      ┌────▼────┐      ┌────▼────┐              │
│    │ Network │      │ Network │      │ Network │              │
│    │ Stream  │      │ Stream  │      │ Stream  │              │
│    │ (Single) │      │ (Single) │      │ (Single) │              │
│    └────┬────┘      └────┬────┘      └────┬────┘              │
│         │                 │                 │                   │
│         └─────────────────┼─────────────────┘                   │
│                           │                                      │
│                    ┌──────▼──────┐                               │
│                    │  Stream     │                               │
│                    │  Manager    │                               │
│                    │  (On-Demand)│                               │
│                    └──────┬──────┘                               │
│                           │                                      │
└───────────────────────────┼──────────────────────────────────────┘
                           │
         ┌─────────────────┼─────────────────┐
         │                 │                 │
    ┌────▼────┐      ┌────▼────┐      ┌────▼────┐
    │ Client 1│      │ Client 2│      │ Client N│
    │ (Same   │      │ (Same   │      │ (Same   │
    │ Protocol)│     │ Protocol)│     │ Protocol)│
    └─────────┘      └─────────┘      └─────────┘
```

### On-Demand Streaming Flow
```
First Client Connection (Protocol Selection):
┌─────────┐    ┌─────────┐    ┌─────────┐    ┌─────────┐
│ Client  │───▶│ Protocol │───▶│ Encoder │───▶│ Camera  │
│ Connect │    │ Select  │    │ Start   │    │ Start   │
│ (MJPEG) │    │ & Lock  │    │ (Single)│    │         │
└─────────┘    └─────────┘    └─────────┘    └─────────┘
                                                   │
                                                   ▼
                                            ┌─────────┐
                                            │ Frame   │
                                            │ Capture │
                                            └────┬────┘
                                                 │
                                                 ▼
                                            ┌─────────┐
                                            │ MJPEG   │
                                            │ Encoder │
                                            └────┬────┘
                                                 │
                                                 ▼
                                            ┌─────────┐
                                            │ HTTP    │
                                            │ Stream  │
                                            └────┬────┘
                                                 │
              ┌──────────────────────────────────┼──────────────────────────┐
              │                                  │                          │
        ┌─────▼─────┐                    ┌──────▼──────┐            ┌──────▼──────┐
        │ Client 1  │                    │ Client 2    │            │ Client N    │
        │ (MJPEG)   │                    │ (MJPEG)     │            │ (MJPEG)     │
        └───────────┘                    └─────────────┘            └─────────────┘

Second Client Connection (Protocol Locked):
┌─────────┐    ┌─────────┐    ┌─────────┐
│ Client  │───▶│ Protocol │───▶│ Session │
│ Connect │    │ Check   │    │ Add    │
│ (RTSP)  │    │ (Locked)│    │ (MJPEG)│
└─────────┘    └─────────┘    └─────────┘
                      │
                      ▼
              ┌─────────────┐
              │ Protocol    │
              │ Mismatch!   │
              │ Return Error│
              └─────────────┘

Client Disconnect Flow:
┌─────────┐    ┌─────────┐    ┌─────────┐    ┌─────────┐
│ Client  │───▶│ Session │───▶│ Check   │───▶│ Camera  │
│ Disconnect│   │ Remove  │    │ Clients │    │ Stop    │
└─────────┘    └─────────┘    └─────────┘    └─────────┘
                                   │
                    ┌──────────────┼──────────────┐
                    │              │              │
              ┌─────▼─────┐  ┌─────▼─────┐  ┌─────▼─────┐
              │ More      │  │ More      │  │ No More   │
              │ Clients?  │  │ Clients?  │  │ Clients   │
              └─────┬─────┘  └─────┬─────┘  └─────┬─────┘
                    │              │              │
                    │ NO           │ YES          │
                    │              │              │
              ┌─────▼─────┐  ┌─────▼─────┐  ┌─────▼─────┐
              │ Stop      │  │ Keep      │  │ Stop      │
              │ Encoder   │  │ Streaming│  │ Camera    │
              │ & Unlock  │  │          │  │ & Unlock  │
              └───────────┘  └───────────┘  └───────────┘
```

### Protocol Selection Flow
```
┌─────────┐
│ Client  │
│ Request │
└────┬────┘
     │
     ▼
┌─────────────┐
│ Check Stream│
│ Status      │
└────┬────────┘
     │
     ├──────────┬──────────┐
     │          │          │
     ▼          ▼          ▼
┌─────────┐ ┌─────────┐ ┌─────────┐
│ No      │ │ Active  │ │ Active  │
│ Clients │ │ Stream  │ │ Stream  │
└────┬────┘ └────┬────┘ └────┬────┘
     │          │          │
     │          │          ▼
     │          │    ┌─────────────┐
     │          │    │ Check       │
     │          │    │ Protocol   │
     │          │    │ Match?      │
     │          │    └──────┬──────┘
     │          │           │
     │          │    ├───────┴───────┐
     │          │    │               │
     │          │    ▼               ▼
     │          │ ┌─────────┐   ┌─────────┐
     │          │ │ Match   │   │ Mismatch│
     │          │ │ Allow   │   │ Reject  │
     │          │ └────┬────┘   └─────────┘
     │          │      │
     │          └──────┼──────────────┐
     │                 │              │
     ▼                 ▼              ▼
┌─────────────┐  ┌─────────┐   ┌─────────┐
│ Select     │  │ Add     │   │ Return  │
│ Protocol   │  │ Client  │   │ Error   │
└────┬────────┘  └─────────┘   └─────────┘
     │
     ▼
┌─────────────┐
│ Start       │
│ Encoder     │
│ (Single)    │
└────┬────────┘
     │
     ▼
┌─────────────┐
│ Lock        │
│ Protocol    │
└────┬────────┘
     │
     ▼
┌─────────────┐
│ Stream to   │
│ Client      │
└─────────────┘
```

### Multi-Client Frame Distribution (Single Protocol)
```
┌─────────────┐
│ Camera      │
│ Frame       │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│ Single      │
│ Encoder     │
│ (MJPEG)     │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│ Single      │
│ Network     │
│ Stream      │
└──────┬──────┘
       │
       ├──────────┬──────────┬──────────┐
       │          │          │          │
       ▼          ▼          ▼          ▼
┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐
│ Client 1│ │ Client 2│ │ Client 3│ │ Client N│
│ (MJPEG) │ │ (MJPEG) │ │ (MJPEG) │ │ (MJPEG) │
└─────────┘ └─────────┘ └─────────┘ └─────────┘
```

## API Design

### Protocol Selection
```
First Client Request (Protocol Selection):
GET /video/camera_0?protocol=mjpeg  → Selects MJPEG, locks protocol
GET /video/camera_0?protocol=udp    → Selects UDP, locks protocol
GET /video/camera_0?protocol=rtsp   → Selects RTSP, locks protocol

Subsequent Client Requests (Must Match):
GET /video/camera_0?protocol=mjpeg  → Allowed (matches locked protocol)
GET /video/camera_0?protocol=udp    → Error (protocol mismatch)
GET /video/camera_0?protocol=rtsp   → Error (protocol mismatch)
```

### Streaming Endpoints
```
MJPEG:   GET /video/{id}?protocol=mjpeg
UDP:     GET /video/{id}?protocol=udp
RTSP:    GET /video/{id}?protocol=rtsp
WebRTC:  GET /video/{id}?protocol=webrtc
HLS:     GET /video/{id}?protocol=hls
```

### Protocol Status API
```
GET /api/streams/{id}/status
Response:
{
  "id": "camera_0",
  "active": true,
  "protocol": "mjpeg",
  "locked": true,
  "clients": 3
}
```

## Camera Auto-Detection

### Detection Process
```
System Startup
       │
       ▼
┌─────────────┐
│ Scan /dev/  │
│ video*      │
└──────┬──────┘
       │
       ├────────┬────────┬────────┐
       │        │        │        │
       ▼        ▼        ▼        ▼
┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐
│/dev/    │ │/dev/    │ │/dev/    │ │/dev/    │
│video0   │ │video1   │ │video2   │ │videoN   │
└────┬────┘ └────┬────┘ └────┬────┘ └────┬────┘
     │          │          │          │
     ▼          ▼          ▼          ▼
┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐
│Camera 0 │ │Camera 1 │ │Camera 2 │ │Camera N │
│Stream   │ │Stream   │ │Stream   │ │Stream   │
│Created  │ │Created  │ │Created  │ │Created  │
└─────────┘ └─────────┘ └─────────┘ └─────────┘
```

## Implementation Phases

### Phase 1: MJPEG Streaming
- Implement MJPEG over HTTP
- Browser-compatible streaming
- Basic multi-client support

### Phase 2: UDP/RTP Streaming
- Implement UDP multicast
- Low-latency streaming
- Local network optimization

### Phase 3: RTSP Support
- Implement RTSP server
- Standard protocol support
- IP camera compatibility

### Phase 4: WebRTC Support
- Implement WebRTC signaling
- Modern browser support
- Adaptive streaming

### Phase 5: HLS Support
- Implement HLS packaging
- Internet streaming
- CDN integration

## Configuration

### YAML Configuration
```
streaming:
  enabled: true
  maxStreams: 10
  autoDetectCameras: true
  defaultProtocol: mjpeg
  protocols:
    mjpeg:
      enabled: true
      quality: 80
      fps: 30
    udp:
      enabled: true
      multicast: true
      multicastGroup: "239.255.0.1"
      port: 5004
    rtsp:
      enabled: true
      port: 8554
    webrtc:
      enabled: false
      stunServer: "stun:stun.l.google.com:19302"
    hls:
      enabled: false
      segmentDuration: 2
```

## Benefits

1. **Resource Efficiency**: Single encoder per camera minimizes CPU/memory usage
2. **Protocol Flexibility**: Clients can choose optimal protocol for their use case
3. **Browser Support**: Native browser streaming via MJPEG/WebRTC
4. **Performance**: Low-latency options for real-time applications
5. **Scalability**: Multi-client support within same protocol
6. **Standards**: Industry-standard protocols for compatibility
7. **On-Demand**: Automatic camera start/stop based on client connections
8. **Protocol Locking**: Prevents resource conflicts and ensures stability

## Resource Comparison

### Multi-Protocol (Not Recommended)
```
Camera 0:
├── MJPEG Encoder (CPU: 15%, Memory: 50MB)
├── UDP Encoder (CPU: 10%, Memory: 40MB)
├── RTSP Encoder (CPU: 12%, Memory: 45MB)
└── Total: CPU 37%, Memory 135MB (Too expensive!)
```

### Single-Protocol (Recommended)
```
Camera 0:
└── MJPEG Encoder (CPU: 15%, Memory: 50MB) ← Only one encoder!

Resource Savings:
- CPU: 59% reduction
- Memory: 63% reduction
- Better for Jetson/Pi constraints
```

## Protocol Selection Strategy

### Default Protocol Priority
1. **MJPEG** - Default for web browsers (simplest, most compatible)
2. **UDP/RTP** - For local network, low-latency needs
3. **RTSP** - For IP camera compatibility
4. **WebRTC** - For modern web applications
5. **HLS** - For internet streaming

### UI Protocol Selection
```
UI Component:
┌─────────────────────────────────┐
│ Camera 0                        │
│ ┌─────────────────────────────┐ │
│ │ Protocol: [MJPEG ▼]         │ │
│ │ Status: Active (3 viewers)  │ │
│ │ [Change Protocol]            │ │
│ └─────────────────────────────┘ │
│ ┌─────────────────────────────┐ │
│ │ [Video Stream]              │ │
│ └─────────────────────────────┘ │
└─────────────────────────────────┘

Change Protocol Flow:
1. User clicks "Change Protocol"
2. UI checks if other clients are connected
3. If yes: Show warning "Disconnect all viewers first"
4. If no: Allow protocol change
5. Backend stops current encoder, starts new encoder
```