<template>
  <div class="streaming-page">
    <div class="page-header">
      <h1>Streaming Management</h1>
      <button @click="showCreateModal = true" class="btn btn-primary">
        <i class="icon">+</i> Add Stream
      </button>
    </div>

    <div class="streams-grid">
      <div v-for="stream in streams" :key="stream.id" class="stream-card">
        <div class="stream-header">
          <h3>{{ stream.name || stream.id }}</h3>
          <div class="stream-status">
            <span class="badge" :class="stream.streaming ? 'badge-success' : 'badge-secondary'">
              {{ stream.streaming ? 'Streaming' : 'Stopped' }}
            </span>
            <span class="badge" :class="stream.enabled ? 'badge-success' : 'badge-danger'">
              {{ stream.enabled ? 'Enabled' : 'Disabled' }}
            </span>
          </div>
        </div>
        
        <div class="stream-thumbnail" v-if="getThumbnailUrl(stream.id)">
          <img :src="getThumbnailUrl(stream.id)" :alt="stream.name" @error="handleThumbnailError" />
        </div>
        
        <div class="stream-info">
          <div class="info-row">
            <span class="label">Type:</span>
            <span class="value">{{ getStreamTypeLabel(stream.type) }}</span>
          </div>
          <div class="info-row">
            <span class="label">Source:</span>
            <span class="value">{{ stream.sourcePath }}</span>
          </div>
          <div class="info-row">
            <span class="label">Quality:</span>
            <span class="value">{{ stream.quality }}%</span>
          </div>
          <div class="info-row" v-if="stream.type !== 0">
            <span class="label">Recording:</span>
            <span class="value">{{ stream.supportsRecording ? 'Supported' : 'Not Supported' }}</span>
          </div>
        </div>
        
        <div class="stream-actions">
          <button @click="toggleStreaming(stream)" class="btn" :class="stream.streaming ? 'btn-warning' : 'btn-success'">
            <i class="icon">{{ stream.streaming ? '⏸' : '▶' }}</i>
            {{ stream.streaming ? 'Stop' : 'Start' }}
          </button>
          <button @click="viewStatistics(stream)" class="btn btn-secondary">
            <i class="icon">📊</i>
            Stats
          </button>
          <button @click="editStream(stream)" class="btn btn-secondary">
            <i class="icon">✎</i>
            Edit
          </button>
          <button @click="deleteStream(stream)" class="btn btn-danger">
            <i class="icon">✕</i>
            Delete
          </button>
        </div>
      </div>
    </div>

    <!-- Create Stream Modal -->
    <div v-if="showCreateModal" class="modal-overlay" @click.self="showCreateModal = false">
      <div class="modal">
        <div class="modal-header">
          <h2>Add Stream</h2>
          <button @click="showCreateModal = false" class="btn-close">×</button>
        </div>
        <div class="modal-body">
          <form @submit.prevent="createStream">
            <div class="form-group">
              <label>Stream ID</label>
              <input v-model="newStream.id" type="text" required placeholder="e.g., camera_1" />
            </div>
            <div class="form-group">
              <label>Name</label>
              <input v-model="newStream.name" type="text" placeholder="e.g., Main Camera" />
            </div>
            <div class="form-group">
              <label>Type</label>
              <select v-model="newStream.type" required @change="handleTypeChange">
                <option value="0">MP4 File</option>
                <option value="1">Camera Device</option>
                <option value="2">Network Stream</option>
              </select>
            </div>
            <div class="form-group">
              <label>Source Path</label>
              <input v-model="newStream.sourcePath" type="text" required 
                     :placeholder="getSourcePathPlaceholder(newStream.type)" />
            </div>
            <div class="form-group">
              <label>Quality (1-100)</label>
              <input v-model="newStream.quality" type="number" min="1" max="100" value="80" />
            </div>
            <div class="form-group" v-if="newStream.type == 0">
              <label>
                <input type="checkbox" v-model="newStream.loop" />
                Loop playback
              </label>
            </div>
            <div class="form-actions">
              <button type="button" @click="showCreateModal = false" class="btn btn-secondary">Cancel</button>
              <button type="submit" class="btn btn-primary">Create</button>
            </div>
          </form>
        </div>
      </div>
    </div>

    <!-- Edit Stream Modal -->
    <div v-if="showEditModal" class="modal-overlay" @click.self="showEditModal = false">
      <div class="modal">
        <div class="modal-header">
          <h2>Edit Stream</h2>
          <button @click="showEditModal = false" class="btn-close">×</button>
        </div>
        <div class="modal-body">
          <form @submit.prevent="updateStream">
            <div class="form-group">
              <label>Stream ID</label>
              <input v-model="editingStream.id" type="text" disabled />
            </div>
            <div class="form-group">
              <label>Name</label>
              <input v-model="editingStream.name" type="text" />
            </div>
            <div class="form-group">
              <label>Source Path</label>
              <input v-model="editingStream.sourcePath" type="text" />
            </div>
            <div class="form-group">
              <label>Quality (1-100)</label>
              <input v-model="editingStream.quality" type="number" min="1" max="100" />
            </div>
            <div class="form-group" v-if="editingStream.type == 0">
              <label>
                <input type="checkbox" v-model="editingStream.loop" />
                Loop playback
              </label>
            </div>
            <div class="form-actions">
              <button type="button" @click="showEditModal = false" class="btn btn-secondary">Cancel</button>
              <button type="submit" class="btn btn-primary">Save</button>
            </div>
          </form>
        </div>
      </div>
    </div>

    <!-- Statistics Modal -->
    <div v-if="showStatsModal" class="modal-overlay" @click.self="showStatsModal = false">
      <div class="modal">
        <div class="modal-header">
          <h2>Stream Statistics - {{ statsStream?.name || statsStream?.id }}</h2>
          <button @click="showStatsModal = false" class="btn-close">×</button>
        </div>
        <div class="modal-body">
          <div v-if="statistics" class="statistics-container">
            <div class="stat-item">
              <span class="stat-label">Bytes Served:</span>
              <span class="stat-value">{{ formatBytes(statistics.bytesServed) }}</span>
            </div>
            <div class="stat-item">
              <span class="stat-label">Frames Served:</span>
              <span class="stat-value">{{ statistics.framesServed.toLocaleString() }}</span>
            </div>
            <div class="stat-item">
              <span class="stat-label">Client Connections:</span>
              <span class="stat-value">{{ statistics.clientConnections.toLocaleString() }}</span>
            </div>
            <div class="stat-item">
              <span class="stat-label">Current Viewers:</span>
              <span class="stat-value">{{ statistics.currentViewers }}</span>
            </div>
            <div class="stat-item">
              <span class="stat-label">Average Bitrate:</span>
              <span class="stat-value">{{ formatBitrate(statistics.averageBitrate) }}</span>
            </div>
            <div class="stat-item">
              <span class="stat-label">Start Time:</span>
              <span class="stat-value">{{ formatDate(statistics.startTime) }}</span>
            </div>
            <div class="stat-item">
              <span class="stat-label">Last Frame:</span>
              <span class="stat-value">{{ formatDate(statistics.lastFrameTime) }}</span>
            </div>
          </div>
          <div v-else class="no-statistics">
            No statistics available
          </div>
          <div class="form-actions">
            <button @click="refreshStatistics" class="btn btn-secondary">Refresh</button>
            <button @click="resetStatistics" class="btn btn-warning">Reset Statistics</button>
            <button @click="showStatsModal = false" class="btn btn-primary">Close</button>
          </div>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, onMounted } from 'vue'
