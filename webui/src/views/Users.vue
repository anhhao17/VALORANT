<template>
  <div class="users-page">
    <div class="page-header">
      <h1>User Management</h1>
      <button @click="showCreateModal = true" class="btn btn-primary" v-if="isAdmin">
        <i class="icon">+</i> Create User
      </button>
    </div>

    <div class="users-table-container">
      <table class="users-table">
        <thead>
          <tr>
            <th>Username</th>
            <th>Role</th>
            <th>Email</th>
            <th>Status</th>
            <th>Last Login</th>
            <th>Actions</th>
          </tr>
        </thead>
        <tbody>
          <tr v-for="user in users" :key="user.username">
            <td>{{ user.username }}</td>
            <td>
              <span class="badge" :class="getRoleBadgeClass(user.role)">
                {{ user.role }}
              </span>
            </td>
            <td>{{ user.email || '-' }}</td>
            <td>
              <span class="badge" :class="user.enabled ? 'badge-success' : 'badge-danger'">
                {{ user.enabled ? 'Active' : 'Disabled' }}
              </span>
            </td>
            <td>{{ formatDate(user.lastLoginAt) }}</td>
            <td>
              <div class="action-buttons">
                <button @click="editUser(user)" class="btn btn-sm btn-secondary" title="Edit">
                  <i class="icon">✎</i>
                </button>
                <button @click="changePassword(user)" class="btn btn-sm btn-secondary" title="Change Password">
                  <i class="icon">🔑</i>
                </button>
                <button @click="toggleUserStatus(user)" class="btn btn-sm" :class="user.enabled ? 'btn-warning' : 'btn-success'" :title="user.enabled ? 'Disable' : 'Enable'">
                  <i class="icon">{{ user.enabled ? '⊘' : '✓' }}</i>
                </button>
                <button @click="deleteUser(user)" class="btn btn-sm btn-danger" title="Delete" v-if="isAdmin && user.username !== currentUser">
                  <i class="icon">✕</i>
                </button>
              </div>
            </td>
          </tr>
        </tbody>
      </table>
    </div>

    <!-- Create User Modal -->
    <div v-if="showCreateModal" class="modal-overlay" @click.self="showCreateModal = false">
      <div class="modal">
        <div class="modal-header">
          <h2>Create User</h2>
          <button @click="showCreateModal = false" class="btn-close">×</button>
        </div>
        <div class="modal-body">
          <form @submit.prevent="createUser">
            <div class="form-group">
              <label>Username</label>
              <input v-model="newUser.username" type="text" required />
            </div>
            <div class="form-group">
              <label>Password</label>
              <input v-model="newUser.password" type="password" required />
            </div>
            <div class="form-group">
              <label>Role</label>
              <select v-model="newUser.role" required>
                <option value="user">User</option>
                <option value="admin">Admin</option>
              </select>
            </div>
            <div class="form-group">
              <label>Email</label>
              <input v-model="newUser.email" type="email" />
            </div>
            <div class="form-actions">
              <button type="button" @click="showCreateModal = false" class="btn btn-secondary">Cancel</button>
              <button type="submit" class="btn btn-primary">Create</button>
            </div>
          </form>
        </div>
      </div>
    </div>

    <!-- Edit User Modal -->
    <div v-if="showEditModal" class="modal-overlay" @click.self="showEditModal = false">
      <div class="modal">
        <div class="modal-header">
          <h2>Edit User</h2>
          <button @click="showEditModal = false" class="btn-close">×</button>
        </div>
        <div class="modal-body">
          <form @submit.prevent="updateUser">
            <div class="form-group">
              <label>Username</label>
              <input v-model="editingUser.username" type="text" disabled />
            </div>
            <div class="form-group">
              <label>Email</label>
              <input v-model="editingUser.email" type="email" />
            </div>
            <div class="form-group" v-if="isAdmin">
              <label>Role</label>
              <select v-model="editingUser.role">
                <option value="user">User</option>
                <option value="admin">Admin</option>
              </select>
            </div>
            <div class="form-actions">
              <button type="button" @click="showEditModal = false" class="btn btn-secondary">Cancel</button>
              <button type="submit" class="btn btn-primary">Save</button>
            </div>
          </form>
        </div>
      </div>
    </div>

    <!-- Change Password Modal -->
    <div v-if="showPasswordModal" class="modal-overlay" @click.self="showPasswordModal = false">
      <div class="modal">
        <div class="modal-header">
          <h2>Change Password</h2>
          <button @click="showPasswordModal = false" class="btn-close">×</button>
        </div>
        <div class="modal-body">
          <form @submit.prevent="updatePassword">
            <div class="form-group">
              <label>Username</label>
              <input v-model="passwordUser.username" type="text" disabled />
            </div>
            <div class="form-group" v-if="!isAdmin">
              <label>Current Password</label>
              <input v-model="passwordUser.oldPassword" type="password" required />
            </div>
            <div class="form-group">
              <label>New Password</label>
              <input v-model="passwordUser.newPassword" type="password" required />
            </div>
            <div class="form-group">
              <label>Confirm New Password</label>
              <input v-model="passwordUser.confirmPassword" type="password" required />
            </div>
            <div class="form-actions">
              <button type="button" @click="showPasswordModal = false" class="btn btn-secondary">Cancel</button>
              <button type="submit" class="btn btn-primary">Change Password</button>
            </div>
          </form>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, onMounted, computed } from 'vue'
