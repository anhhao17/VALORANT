<template>
  <div class="chart-container">
    <canvas ref="chartCanvas"></canvas>
  </div>
</template>

<script setup>
import { ref, onMounted, onUnmounted, watch } from 'vue'
import {
  Chart as ChartJS,
  CategoryScale,
  LinearScale,
  PointElement,
  LineElement,
  Title,
  Tooltip,
  Legend,
  Filler
} from 'chart.js'
import { Line } from 'vue-chartjs'

ChartJS.register(
  CategoryScale,
  LinearScale,
  PointElement,
  LineElement,
  Title,
  Tooltip,
  Legend,
  Filler
)

const props = defineProps({
  chartData: {
    type: Object,
    required: true
  },
  chartOptions: {
    type: Object,
    default: () => ({})
  },
  chartId: {
    type: String,
    default: 'default-chart'
  }
})

const chartCanvas = ref(null)
let chartInstance = null

const createChart = () => {
  if (chartCanvas.value) {
    const ctx = chartCanvas.value.getContext('2d')
    chartInstance = new ChartJS(ctx, {
      type: 'line',
      data: props.chartData,
      options: {
        responsive: true,
        maintainAspectRatio: false,
        plugins: {
          legend: {
            display: true,
            position: 'top'
          },
          title: {
            display: true,
            text: props.chartOptions.title || 'Chart'
          }
        },
        scales: {
          y: {
            beginAtZero: false,
            ...props.chartOptions.scales?.y
          },
          x: {
            ...props.chartOptions.scales?.x
          }
        },
        ...props.chartOptions
      }
    })
  }
}

const updateChart = () => {
  if (chartInstance) {
    chartInstance.data = props.chartData
    chartInstance.options = {
      ...chartInstance.options,
      ...props.chartOptions
    }
    chartInstance.update()
  }
}

watch(() => props.chartData, () => {
  updateChart()
}, { deep: true })

watch(() => props.chartOptions, () => {
  updateChart()
}, { deep: true })

onMounted(() => {
  createChart()
})

onUnmounted(() => {
  if (chartInstance) {
    chartInstance.destroy()
  }
})
</script>

<style scoped>
.chart-container {
  position: relative;
  height: 300px;
  width: 100%;
}
</style>