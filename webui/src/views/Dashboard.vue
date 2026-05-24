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
import { ref, onMounted } from 'vue'
import { systemApi, hwmonApi } from '../api'

const systemStatus = ref(null)
const temperature = ref(null)
const power = ref(null)

onMounted(async () => {
  try {
    const statusResponse = await systemApi.getStatus()
    systemStatus.value = statusResponse.data

    const tempResponse = await hwmonApi.getTemperature()
    temperature.value = tempResponse.data

    const powerResponse = await hwmonApi.getPower()
    power.value = powerResponse.data
  } catch (error) {
    console.error('Failed to load dashboard data:', error)
  }
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
</style>