import { useAuthStore } from '../store/auth'

const authStore = useAuthStore()

const users = ref([])
const currentUser = computed(() => authStore.user?.username || '')
const isAdmin = computed(() => authStore.user?.role === 'admin')

const showCreateModal = ref(false)
const showEditModal = ref(false)
const showPasswordModal = ref(false)

const newUser = ref({
  username: '',
  password: '',
  role: 'user',
  email: ''
})

const editingUser = ref({
  username: '',
  email: '',
  role: 'user'
})

const passwordUser = ref({
  username: '',
  oldPassword: '',
  newPassword: '',
  confirmPassword: ''
})

const loadUsers = async () => {
  try {
    const response = await fetch('/api/users', {
      headers: {
        'Authorization': `Token ${authStore.sessionToken}`
      }
    })
    
    if (response.ok) {
      users.value = await response.json()
    } else if (response.status === 403) {
      console.error('Access denied: Admin access required')
      // Load only current user info
      const userResponse = await fetch(`/api/users/${currentUser.value}`, {
        headers: {
          'Authorization': `Token ${authStore.sessionToken}`
        }
      })
      if (userResponse.ok) {
        const userData = await userResponse.json()
        users.value = [userData]
      }
    } else {
      console.error('Failed to load users')
    }
  } catch (error) {
    console.error('Error loading users:', error)
  }
}

const createUser = async () => {
  try {
    const response = await fetch('/api/users', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'Authorization': `Token ${authStore.sessionToken}`
      },
      body: JSON.stringify(newUser.value)
    })
    
    if (response.ok) {
      showCreateModal.value = false
      newUser.value = { username: '', password: '', role: 'user', email: '' }
      await loadUsers()
    } else {
      const error = await response.json()
      alert(error.error || 'Failed to create user')
    }
  } catch (error) {
    console.error('Error creating user:', error)
    alert('Failed to create user')
  }
}

const editUser = (user) => {
  editingUser.value = {
    username: user.username,
    email: user.email,
    role: user.role
  }
  showEditModal.value = true
}

const updateUser = async () => {
  try {
    const response = await fetch(`/api/users/${editingUser.value.username}`, {
      method: 'PUT',
      headers: {
        'Content-Type': 'application/json',
        'Authorization': `Token ${authStore.sessionToken}`
      },
      body: JSON.stringify({
        email: editingUser.value.email,
        role: editingUser.value.role
      })
    })
    
    if (response.ok) {
      showEditModal.value = false
      await loadUsers()
    } else {
      const error = await response.json()
      alert(error.error || 'Failed to update user')
    }
  } catch (error) {
    console.error('Error updating user:', error)
    alert('Failed to update user')
  }
}

const changePassword = (user) => {
  passwordUser.value = {
    username: user.username,
    oldPassword: '',
    newPassword: '',
    confirmPassword: ''
  }
  showPasswordModal.value = true
}

const updatePassword = async () => {
  if (passwordUser.value.newPassword !== passwordUser.value.confirmPassword) {
    alert('Passwords do not match')
    return
  }
  
  try {
    const response = await fetch(`/api/users/${passwordUser.value.username}/password`, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'Authorization': `Token ${authStore.sessionToken}`
      },
      body: JSON.stringify({
        oldPassword: passwordUser.value.oldPassword,
        newPassword: passwordUser.value.newPassword
      })
    })
    
    if (response.ok) {
      showPasswordModal.value = false
      alert('Password changed successfully')
    } else {
      const error = await response.json()
      alert(error.error || 'Failed to change password')
    }
  } catch (error) {
    console.error('Error changing password:', error)
    alert('Failed to change password')
  }
}

