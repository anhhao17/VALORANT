import axios from 'axios'

const API_BASE_URL = import.meta.env.VITE_API_URL || 'http://localhost:8080'

const api = axios.create({
  baseURL: API_BASE_URL,
  headers: {
    'Content-Type': 'application/json',
    'Authorization': 'Basic YWRtaW46cGFzc3dvcmQ=' // admin:password in base64
  }
})

// System API
export const systemApi = {
  getInfo: () => api.get('/api/system/info'),
  getStatus: () => api.get('/api/system/status'),
  reboot: () => api.post('/api/system/reboot')
}

// Hardware Monitoring API
export const hwmonApi = {
  getTemperature: () => api.get('/api/hwmon/temperature'),
  getPower: () => api.get('/api/hwmon/power'),
  getFans: () => api.get('/api/hwmon/fans'),
  getVoltage: () => api.get('/api/hwmon/voltage')
}

// Test API
export const testApi = {
  test: () => api.get('/api/test')
}

export default api
