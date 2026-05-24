import { defineStore } from 'pinia'
import { ref } from 'vue'

const API_BASE_URL = import.meta.env.VITE_API_URL || 'http://localhost:8080'

export const useAuthStore = defineStore('auth', () => {
  const isAuthenticated = ref(false)
  const user = ref(null)
  const sessionToken = ref('')
  const csrfToken = ref('')

  function login(username, password) {
    return fetch(`${API_BASE_URL}/api/login`, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
      },
      body: JSON.stringify({ username, password }),
    })
      .then(response => {
        if (!response.ok) {
          throw new Error('Login failed')
        }
        return response.json()
      })
      .then(data => {
        sessionToken.value = data.sessionToken
        csrfToken.value = data.csrfToken
        user.value = {
          username: data.username,
          role: data.role || 'user' // Default to user if role not provided
        }
        isAuthenticated.value = true
        return data
      })
  }

  function logout() {
    return fetch(`${API_BASE_URL}/api/logout`, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'Authorization': `Token ${sessionToken.value}`,
      },
    })
      .then(response => {
        if (!response.ok) {
          throw new Error('Logout failed')
        }
        return response.json()
      })
      .then(() => {
        sessionToken.value = ''
        csrfToken.value = ''
        user.value = null
        isAuthenticated.value = false
      })
      .catch((error) => {
        // Even if logout API fails, clear local auth state
        console.error('Logout error:', error)
        sessionToken.value = ''
        csrfToken.value = ''
        user.value = null
        isAuthenticated.value = false
      })
  }

  function checkSession() {
    return fetch(`${API_BASE_URL}/api/session`, {
      method: 'GET',
      headers: {
        'Authorization': `Token ${sessionToken.value}`,
      },
    })
      .then(response => {
        if (!response.ok) {
          // Session is invalid, clear auth state
          sessionToken.value = ''
          csrfToken.value = ''
          user.value = null
          isAuthenticated.value = false
          throw new Error('Session invalid')
        }
        return response.json()
      })
      .then(data => {
        user.value = {
          username: data.username,
          role: data.role || 'user'
        }
        isAuthenticated.value = true
        return data
      })
  }

  return {
    isAuthenticated,
    user,
    sessionToken,
    csrfToken,
    login,
    logout,
    checkSession,
  }
})
