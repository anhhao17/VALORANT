<template>
  <div class="dashboard">
    <h2>Dashboard</h2>
    
    <!-- System Status Card -->
    <div class="status-cards">
      <div class="card">
        <h3>System Status</h3>
        <p v-if="systemStatus">{{ systemStatus.health }}</p>
        <p v-else>Loading...</p>
      </div>
      
      <!-- Streaming Status Card -->
      <div class="card">
        <h3>Streaming</h3>
        <p v-if="streamingStats">{{ streamingStats.active }}/{{ streamingStats.total }} Active</p>
        <p v-else>Loading...</p>
        <router-link to="/streaming" class="btn-link">Manage Streams →</router-link>
      </div>
      
      <!-- Temperature Gauge -->
      <div class="card">
        <h3>Temperature</h3>
        <GaugeChart 
          v-if="temperature && temperature.cpu"
          :value="temperature.cpu"
          :min="0"
          :max="100"
          unit="°C"
          label="CPU Temperature"
          :thresholds="{ warning: 70, critical: 85 }"
        />
        <p v-else>Loading...</p>
        <span class="live-indicator" v-if="isWebSocketConnected">🔴 Live</span>
      </div>
      
      <!-- Power Gauge -->
      <div class="card">
        <h3>Power</h3>
        <GaugeChart 
          v-if="power && power.total"
          :value="power.total"
          :min="0"
          :max="30"
          unit="W"
          label="Total Power"
          :thresholds="{ warning: 20, critical: 25 }"
        />
        <p v-else>Loading...</p>
      </div>
    </div>

    <!-- Temperature History Chart -->
    <div class="chart-section">
      <h3>Temperature History</h3>
      <LineChart 
        v-if="temperatureHistory.labels.length > 0"
        :chartData="temperatureChartData"
        :chartOptions="temperatureChartOptions"
      />
      <p v-else>Loading temperature data...</p>
    </div>

    <!-- Power History Chart -->
    <div class="chart-section">
      <h3>Power History</h3>
      <LineChart 
        v-if="powerHistory.labels.length > 0"
        :chartData="powerChartData"
        :chartOptions="powerChartOptions"
      />
      <p v-else>Loading power data...</p>
    </div>
  </div>
</template>

<script setup>
import { ref, onMounted, onUnmounted, computed } from 'vue'
import { systemApi, hwmonApi } from '../api'
import { websocketService } from '../websocket'
import { useAuthStore } from '../store/auth'
import LineChart from '../components/LineChart.vue'
import GaugeChart from '../components/GaugeChart.vue'

const systemStatus = ref(null)
const temperature = ref(null)
const power = ref(null)
const streamingStats = ref({ active: 0, total: 0 })
const isWebSocketConnected = ref(false)

// Historical data for charts
const temperatureHistory = ref({
  labels: [],
  cpu: [],
  gpu: [],
  pmic: []
})

const powerHistory = ref({
  labels: [],
  total: [],
  cpu: [],
  gpu: []
})

const MAX_HISTORY_POINTS = 30

const temperatureChartData = computed(() => ({
  labels: temperatureHistory.value.labels,
  datasets: [
    {
      label: 'CPU Temperature',
      data: temperatureHistory.value.cpu,
      borderColor: '#f44336',
      backgroundColor: 'rgba(244, 67, 54, 0.1)',
      fill: true,
      tension: 0.4
    },
    {
      label: 'GPU Temperature',
      data: temperatureHistory.value.gpu,
      borderColor: '#2196F3',
      backgroundColor: 'rgba(33, 150, 243, 0.1)',
      fill: true,
      tension: 0.4
    },
    {
      label: 'PMIC Temperature',
      data: temperatureHistory.value.pmic,
      borderColor: '#FF9800',
      backgroundColor: 'rgba(255, 152, 0, 0.1)',
      fill: true,
      tension: 0.4
    }
  ]
}))

const temperatureChartOptions = computed(() => ({
  title: {
    display: true,
    text: 'Temperature History (°C)'
  },
  scales: {
    y: {
      beginAtZero: false,
      title: {
        display: true,
        text: 'Temperature (°C)'
      }
    }
  }
}))

const powerChartData = computed(() => ({
  labels: powerHistory.value.labels,
  datasets: [
    {
      label: 'Total Power',
      data: powerHistory.value.total,
      borderColor: '#4CAF50',
      backgroundColor: 'rgba(76, 175, 80, 0.1)',
      fill: true,
      tension: 0.4
    },
    {
      label: 'CPU Power',
      data: powerHistory.value.cpu,
      borderColor: '#2196F3',
      backgroundColor: 'rgba(33, 150, 243, 0.1)',
      fill: true,
      tension: 0.4
    },
    {
      label: 'GPU Power',
      data: powerHistory.value.gpu,
      borderColor: '#9C27B0',
      backgroundColor: 'rgba(156, 39, 176, 0.1)',
      fill: true,
      tension: 0.4
    }
  ]
}))

