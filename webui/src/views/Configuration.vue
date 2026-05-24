<template>
  <div class="configuration">
    <h2>Configuration Management</h2>
    
    <div class="config-tabs">
      <button 
        v-for="tab in tabs" 
        :key="tab.id"
        @click="activeTab = tab.id"
        :class="{ active: activeTab === tab.id }"
        class="tab-button"
      >
        {{ tab.label }}
      </button>
    </div>

    <div class="config-content">
      <!-- System Configuration -->
      <div v-if="activeTab === 'system'" class="config-section">
        <h3>System Settings</h3>
        <form @submit.prevent="saveSystemConfig" class="config-form">
          <div class="form-group">
            <label for="log_level">Log Level</label>
            <select id="log_level" v-model="systemConfig.log_level" class="form-control">
              <option value="debug">Debug</option>
              <option value="info">Info</option>
              <option value="warning">Warning</option>
              <option value="error">Error</option>
            </select>
          </div>
          <div class="form-group">
            <label for="max_connections">Max Connections</label>
            <input 
              type="number" 
              id="max_connections" 
              v-model="systemConfig.max_connections" 
              class="form-control"
              min="1"
              max="1000"
            >
          </div>
          <div class="form-group">
            <label for="session_timeout">Session Timeout (seconds)</label>
            <input 
              type="number" 
              id="session_timeout" 
              v-model="systemConfig.session_timeout" 
              class="form-control"
              min="60"
              max="86400"
            >
          </div>
          <button type="submit" class="btn btn-primary" :disabled="saving">
            {{ saving ? 'Saving...' : 'Save System Config' }}
          </button>
        </form>
      </div>

      <!-- Network Configuration -->
      <div v-if="activeTab === 'network'" class="config-section">
        <h3>Network Settings</h3>
        <form @submit.prevent="saveNetworkConfig" class="config-form">
          <div class="form-group">
            <label for="hostname">Hostname</label>
            <input 
              type="text" 
              id="hostname" 
              v-model="networkConfig.hostname" 
              class="form-control"
              required
            >
          </div>
          <div class="form-group">
            <label for="port">HTTP Port</label>
            <input 
              type="number" 
              id="port" 
              v-model="networkConfig.port" 
              class="form-control"
              min="1"
              max="65535"
            >
          </div>
          <div class="form-group">
            <label for="ssl_port">HTTPS Port</label>
            <input 
              type="number" 
              id="ssl_port" 
              v-model="networkConfig.ssl_port" 
              class="form-control"
              min="1"
              max="65535"
            >
          </div>
          <div class="form-group">
            <label for="enable_ssl">Enable SSL</label>
            <input 
              type="checkbox" 
              id="enable_ssl" 
              v-model="networkConfig.enable_ssl"
              class="form-checkbox"
            >
          </div>
          <button type="submit" class="btn btn-primary" :disabled="saving">
            {{ saving ? 'Saving...' : 'Save Network Config' }}
          </button>
        </form>
      </div>

      <!-- Hardware Configuration -->
      <div v-if="activeTab === 'hardware'" class="config-section">
        <h3>Hardware Settings</h3>
        <form @submit.prevent="saveHardwareConfig" class="config-form">
          <div class="form-group">
            <label for="sensor_update_interval">Sensor Update Interval (ms)</label>
            <input 
              type="number" 
              id="sensor_update_interval" 
              v-model="hardwareConfig.sensor_update_interval" 
              class="form-control"
              min="100"
              max="60000"
            >
          </div>
          <div class="form-group">
            <label for="temp_warning">Temperature Warning Threshold (°C)</label>
            <input 
              type="number" 
              id="temp_warning" 
              v-model="hardwareConfig.temperature_threshold_warning" 
              class="form-control"
              min="0"
              max="100"
            >
          </div>
          <div class="form-group">
            <label for="temp_critical">Temperature Critical Threshold (°C)</label>
            <input 
              type="number" 
              id="temp_critical" 
              v-model="hardwareConfig.temperature_threshold_critical" 
              class="form-control"
              min="0"
              max="100"
            >
          </div>
          <div class="form-group">
            <label for="power_warning">Power Warning Threshold (W)</label>
            <input 
              type="number" 
              id="power_warning" 
              v-model="hardwareConfig.power_threshold_warning" 
              class="form-control"
              min="0"
              max="100"
            >
          </div>
          <div class="form-group">
            <label for="power_critical">Power Critical Threshold (W)</label>
            <input 
              type="number" 
              id="power_critical" 
              v-model="hardwareConfig.power_threshold_critical" 
              class="form-control"
              min="0"
              max="100"
            >
          </div>
          <button type="submit" class="btn btn-primary" :disabled="saving">
            {{ saving ? 'Saving...' : 'Save Hardware Config' }}
          </button>
        </form>
      </div>

      <!-- Security Configuration -->
      <div v-if="activeTab === 'security'" class="config-section">
        <h3>Security Settings</h3>
        <form @submit.prevent="saveSecurityConfig" class="config-form">
          <div class="form-group">
            <label for="enable_auth">Enable Authentication</label>
            <input 
              type="checkbox" 
              id="enable_auth" 
              v-model="securityConfig.enable_auth"
              class="form-checkbox"
            >
          </div>
          <div class="form-group">
            <label for="max_login_attempts">Max Login Attempts</label>
            <input 
              type="number" 
              id="max_login_attempts" 
              v-model="securityConfig.max_login_attempts" 
              class="form-control"
              min="1"
              max="10"
            >
          </div>
          <div class="form-group">
            <label for="csrf_protection">Enable CSRF Protection</label>
            <input 
              type="checkbox" 
              id="csrf_protection" 
              v-model="securityConfig.csrf_protection"
              class="form-checkbox"
            >
          </div>
          <div class="form-group">
            <label for="session_cookie_name">Session Cookie Name</label>
            <input 
              type="text" 
              id="session_cookie_name" 
              v-model="securityConfig.session_cookie_name" 
              class="form-control"
              required
            >
          </div>
          <button type="submit" class="btn btn-primary" :disabled="saving">
            {{ saving ? 'Saving...' : 'Save Security Config' }}
          </button>
        </form>
      </div>
    </div>

    <!-- Status Message -->
    <div v-if="statusMessage" :class="['status-message', statusType]">
      {{ statusMessage }}
    </div>
  </div>
