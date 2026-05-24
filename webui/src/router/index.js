import { createRouter, createWebHistory } from 'vue-router'
import { useAuthStore } from '../store/auth'
import Dashboard from '../views/Dashboard.vue'
import System from '../views/System.vue'
import Hardware from '../views/Hardware.vue'
import Login from '../views/Login.vue'

const routes = [
  {
    path: '/login',
    name: 'Login',
    component: Login
  },
  {
    path: '/',
    name: 'Dashboard',
    component: Dashboard,
    meta: { requiresAuth: true }
  },
  {
    path: '/system',
    name: 'System',
    component: System,
    meta: { requiresAuth: true }
  },
  {
    path: '/hwmon',
    name: 'Hardware',
    component: Hardware,
    meta: { requiresAuth: true }
  }
]

const router = createRouter({
  history: createWebHistory(),
  routes
})

// Navigation guard for authentication
router.beforeEach((to, from, next) => {
  const authStore = useAuthStore()
  
  if (to.meta.requiresAuth && !authStore.isAuthenticated) {
    // Store the intended destination for redirect after login
    next({ name: 'Login', query: { redirect: to.fullPath } })
  } else if (to.name === 'Login' && authStore.isAuthenticated) {
    // Redirect to dashboard if already logged in
    next({ name: 'Dashboard' })
  } else {
    next()
  }
})

export default router
