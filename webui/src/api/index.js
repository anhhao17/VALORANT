import axios from 'axios'
import { useAuthStore } from '../store/auth'

const API_BASE_URL = import.meta.env.VITE_API_URL || 'http://localhost:8080'

const api = axios.create({
  baseURL: API_BASE_URL,
  headers: {
    'Content-Type': 'application/json',
  }
})

// Request interceptor to add auth token
api.interceptors.request.use(
  (config) => {
    const authStore = useAuthStore()
    if (authStore.sessionToken) {
      config.headers.Authorization = `Token ${authStore.sessionToken}`
    }
    // Add CSRF token for non-GET requests
    if (config.method !== 'get' && authStore.csrfToken) {
      config.headers['X-CSRF-Token'] = authStore.csrfToken
    }
    return config
  },
  (error) => {
    return Promise.reject(error)
  }
)

// Response interceptor to handle 401 errors
api.interceptors.response.use(
  (response) => response,
  (error) => {
    if (error.response && error.response.status === 401) {
      const authStore = useAuthStore()
      authStore.logout()
      window.location.href = '/login'
    }
    return Promise.reject(error)
  }
)

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