</template>

<script setup>
import { ref, onMounted } from 'vue'
import axios from 'axios'

const API_BASE_URL = import.meta.env.VITE_API_BASE_URL || 'http://localhost:8080'

const tabs = [
  { id: 'system', label: 'System' },
  { id: 'network', label: 'Network' },
  { id: 'hardware', label: 'Hardware' },
  { id: 'security', label: 'Security' }
]

const activeTab = ref('system')
const saving = ref(false)
const statusMessage = ref('')
const statusType = ref('success')

const systemConfig = ref({
  log_level: 'info',
  max_connections: 100,
  session_timeout: 3600
})

const networkConfig = ref({
  hostname: 'jetson-bmc',
  port: 8080,
  ssl_port: 8443,
  enable_ssl: false
})

const hardwareConfig = ref({
  sensor_update_interval: 1000,
  temperature_threshold_warning: 70,
  temperature_threshold_critical: 85,
  power_threshold_warning: 20,
  power_threshold_critical: 25
})

const securityConfig = ref({
  enable_auth: true,
  max_login_attempts: 5,
  csrf_protection: true,
  session_cookie_name: 'SESSION'
})

const loadAllConfig = async () => {
  try {
    const response = await axios.get(`${API_BASE_URL}/api/config`)
    const config = response.data
    
    // Parse configuration into sections
    Object.keys(config).forEach(key => {
      const value = config[key]
      
      if (key.startsWith('system.')) {
        const setting = key.replace('system.', '')
        systemConfig.value[setting] = value
      } else if (key.startsWith('network.')) {
        const setting = key.replace('network.', '')
        if (setting === 'enable_ssl') {
          networkConfig.value[setting] = value === 'true'
        } else {
          networkConfig.value[setting] = value
        }
      } else if (key.startsWith('hardware.')) {
        const setting = key.replace('hardware.', '')
        hardwareConfig.value[setting] = value
      } else if (key.startsWith('security.')) {
        const setting = key.replace('security.', '')
        if (setting === 'enable_auth' || setting === 'csrf_protection') {
          securityConfig.value[setting] = value === 'true'
        } else {
          securityConfig.value[setting] = value
        }
      }
    })
  } catch (error) {
    console.error('Failed to load configuration:', error)
    showStatus('Failed to load configuration', 'error')
  }
}