import { useAuthStore } from '../store/auth'

const authStore = useAuthStore()

const streams = ref([])
const statistics = ref(null)
const statsStream = ref(null)

const showCreateModal = ref(false)
const showEditModal = ref(false)
const showStatsModal = ref(false)

const newStream = ref({
  id: '',
  name: '',
  type: 0,
  sourcePath: '',
  quality: 80,
  loop: true
})

const editingStream = ref({
  id: '',
  name: '',
  sourcePath: '',
  quality: 80,
  loop: true,
  type: 0
})

const loadStreams = async () => {
  try {
    const response = await fetch('/api/streams', {
      headers: {
        'Authorization': `Token ${authStore.sessionToken}`
      }
    })
    
    if (response.ok) {
      streams.value = await response.json()
    } else {
      console.error('Failed to load streams')
    }
  } catch (error) {
    console.error('Error loading streams:', error)
  }
}

const createStream = async () => {
  try {
    const response = await fetch('/api/streams', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'Authorization': `Token ${authStore.sessionToken}`
      },
      body: JSON.stringify({
        id: newStream.value.id,
        name: newStream.value.name,
        type: parseInt(newStream.value.type),
        sourcePath: newStream.value.sourcePath,
        quality: newStream.value.quality,
        loop: newStream.value.loop
      })
    })
    
    if (response.ok) {
      showCreateModal.value = false
      newStream.value = { id: '', name: '', type: 0, sourcePath: '', quality: 80, loop: true }
      await loadStreams()
    } else {
      const error = await response.json()
      alert(error.error || 'Failed to create stream')
    }
  } catch (error) {
    console.error('Error creating stream:', error)
    alert('Failed to create stream')
  }
}

