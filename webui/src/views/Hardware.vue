<template>
  <div class="hardware">
    <h2>Hardware Monitoring</h2>
    
    <div class="section">
      <h3>Temperature Sensors</h3>
      <div class="sensor-grid" v-if="temperature">
        <div class="sensor-card">
          <label>CPU</label>
          <span>{{ temperature.cpu }}°C</span>
        </div>
        <div class="sensor-card">
          <label>GPU</label>
          <span>{{ temperature.gpu }}°C</span>
        </div>
        <div class="sensor-card">
          <label>PMIC</label>
          <span>{{ temperature.pmic }}°C</span>
        </div>
        <div class="sensor-card">
          <label>Thermal</label>
          <span>{{ temperature.thermal }}°C</span>
        </div>
      </div>
    </div>

    <div class="section">
      <h3>Power Sensors</h3>
      <div class="sensor-grid" v-if="power">
        <div class="sensor-card">
          <label>Total</label>
          <span>{{ power.total }}W</span>
        </div>
        <div class="sensor-card">
          <label>CPU</label>
          <span>{{ power.cpu }}W</span>
        </div>
        <div class="sensor-card">
          <label>GPU</label>
          <span>{{ power.gpu }}W</span>
        </div>
        <div class="sensor-card">
          <label>DDR</label>
          <span>{{ power.ddr }}W</span>
        </div>
      </div>
    </div>

    <div class="section">
      <h3>Fan Speeds</h3>
      <div class="sensor-grid" v-if="fans">
        <div class="sensor-card">
          <label>Fan 1</label>
          <span>{{ fans.fan1 }} RPM</span>
        </div>
        <div class="sensor-card">
          <label>Fan 2</label>
          <span>{{ fans.fan2 }} RPM</span>
        </div>
        <div class="sensor-card">
          <label>Fan 3</label>
          <span>{{ fans.fan3 }} RPM</span>
        </div>
      </div>
    </div>

    <div class="section">
      <h3>Voltage Sensors</h3>
      <div class="sensor-grid" v-if="voltage">
        <div class="sensor-card">
          <label>VDD CPU</label>
          <span>{{ voltage.vdd_cpu }}V</span>
        </div>
        <div class="sensor-card">
          <label>VDD GPU</label>
          <span>{{ voltage.vdd_gpu }}V</span>
        </div>
        <div class="sensor-card">
          <label>VDD DDR</label>
          <span>{{ voltage.vdd_ddr }}V</span>
        </div>
        <div class="sensor-card">
          <label>VDD 5V</label>
          <span>{{ voltage.vdd_5v }}V</span>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, onMounted } from 'vue'
import { hwmonApi } from '../api'

const temperature = ref(null)
const power = ref(null)
const fans = ref(null)
const voltage = ref(null)

onMounted(async () => {
  try {
    const [tempResponse, powerResponse, fansResponse, voltageResponse] = await Promise.all([
      hwmonApi.getTemperature(),
      hwmonApi.getPower(),
      hwmonApi.getFans(),
      hwmonApi.getVoltage()
    ])

    temperature.value = tempResponse.data
    power.value = powerResponse.data
    fans.value = fansResponse.data
    voltage.value = voltageResponse.data
  } catch (error) {
    console.error('Failed to load hardware data:', error)
  }
})
</script>

<style scoped>
.section {
  margin-top: 2rem;
}

.section h3 {
  margin-bottom: 1rem;
  color: #333;
}

.sensor-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));
  gap: 1rem;
}

.sensor-card {
  background: white;
  padding: 1rem;
  border-radius: 8px;
  box-shadow: 0 2px 4px rgba(0,0,0,0.1);
  text-align: center;
}

.sensor-card label {
  display: block;
  font-weight: bold;
  color: #666;
  margin-bottom: 0.5rem;
  font-size: 0.9rem;
}

.sensor-card span {
  font-size: 1.5rem;
  font-weight: bold;
  color: #4CAF50;
}
</style>