const saveSystemConfig = async () => {
  saving.value = true
  try {
    await axios.put(`${API_BASE_URL}/api/config/system`, systemConfig.value)
    showStatus('System configuration saved successfully', 'success')
  } catch (error) {
    console.error('Failed to save system configuration:', error)
    showStatus('Failed to save system configuration', 'error')
  } finally {
    saving.value = false
  }
}

const saveNetworkConfig = async () => {
  saving.value = true
  try {
    await axios.put(`${API_BASE_URL}/api/config/network`, networkConfig.value)
    showStatus('Network configuration saved successfully', 'success')
  } catch (error) {
    console.error('Failed to save network configuration:', error)
    showStatus('Failed to save network configuration', 'error')
  } finally {
    saving.value = false
  }
}

const saveHardwareConfig = async () => {
  saving.value = true
  try {
    await axios.put(`${API_BASE_URL}/api/config/hardware`, hardwareConfig.value)
    showStatus('Hardware configuration saved successfully', 'success')
  } catch (error) {
    console.error('Failed to save hardware configuration:', error)
    showStatus('Failed to save hardware configuration', 'error')
  } finally {
    saving.value = false
  }
}

const saveSecurityConfig = async () => {
  saving.value = true
  try {
    await axios.put(`${API_BASE_URL}/api/config/security`, securityConfig.value)
    showStatus('Security configuration saved successfully', 'success')
  } catch (error) {
    console.error('Failed to save security configuration:', error)
    showStatus('Failed to save security configuration', 'error')
  } finally {
    saving.value = false
  }
}

const showStatus = (message, type) => {
  statusMessage.value = message
  statusType.value = type
  setTimeout(() => {
    statusMessage.value = ''
  }, 3000)
}

onMounted(() => {
  loadAllConfig()
})
</script>

<style scoped>
.configuration {
  padding: 2rem;
}

.config-tabs {
  display: flex;
  gap: 0.5rem;
  margin-bottom: 2rem;
  border-bottom: 2px solid #e0e0e0;
  padding-bottom: 0.5rem;
}

.tab-button {
  padding: 0.75rem 1.5rem;
  border: none;
  background: none;
  cursor: pointer;
  font-size: 1rem;
  color: #666;
  border-radius: 4px 4px 0 0;
  transition: all 0.3s;
}

.tab-button:hover {
  background-color: #f5f5f5;
}

.tab-button.active {
  background-color: #2196F3;
  color: white;
}

.config-content {
  background: white;
  padding: 2rem;
  border-radius: 8px;
  box-shadow: 0 2px 4px rgba(0,0,0,0.1);
}

.config-section h3 {
  margin-bottom: 1.5rem;
  color: #333;
}

.config-form {
  display: flex;
  flex-direction: column;
  gap: 1.5rem;
}

.form-group {
  display: flex;
  flex-direction: column;
  gap: 0.5rem;
}

.form-group label {
  font-weight: 600;
  color: #555;
}

.form-control {
  padding: 0.75rem;
  border: 1px solid #ddd;
  border-radius: 4px;
  font-size: 1rem;
}

.form-control:focus {
  outline: none;
  border-color: #2196F3;
  box-shadow: 0 0 0 2px rgba(33, 150, 243, 0.1);
}

.form-checkbox {
  width: 20px;
  height: 20px;
  cursor: pointer;
}

.btn {
  padding: 0.75rem 1.5rem;
  border: none;
  border-radius: 4px;
  font-size: 1rem;
  cursor: pointer;
  transition: background-color 0.3s;
}

.btn-primary {
  background-color: #2196F3;
  color: white;
}

.btn-primary:hover:not(:disabled) {
  background-color: #1976D2;
}

.btn:disabled {
  opacity: 0.6;
  cursor: not-allowed;
}

.status-message {
  margin-top: 1rem;
  padding: 1rem;
  border-radius: 4px;
  font-weight: 500;
}

.status-message.success {
  background-color: #4CAF50;
  color: white;
}

.status-message.error {
  background-color: #f44336;
  color: white;
}
</style>
