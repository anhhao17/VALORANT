import { createRouter, createWebHistory } from 'vue-router'
import Dashboard from '../views/Dashboard.vue'
import System from '../views/System.vue'
import Hardware from '../views/Hardware.vue'

const routes = [
  {
    path: '/',
    name: 'Dashboard',
    component: Dashboard
  },
  {
    path: '/system',
    name: 'System',
    component: System
  },
  {
    path: '/hwmon',
    name: 'Hardware',
    component: Hardware
  }
]

const router = createRouter({
  history: createWebHistory(),
  routes
})

export default router