const powerChartOptions = computed(() => ({
  title: {
    display: true,
    text: 'Power History (W)'
  },
  scales: {
    y: {
      beginAtZero: true,
      title: {
        display: true,
        text: 'Power (W)'
      }
    }
  }
}))

const addDataPoint = (history, label, data) => {
  const now = new Date()
  const timeLabel = label || now.toLocaleTimeString()
  
  history.labels.push(timeLabel)
  
  if (data.cpu !== undefined) history.cpu.push(data.cpu)
  if (data.gpu !== undefined) history.gpu.push(data.gpu)
  if (data.pmic !== undefined) history.pmic.push(data.pmic)
  if (data.total !== undefined) history.total.push(data.total)
  
  // Keep only the last MAX_HISTORY_POINTS
  if (history.labels.length > MAX_HISTORY_POINTS) {
    history.labels.shift()
    if (history.cpu.length > 0) history.cpu.shift()
    if (history.gpu.length > 0) history.gpu.shift()
    if (history.pmic.length > 0) history.pmic.shift()
    if (history.total.length > 0) history.total.shift()
  }
}

const handleSensorData = (data) => {
  if (data.sensors) {
    // Update current readings
    if (data.sensors.cpu_temp) {
      temperature.value = { cpu: data.sensors.cpu_temp.value }
    }
    if (data.sensors.power) {
      power.value = { total: data.sensors.power.value }
    }
    
    // Add to history for charts
    const now = new Date().toLocaleTimeString()
    
    if (data.sensors.cpu_temp || data.sensors.gpu_temp || data.sensors.pmic_temp) {
      addDataPoint(temperatureHistory.value, now, {
        cpu: data.sensors.cpu_temp?.value,
        gpu: data.sensors.gpu_temp?.value,
        pmic: data.sensors.pmic_temp?.value
      })
    }
    
    if (data.sensors.power || data.sensors.cpu_power || data.sensors.gpu_power) {
      addDataPoint(powerHistory.value, now, {
        total: data.sensors.power?.value,
        cpu: data.sensors.cpu_power?.value,
        gpu: data.sensors.gpu_power?.value
      })
    }
  }
}

const loadStreamingStats = async () => {
  try {
    const authStore = useAuthStore()
    const response = await fetch('/api/streams', {
      headers: {
        'Authorization': `Token ${authStore.sessionToken}`
      }
    })
    
    if (response.ok) {
      const streams = await response.json()
      const active = streams.filter(s => s.streaming).length
      streamingStats.value = {
        active,
        total: streams.length
      }
    }
  } catch (error) {
    console.error('Failed to load streaming stats:', error)
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

    // Load streaming stats
    await loadStreamingStats()

    // Initialize chart data with current readings
    const now = new Date().toLocaleTimeString()
    if (temperature.value) {
      addDataPoint(temperatureHistory.value, now, {
        cpu: temperature.value.cpu,
        gpu: temperature.value.gpu,
        pmic: temperature.value.pmic
      })
    }
    if (power.value) {
      addDataPoint(powerHistory.value, now, {
        total: power.value.total,
        cpu: power.value.cpu,
        gpu: power.value.gpu
      })
    }

    // Connect to WebSocket for real-time updates
    websocketService.on('connected', () => {
      isWebSocketConnected.value = true
      websocketService.subscribeToSensors()
    })

    websocketService.on('disconnected', () => {
      isWebSocketConnected.value = false
    })

    websocketService.on('sensor_data', handleSensorData)

    // Set session token for WebSocket authentication
    const authStore = useAuthStore()
    if (authStore.sessionToken) {
      websocketService.setSessionToken(authStore.sessionToken)
    }

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

.chart-section {
  background: white;
  border-radius: 8px;
  padding: 1.5rem;
  box-shadow: 0 2px 4px rgba(0,0,0,0.1);
  margin-top: 2rem;
}

.chart-section h3 {
  margin-top: 0;
  color: #333;
  margin-bottom: 1rem;
}

@keyframes pulse {
  0%, 100% {
    opacity: 1;
  }
  50% {
    opacity: 0.5;
  }
}

.btn-link {
  color: #007bff;
  text-decoration: none;
  font-size: 0.9rem;
  margin-top: 0.5rem;
  display: inline-block;
}

.btn-link:hover {
  text-decoration: underline;
}
</style>
