<template>
  <div id="app">
    <nav v-if="showNavbar" class="navbar">
      <div class="container">
        <h1 class="logo">Jetson BMCweb</h1>
        <ul class="nav-links">
          <li><router-link to="/">Dashboard</router-link></li>
          <li><router-link to="/system">System</router-link></li>
          <li><router-link to="/hwmon">Hardware</router-link></li>
          <li><router-link to="/streaming">Streaming</router-link></li>
          <li><router-link to="/configuration">Configuration</router-link></li>
          <li><router-link to="/users">Users</router-link></li>
        </ul>
        <div v-if="authStore.isAuthenticated" class="user-info">
          <span>{{ authStore.user?.username }}</span>
          <span class="badge" :class="getRoleBadgeClass(authStore.user?.role)">{{ authStore.user?.role }}</span>
          <button @click="handleLogout" class="logout-btn">Logout</button>
        </div>
      </div>
    </nav>
    <main class="main-content">
      <router-view />
    </main>
  </div>
</template>

<script setup>
import { computed } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import { useAuthStore } from './store/auth'

const route = useRoute()
const router = useRouter()
const authStore = useAuthStore()

const showNavbar = computed(() => {
  return route.name !== 'Login'
})

const handleLogout = async () => {
  await authStore.logout()
  router.push('/login')
}

const getRoleBadgeClass = (role) => {
  return role === 'admin' ? 'badge-admin' : 'badge-user'
}
</script>

<style scoped>
.navbar {
  background-color: #1a1a1a;
  color: white;
  padding: 1rem 0;
  box-shadow: 0 2px 4px rgba(0,0,0,0.1);
}

.container {
  max-width: 1200px;
  margin: 0 auto;
  padding: 0 2rem;
  display: flex;
  justify-content: space-between;
  align-items: center;
  width: 100%;
  box-sizing: border-box;
}

.logo {
  margin: 0;
  font-size: 1.5rem;
  white-space: nowrap;
  font-weight: bold;
  letter-spacing: 0.5px;
  padding-right: 3rem;
}

.nav-links {
  list-style: none;
  display: flex;
  align-items: center;
  gap: 2rem;
  margin: 0;
  padding: 0;
}

.nav-links > li {
  display: flex;
  align-items: center;
}

.nav-links a {
  color: white;
  text-decoration: none;
  transition: color 0.3s;
  white-space: nowrap;
  font-size: 1rem;
}

.nav-links a:hover,
.nav-links a.router-link-active {
  color: #4CAF50;
}

.user-info {
  display: flex;
  align-items: center;
  gap: 1rem;
  padding-left: 2rem;
  border-left: 1px solid rgba(255,255,255,0.2);
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
  white-space: nowrap;
  font-size: 0.9rem;
}

.logout-btn:hover {
  background-color: #d32f2f;
}

.badge {
  padding: 0.25rem 0.5rem;
  border-radius: 4px;
  font-size: 0.75rem;
  font-weight: 500;
  text-transform: uppercase;
  white-space: nowrap;
}

.badge-admin {
  background-color: #dc3545;
  color: white;
}

.badge-user {
  background-color: #6c757d;
  color: white;
}

.main-content {
  max-width: 1200px;
  margin: 2rem auto;
  padding: 0 2rem;
}

/* Responsive adjustments */
@media (max-width: 768px) {
  .container {
    flex-direction: column;
    gap: 1rem;
    align-items: flex-start;
  }
  
  .logo {
    padding-right: 0;
  }
  
  .nav-links {
    flex-wrap: wrap;
    gap: 1rem;
    width: 100%;
  }
  
  .user-info {
    padding-left: 0;
    border-left: none;
    border-top: 1px solid rgba(255,255,255,0.2);
    padding-top: 1rem;
    width: 100%;
  }
}
</style>