const toggleUserStatus = async (user) => {
  if (!confirm(`Are you sure you want to ${user.enabled ? 'disable' : 'enable'} user ${user.username}?`)) {
    return
  }
  
  try {
    const response = await fetch(`/api/users/${user.username}/enable`, {
      method: 'PUT',
      headers: {
        'Content-Type': 'application/json',
        'Authorization': `Token ${authStore.sessionToken}`
      },
      body: JSON.stringify({ enabled: !user.enabled })
    })
    
    if (response.ok) {
      await loadUsers()
    } else {
      const error = await response.json()
      alert(error.error || 'Failed to update user status')
    }
  } catch (error) {
    console.error('Error updating user status:', error)
    alert('Failed to update user status')
  }
}

const deleteUser = async (user) => {
  if (!confirm(`Are you sure you want to delete user ${user.username}?`)) {
    return
  }
  
  try {
    const response = await fetch(`/api/users/${user.username}`, {
      method: 'DELETE',
      headers: {
        'Authorization': `Token ${authStore.sessionToken}`
      }
    })
    
    if (response.ok) {
      await loadUsers()
    } else {
      const error = await response.json()
      alert(error.error || 'Failed to delete user')
    }
  } catch (error) {
    console.error('Error deleting user:', error)
    alert('Failed to delete user')
  }
}

const getRoleBadgeClass = (role) => {
  return role === 'admin' ? 'badge-admin' : 'badge-user'
}

const formatDate = (dateString) => {
  if (!dateString) return 'Never'
  const date = new Date(dateString)
  return date.toLocaleString()
}

onMounted(() => {
  loadUsers()
})
</script>

<style scoped>
.users-page {
  padding: 2rem;
  max-width: 1200px;
  margin: 0 auto;
}

.page-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 2rem;
}

.page-header h1 {
  margin: 0;
  color: #333;
}

.users-table-container {
  background: white;
  border-radius: 8px;
  box-shadow: 0 2px 4px rgba(0,0,0,0.1);
  overflow: hidden;
}

.users-table {
  width: 100%;
  border-collapse: collapse;
}

.users-table th,
.users-table td {
  padding: 1rem;
  text-align: left;
  border-bottom: 1px solid #eee;
}

.users-table th {
  background: #f8f9fa;
  font-weight: 600;
  color: #333;
}

.users-table tr:hover {
  background: #f8f9fa;
}

.badge {
  padding: 0.25rem 0.5rem;
  border-radius: 4px;
  font-size: 0.875rem;
  font-weight: 500;
}

.badge-admin {
  background: #dc3545;
  color: white;
}

.badge-user {
  background: #6c757d;
  color: white;
}

.badge-success {
  background: #28a745;
  color: white;
}

.badge-danger {
  background: #dc3545;
  color: white;
}

.action-buttons {
  display: flex;
  gap: 0.5rem;
}

.btn {
  padding: 0.5rem 1rem;
  border: none;
  border-radius: 4px;
  cursor: pointer;
  font-size: 0.875rem;
  transition: background 0.2s;
}

.btn-sm {
  padding: 0.25rem 0.5rem;
}

.btn-primary {
  background: #007bff;
  color: white;
}

.btn-primary:hover {
  background: #0056b3;
}

.btn-secondary {
  background: #6c757d;
  color: white;
}

.btn-secondary:hover {
  background: #545b62;
}

.btn-success {
  background: #28a745;
  color: white;
}

.btn-success:hover {
  background: #218838;
}

.btn-warning {
  background: #ffc107;
  color: #333;
}

.btn-warning:hover {
  background: #e0a800;
}

.btn-danger {
  background: #dc3545;
  color: white;
}

.btn-danger:hover {
  background: #c82333;
}

.icon {
  font-style: normal;
}

.modal-overlay {
  position: fixed;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  background: rgba(0,0,0,0.5);
  display: flex;
  align-items: center;
  justify-content: center;
  z-index: 1000;
}

.modal {
  background: white;
  border-radius: 8px;
  width: 90%;
  max-width: 500px;
  max-height: 90vh;
  overflow-y: auto;
}

.modal-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 1.5rem;
  border-bottom: 1px solid #eee;
}

.modal-header h2 {
  margin: 0;
  color: #333;
}

.btn-close {
  background: none;
  border: none;
  font-size: 1.5rem;
  cursor: pointer;
  color: #666;
}

.modal-body {
  padding: 1.5rem;
}

.form-group {
  margin-bottom: 1rem;
}

.form-group label {
  display: block;
  margin-bottom: 0.5rem;
  color: #333;
  font-weight: 500;
}

.form-group input,
.form-group select {
  width: 100%;
  padding: 0.5rem;
  border: 1px solid #ddd;
  border-radius: 4px;
  font-size: 1rem;
}

.form-group input:disabled,
.form-group select:disabled {
  background: #f8f9fa;
  color: #666;
}

.form-actions {
  display: flex;
  justify-content: flex-end;
  gap: 0.5rem;
  margin-top: 1.5rem;
}
</style>
