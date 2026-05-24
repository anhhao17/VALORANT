<template>
  <div class="dashboard">
    <h2>Dashboard</h2>
    <div class="status-cards">
      <div class="card">
        <h3>System Status</h3>
        <p v-if="systemStatus">{{ systemStatus.health }}</p>
        <p v-else>Loading...</p>
      </div>
      <div class="card">
        <h3>Temperature</h3>
        <p v-if="temperature">{{ temperature.cpu }}°C</p>
        <p v-else>Loading...</p>
        <span class="live-indicator" v-if="isWebSocketConnected">🔴 Live</span>
      </div>
      <div class="card">
        <h3>Power</h3>
        <p v-if="power">{{ power.total }}W</p>
        <p v-else>Loading...</p>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, onMounted, onUnmounted } from 'vue'
import { systemApi, hwmonApi } from '../api'
import { websocketService } from '../websocket'

const systemStatus = ref(null)
const temperature = ref(null)
const power = ref(null)
const isWebSocketConnected = ref(false)

const handleSensorData = (data) => {
  if (data.sensors) {
    // Update temperature from WebSocket data
    if (data.sensors.cpu_temp) {
      temperature.value = { cpu: data.sensors.cpu_temp.value }
    }
    // Update power from WebSocket data
    if (data.sensors.power) {
      power.value = { total: data.sensors.power.value }
    }
  }
}

onMounted(async () => {
  try {
    // Load initial data via HTTP
    const statusResponse = await systemApi.getStatus()
    systemStatus.value = statusResponse.data

    const tempResponse = await hwmonApi.getTemperature()
    temperature.value = tempResponse.data

    const powerResponse = await hwmonApi.getPower()
    power.value = powerResponse.data

    // Connect to WebSocket for real-time updates
    websocketService.on('connected', () => {
      isWebSocketConnected.value = true
      websocketService.subscribeToSensors()
    })

    websocketService.on('disconnected', () => {
      isWebSocketConnected.value = false
    })

    websocketService.on('sensor_data', handleSensorData)

    websocketService.connect()
  } catch (error) {
    console.error('Failed to load dashboard data:', error)
  }
})

onUnmounted(() => {
  websocketService.off('connected')
  websocketService.off('disconnected')
  websocketService.off('sensor_data', handleSensorData)
  websocketService.disconnect()
})
</script>

<style scoped>
.status-cards {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
  gap: 1.5rem;
  margin-top: 2rem;
}

.card {
  background: white;
  border-radius: 8px;
  padding: 1.5rem;
  box-shadow: 0 2px 4px rgba(0,0,0,0.1);
  position: relative;
}

.card h3 {
  margin-top: 0;
  color: #333;
}

.card p {
  font-size: 1.5rem;
  font-weight: bold;
  color: #4CAF50;
}

.live-indicator {
  position: absolute;
  top: 1rem;
  right: 1rem;
  font-size: 0.8rem;
  background: #f44336;
  color: white;
  padding: 0.25rem 0.5rem;
  border-radius: 4px;
  animation: pulse 2s infinite;
}

@keyframes pulse {
  0%, 100% {
    opacity: 1;
  }
  50% {
    opacity: 0.5;
  }
}
</style>
