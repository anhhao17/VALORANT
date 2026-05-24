// WebSocket service for real-time sensor data streaming
class WebSocketService {
  constructor() {
    this.ws = null
    this.reconnectInterval = 5000
    this.reconnectAttempts = 0
    this.maxReconnectAttempts = 10
    this.listeners = new Map()
  }

  connect(url = 'ws://localhost:8080') {
    if (this.ws && this.ws.readyState === WebSocket.OPEN) {
      console.log('WebSocket already connected')
      return
    }

    this.ws = new WebSocket(url)

    this.ws.onopen = () => {
      console.log('WebSocket connected')
      this.reconnectAttempts = 0
      this.emit('connected')
    }

    this.ws.onmessage = (event) => {
      try {
        const data = JSON.parse(event.data)
        this.emit('message', data)
        
        // Emit specific events based on message type
        if (data.type === 'sensor_data') {
          this.emit('sensor_data', data)
        } else if (data.type === 'event') {
          this.emit('event', data)
        }
      } catch (error) {
        console.error('Failed to parse WebSocket message:', error)
      }
    }

    this.ws.onerror = (error) => {
      console.error('WebSocket error:', error)
      this.emit('error', error)
    }

    this.ws.onclose = () => {
      console.log('WebSocket disconnected')
      this.emit('disconnected')
      this.attemptReconnect()
    }
  }

  disconnect() {
    if (this.ws) {
      this.ws.close()
      this.ws = null
    }
  }

  send(message) {
    if (this.ws && this.ws.readyState === WebSocket.OPEN) {
      this.ws.send(JSON.stringify(message))
    } else {
      console.warn('WebSocket not connected, message not sent')
    }
  }

  subscribeToSensors() {
    this.send({ type: 'subscribe', channel: 'sensors' })
  }

  unsubscribeFromSensors() {
    this.send({ type: 'unsubscribe', channel: 'sensors' })
  }

  on(event, callback) {
    if (!this.listeners.has(event)) {
      this.listeners.set(event, [])
    }
    this.listeners.get(event).push(callback)
  }

  off(event, callback) {
    if (this.listeners.has(event)) {
      const callbacks = this.listeners.get(event)
      const index = callbacks.indexOf(callback)
      if (index > -1) {
        callbacks.splice(index, 1)
      }
    }
  }

  emit(event, data) {
    if (this.listeners.has(event)) {
      this.listeners.get(event).forEach(callback => callback(data))
    }
  }

  attemptReconnect() {
    if (this.reconnectAttempts < this.maxReconnectAttempts) {
      this.reconnectAttempts++
      console.log(`Attempting to reconnect (${this.reconnectAttempts}/${this.maxReconnectAttempts})...`)
      setTimeout(() => {
        this.connect()
      }, this.reconnectInterval)
    } else {
      console.error('Max reconnection attempts reached')
      this.emit('max_reconnect_attempts_reached')
    }
  }

  isConnected() {
    return this.ws && this.ws.readyState === WebSocket.OPEN
  }
}

// Export singleton instance
export const websocketService = new WebSocketService()