const editStream = (stream) => {
  editingStream.value = {
    id: stream.id,
    name: stream.name,
    sourcePath: stream.sourcePath,
    quality: stream.quality,
    loop: stream.loop,
    type: stream.type
  }
  showEditModal.value = true
}

const updateStream = async () => {
  try {
    const response = await fetch(`/api/streams/${editingStream.value.id}`, {
      method: 'PUT',
      headers: {
        'Content-Type': 'application/json',
        'Authorization': `Token ${authStore.sessionToken}`
      },
      body: JSON.stringify({
        name: editingStream.value.name,
        sourcePath: editingStream.value.sourcePath,
        quality: editingStream.value.quality,
        loop: editingStream.value.loop
      })
    })
    
    if (response.ok) {
      showEditModal.value = false
      await loadStreams()
    } else {
      const error = await response.json()
      alert(error.error || 'Failed to update stream')
    }
  } catch (error) {
    console.error('Error updating stream:', error)
    alert('Failed to update stream')
  }
}

const deleteStream = async (stream) => {
  if (!confirm(`Are you sure you want to delete stream ${stream.id}?`)) {
    return
  }
  
  try {
    const response = await fetch(`/api/streams/${stream.id}`, {
      method: 'DELETE',
      headers: {
        'Authorization': `Token ${authStore.sessionToken}`
      }
    })
    
    if (response.ok) {
      await loadStreams()
    } else {
      const error = await response.json()
      alert(error.error || 'Failed to delete stream')
    }
  } catch (error) {
    console.error('Error deleting stream:', error)
    alert('Failed to delete stream')
  }
}

const toggleStreaming = async (stream) => {
  const action = stream.streaming ? 'stop' : 'start'
  
  try {
    const response = await fetch(`/api/streams/${stream.id}/${action}`, {
      method: 'POST',
      headers: {
        'Authorization': `Token ${authStore.sessionToken}`
      }
    })
    
    if (response.ok) {
      await loadStreams()
    } else {
      const error = await response.json()
      alert(error.error || `Failed to ${action} streaming`)
    }
  } catch (error) {
    console.error(`Error ${action}ing streaming:`, error)
    alert(`Failed to ${action} streaming`)
  }
}

const viewStatistics = async (stream) => {
  statsStream.value = stream
  showStatsModal.value = true
  await refreshStatistics()
}

const refreshStatistics = async () => {
  if (!statsStream.value) return
  
  try {
    const response = await fetch(`/api/streams/${statsStream.value.id}/stats`, {
      headers: {
        'Authorization': `Token ${authStore.sessionToken}`
      }
    })
    
    if (response.ok) {
      statistics.value = await response.json()
    } else {
      statistics.value = null
    }
  } catch (error) {
    console.error('Error loading statistics:', error)
    statistics.value = null
  }
}

const resetStatistics = async () => {
  if (!statsStream.value) return
  
  try {
    const response = await fetch(`/api/streams/${statsStream.value.id}/stats`, {
      method: 'DELETE',
      headers: {
        'Authorization': `Token ${authStore.sessionToken}`
      }
    })
    
    if (response.ok) {
      await refreshStatistics()
    } else {
      alert('Failed to reset statistics')
    }
  } catch (error) {
    console.error('Error resetting statistics:', error)
    alert('Failed to reset statistics')
  }
}

