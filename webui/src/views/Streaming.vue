<template>
  <div class="streaming-page">
    <div class="page-header">
      <h1>Streaming Management</h1>
      <p class="page-subtitle">Server-configured streams - start/stop available streams</p>
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
            <span class="label">Protocol:</span>
            <span class="value">{{ getProtocolLabel(stream.protocol) }}</span>
          </div>
          <div class="info-row">
            <span class="label">Quality:</span>
            <span class="value">{{ stream.quality }}%</span>
          </div>
          <div class="info-row" v-if="stream.type === 0">
            <span class="label">Loop:</span>
            <span class="value">{{ stream.loop ? 'Enabled' : 'Disabled' }}</span>
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
        </div>
      </div>
    </div>

    <div v-if="streams.length === 0" class="no-streams">
      <p>No streams configured. Configure streams via server command line.</p>
      <code>./jetson --video-file /path/to/video.mp4 --camera /dev/video0</code>
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

const showStatsModal = ref(false)

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
    const response = await fetch(`/api/streams/${statsStream.value.id}/statistics`, {
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
    const response = await fetch(`/api/streams/${statsStream.value.id}/statistics/reset`, {
      method: 'POST',
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

const getProtocolLabel = (protocol) => {
  const protocols = {
    0: 'MJPEG',
    1: 'UDP/RTP',
    2: 'RTSP',
    3: 'WebRTC',
    4: 'HLS'
  }
  return protocols[protocol] || 'Unknown'
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

.page-subtitle {
  margin: 0.5rem 0 0 0;
  color: #666;
  font-size: 0.9rem;
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

.no-streams {
  text-align: center;
  padding: 3rem;
  color: #666;
}

.no-streams p {
  margin-bottom: 1rem;
}

.no-streams code {
  background: #f8f9fa;
  padding: 0.5rem 1rem;
  border-radius: 4px;
  font-family: monospace;
  color: #333;
}
</style>
