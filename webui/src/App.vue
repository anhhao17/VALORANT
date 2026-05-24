<template>
  <div id="app">
    <nav v-if="showNavbar" class="navbar">
      <div class="container">
        <h1 class="logo">Jetson BMCweb</h1>
        <ul class="nav-links">
          <li><router-link to="/">Dashboard</router-link></li>
          <li><router-link to="/system">System</router-link></li>
          <li><router-link to="/hwmon">Hardware</router-link></li>
          <li v-if="authStore.isAuthenticated" class="user-info">
            <span>{{ authStore.user }}</span>
            <button @click="handleLogout" class="logout-btn">Logout</button>
          </li>
        </ul>
      </div>
    </nav>
    <main class="main-content">
      <router-view />
    </main>
  </div>
</template>

<script setup>
import { computed } from 'vue'
import { useRoute } from 'vue-router'
import { useAuthStore } from './store/auth'

const route = useRoute()
const authStore = useAuthStore()

const showNavbar = computed(() => {
  return route.name !== 'Login'
})

const handleLogout = async () => {
  await authStore.logout()
}
</script>

<style scoped>
.navbar {
  background-color: #1a1a1a;
  color: white;
  padding: 1rem 0;
}

.container {
  max-width: 1200px;
  margin: 0 auto;
  padding: 0 2rem;
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.logo {
  margin: 0;
  font-size: 1.5rem;
}

.nav-links {
  list-style: none;
  display: flex;
  gap: 2rem;
  margin: 0;
  padding: 0;
}

.nav-links a {
  color: white;
  text-decoration: none;
  transition: color 0.3s;
}

.nav-links a:hover {
  color: #4CAF50;
}

.user-info {
  display: flex;
  align-items: center;
  gap: 1rem;
  margin-left: auto;
}

.user-info span {
  color: white;
  font-weight: bold;
}

.logout-btn {
  padding: 0.5rem 1rem;
  background-color: #f44336;
  color: white;
  border: none;
  border-radius: 4px;
  cursor: pointer;
  transition: background-color 0.3s;
}

.logout-btn:hover {
  background-color: #d32f2f;
}

.main-content {
  max-width: 1200px;
  margin: 2rem auto;
  padding: 0 2rem;
}
</style>