const getThumbnailUrl = (streamId) => {
  return `/api/streams/${streamId}/thumbnail`
}

const handleThumbnailError = (event) => {
  event.target.style.display = 'none'
}

const getStreamTypeLabel = (type) => {
  const types = {
    0: 'MP4 File',
    1: 'Camera Device',
    2: 'Network Stream'
  }
  return types[type] || 'Unknown'
}

const getSourcePathPlaceholder = (type) => {
  const placeholders = {
    0: '/path/to/video.mp4',
    1: '/dev/video0',
    2: 'rtsp://camera-ip/stream'
  }
  return placeholders[type] || ''
}

const handleTypeChange = () => {
  newStream.value.sourcePath = ''
}

const formatBytes = (bytes) => {
  if (bytes === 0) return '0 B'
  const k = 1024
  const sizes = ['B', 'KB', 'MB', 'GB']
  const i = Math.floor(Math.log(bytes) / Math.log(k))
  return Math.round(bytes / Math.pow(k, i) * 100) / 100 + ' ' + sizes[i]
}

const formatBitrate = (bitrate) => {
  if (!bitrate) return '0 Mbps'
  return (bitrate / 1000000).toFixed(2) + ' Mbps'
}

const formatDate = (timestamp) => {
  if (!timestamp) return 'Never'
  const date = new Date(timestamp)
  return date.toLocaleString()
}

onMounted(() => {
  loadStreams()
})
</script>

<style scoped>
.streaming-page {
  padding: 2rem;
  max-width: 1400px;
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

.streams-grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(350px, 1fr));
  gap: 1.5rem;
}

.stream-card {
  background: white;
  border-radius: 8px;
  box-shadow: 0 2px 4px rgba(0,0,0,0.1);
  overflow: hidden;
}

.stream-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 1rem;
  border-bottom: 1px solid #eee;
}

.stream-header h3 {
  margin: 0;
  color: #333;
  font-size: 1.1rem;
}

.stream-status {
  display: flex;
  gap: 0.5rem;
}

.stream-thumbnail {
  width: 100%;
  height: 200px;
  background: #f8f9fa;
  display: flex;
  align-items: center;
  justify-content: center;
  overflow: hidden;
}

.stream-thumbnail img {
  width: 100%;
  height: 100%;
  object-fit: cover;
}

.stream-info {
  padding: 1rem;
}

.info-row {
  display: flex;
  justify-content: space-between;
  margin-bottom: 0.5rem;
  font-size: 0.9rem;
}

.info-row .label {
  color: #666;
  font-weight: 500;
}

.info-row .value {
  color: #333;
}

.stream-actions {
  display: flex;
  gap: 0.5rem;
  padding: 1rem;
  border-top: 1px solid #eee;
  flex-wrap: wrap;
}

.badge {
  padding: 0.25rem 0.5rem;
  border-radius: 4px;
  font-size: 0.75rem;
  font-weight: 500;
}

.badge-success {
  background: #28a745;
  color: white;
}

.badge-secondary {
  background: #6c757d;
  color: white;
}

.badge-danger {
  background: #dc3545;
  color: white;
}

.btn {
  padding: 0.5rem 1rem;
  border: none;
  border-radius: 4px;
  cursor: pointer;
  font-size: 0.875rem;
  transition: background 0.2s;
  display: flex;
  align-items: center;
  gap: 0.25rem;
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

.form-group input[type="checkbox"] {
  width: auto;
  margin-right: 0.5rem;
}

.form-actions {
  display: flex;
  justify-content: flex-end;
  gap: 0.5rem;
  margin-top: 1.5rem;
}

.statistics-container {
  display: flex;
  flex-direction: column;
  gap: 1rem;
}

.stat-item {
  display: flex;
  justify-content: space-between;
  padding: 0.75rem;
  background: #f8f9fa;
  border-radius: 4px;
}

.stat-label {
  font-weight: 500;
  color: #666;
}

.stat-value {
  font-weight: 600;
  color: #333;
}

.no-statistics {
  text-align: center;
  color: #666;
  padding: 2rem;
}
</style>
