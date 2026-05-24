#!/usr/bin/env python3
"""Simple WebSocket test script"""

import asyncio
import websockets
import json

async def test_websocket():
    uri = "ws://localhost:8081"
    try:
        async with websockets.connect(uri) as websocket:
            print(f"Connected to WebSocket at {uri}")
            
            # Send a test message
            message = {"type": "test", "data": "Hello WebSocket"}
            await websocket.send(json.dumps(message))
            print(f"Sent: {message}")
            
            # Receive response
            response = await websocket.recv()
            print(f"Received: {response}")
            
            # Wait for sensor data
            print("Waiting for sensor data...")
            for i in range(3):
                data = await websocket.recv()
                print(f"Sensor data {i+1}: {data}")
                
    except Exception as e:
        print(f"WebSocket test failed: {e}")

if __name__ == "__main__":
    asyncio.run(test_websocket())
