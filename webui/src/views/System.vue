<template>
  <div class="system">
    <h2>System Information</h2>
    <div class="info-grid" v-if="systemInfo">
      <div class="info-item">
        <label>Hostname:</label>
        <span>{{ systemInfo.hostname }}</span>
      </div>
      <div class="info-item">
        <label>Version:</label>
        <span>{{ systemInfo.version }}</span>
      </div>
      <div class="info-item">
        <label>Model:</label>
        <span>{{ systemInfo.model }}</span>
      </div>
      <div class="info-item">
        <label>Uptime:</label>
        <span>{{ systemInfo.uptime }} seconds</span>
      </div>
    </div>
    <div class="actions">
      <button @click="rebootSystem" class="btn btn-danger">Reboot System</button>
    </div>
  </div>
</template>

<script setup>
import { ref, onMounted } from 'vue'
import { systemApi } from '../api'

const systemInfo = ref(null)

onMounted(async () => {
  try {
    const response = await systemApi.getInfo()
    systemInfo.value = response.data
  } catch (error) {
    console.error('Failed to load system info:', error)
  }
})

const rebootSystem = async () => {
  if (confirm('Are you sure you want to reboot the system?')) {
    try {
      await systemApi.reboot()
      alert('System reboot initiated')
    } catch (error) {
      console.error('Failed to reboot system:', error)
      alert('Failed to reboot system')
    }
  }
}
</script>

<style scoped>
.info-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
  gap: 1.5rem;
  margin-top: 2rem;
}

.info-item {
  background: white;
  padding: 1rem;
  border-radius: 8px;
  box-shadow: 0 2px 4px rgba(0,0,0,0.1);
}

.info-item label {
  display: block;
  font-weight: bold;
  color: #666;
  margin-bottom: 0.5rem;
}

.info-item span {
  font-size: 1.1rem;
  color: #333;
}

.actions {
  margin-top: 2rem;
}

.btn {
  padding: 0.75rem 1.5rem;
  border: none;
  border-radius: 4px;
  font-size: 1rem;
  cursor: pointer;
  transition: background-color 0.3s;
}

.btn-danger {
  background-color: #f44336;
  color: white;
}

.btn-danger:hover {
  background-color: #d32f2f;
}
</style>
