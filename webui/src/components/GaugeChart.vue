<template>
  <div class="gauge-container">
    <div class="gauge">
      <div class="gauge-value" :style="{ color: valueColor }">
        {{ value }}{{ unit }}
      </div>
      <div class="gauge-label">{{ label }}</div>
      <div class="gauge-bar">
        <div 
          class="gauge-fill" 
          :style="{ 
            width: percentage + '%',
            backgroundColor: valueColor 
          }"
        ></div>
      </div>
      <div class="gauge-min-max">
        <span>{{ min }}{{ unit }}</span>
        <span>{{ max }}{{ unit }}</span>
      </div>
    </div>
  </div>
</template>

<script setup>
import { computed } from 'vue'

const props = defineProps({
  value: {
    type: Number,
    required: true
  },
  min: {
    type: Number,
    default: 0
  },
  max: {
    type: Number,
    default: 100
  },
  unit: {
    type: String,
    default: ''
  },
  label: {
    type: String,
    default: 'Value'
  },
  thresholds: {
    type: Object,
    default: () => ({
      warning: 70,
      critical: 90
    })
  }
})

const percentage = computed(() => {
  const range = props.max - props.min
  if (range === 0) return 0
  return ((props.value - props.min) / range) * 100
})

const valueColor = computed(() => {
  if (props.value >= props.thresholds.critical) {
    return '#f44336' // Red for critical
  } else if (props.value >= props.thresholds.warning) {
    return '#ff9800' // Orange for warning
  } else {
    return '#4CAF50' // Green for normal
  }
})
</script>

<style scoped>
.gauge-container {
  display: flex;
  justify-content: center;
  align-items: center;
  padding: 1rem;
}

.gauge {
  width: 100%;
  max-width: 300px;
  text-align: center;
}

.gauge-value {
  font-size: 2.5rem;
  font-weight: bold;
  margin-bottom: 0.5rem;
  transition: color 0.3s;
}

.gauge-label {
  font-size: 1rem;
  color: #666;
  margin-bottom: 1rem;
}

.gauge-bar {
  height: 20px;
  background-color: #e0e0e0;
  border-radius: 10px;
  overflow: hidden;
  margin-bottom: 0.5rem;
}

.gauge-fill {
  height: 100%;
  transition: width 0.5s ease-out, background-color 0.3s;
  border-radius: 10px;
}

.gauge-min-max {
  display: flex;
  justify-content: space-between;
  font-size: 0.875rem;
  color: #999;
}
</style>