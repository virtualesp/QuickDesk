<template>
  <div class="home-page" v-loading="loading">
    <div class="page-header">
      <h2>监控面板</h2>
      <el-button
        type="primary"
        size="small"
        @click="loadStats"
        :icon="Refresh"
      >
        刷新数据
      </el-button>
    </div>

    <!-- 概览卡片 -->
    <div class="overview-cards">
      <div class="overview-card purple">
        <div class="overview-icon">
          <el-icon><Monitor /></el-icon>
        </div>
        <div class="overview-content">
          <div class="overview-value">{{ overview.totalDevices }}</div>
          <div class="overview-label">总设备数</div>
          <div class="overview-desc">系统中注册的设备总数</div>
        </div>
      </div>
      <div class="overview-card blue">
        <div class="overview-icon">
          <el-icon><Connection /></el-icon>
        </div>
        <div class="overview-content">
          <div class="overview-value">{{ overview.totalConnections }}</div>
          <div class="overview-label">总连接数</div>
          <div class="overview-desc">当前活跃的连接总数</div>
        </div>
      </div>
      <div class="overview-card green">
        <div class="overview-icon">
          <el-icon><Connection /></el-icon>
        </div>
        <div class="overview-content">
          <div class="overview-value">{{ overview.webSocketConnections }}</div>
          <div class="overview-label">WebSocket连接</div>
          <div class="overview-desc">当前WebSocket连接数</div>
        </div>
      </div>
      <div class="overview-card orange">
        <div class="overview-icon">
          <el-icon><DataLine /></el-icon>
        </div>
        <div class="overview-content">
          <div class="overview-value">{{ overview.apiRequests }}</div>
          <div class="overview-label">API请求数</div>
          <div class="overview-desc">今日API请求总数</div>
        </div>
      </div>
    </div>

    <!-- 最近活动表格 -->
    <el-card class="activity-card" style="margin-top: 20px;">
      <template #header>
        <div class="card-header">
          <el-icon class="card-icon"><Timer /></el-icon>
          <span>最近活动</span>
          <el-button
            type="primary"
            size="small"
            @click="loadActivity"
            :icon="Refresh"
          >
            刷新
          </el-button>
        </div>
      </template>
      <el-table :data="activityList" stripe style="width: 100%" :row-class-name="rowClassName">
        <el-table-column prop="time" label="时间" width="180" />
        <el-table-column prop="deviceId" label="设备ID" width="120" />
        <el-table-column prop="action" label="活动" width="150" />
        <el-table-column prop="details" label="详情" show-overflow-tooltip />
        <el-table-column prop="status" label="状态" width="100">
          <template #default="{ row }">
            <el-tag :type="row.status === 'success' ? 'success' : 'warning'" size="small">
              {{ row.status === 'success' ? '成功' : '失败' }}
            </el-tag>
          </template>
        </el-table-column>
      </el-table>
      <div v-if="activityList.length === 0 && !loading" class="empty-state">
        <el-empty description="暂无活动记录" />
      </div>
    </el-card>
  </div>
</template>

<script setup>
import { ref, onMounted, onUnmounted } from 'vue'
import { ElMessage } from 'element-plus'
import { Monitor, Connection, Timer, Refresh, DataLine } from '@element-plus/icons-vue'
import { getStats, getSystemStatus, getConnectionStatus, getActivity } from '../api/stats.js'

const loading = ref(false)

const overview = ref({
  totalDevices: 0,
  totalConnections: 0,
  webSocketConnections: 0,
  apiRequests: 0
})

const stats = ref({
  totalDevices: 0,
  onlineDevices: 0,
  offlineDevices: 0,
  onlineRate: 0
})

const systemStatus = ref({
  status: 'online',
  statusText: '运行中',
  uptime: '00:00:00',
  apiVersion: 'v1',
  dbStatus: 'connected',
  dbStatusText: '已连接',
  cpu: '0%',
  memory: '0%',
  disk: '0%',
  network: '未知',
  systemVersion: '未知',
  ip: '未知',
  uploadSpeed: 0,
  downloadSpeed: 0,
  uploadTotal: 0,
  downloadTotal: 0
})

const connectionStatus = ref({
  currentConnections: 0,
  todayConnections: 0,
  webSocketConnections: 0,
  apiRequests: 0
})

const activityList = ref([
  {
    time: '2026-03-06 14:45:35',
    deviceId: '642407192',
    action: '设备登录',
    details: '设备 642407192 成功登录',
    status: 'success'
  },
  {
    time: '2026-03-06 14:40:12',
    deviceId: '123456789',
    action: '设备注册',
    details: '新设备 123456789 注册成功',
    status: 'success'
  },
  {
    time: '2026-03-06 14:35:45',
    deviceId: '987654321',
    action: '密码验证',
    details: '设备 987654321 密码验证失败',
    status: 'failed'
  }
])

function rowClassName({ row }) {
  return row.status === 'success' ? 'success-row' : 'failed-row'
}

async function loadActivity() {
  loading.value = true
  try {
    const data = await getActivity()
    activityList.value = data.activity || []
    ElMessage.success('活动数据已更新')
  } catch (e) {
    ElMessage.error('加载活动数据失败: ' + e.message)
  } finally {
    loading.value = false
  }
}

async function loadStats() {
  loading.value = true
  try {
    const [statsData, systemData, connectionData] = await Promise.all([
      getStats(),
      getSystemStatus(),
      getConnectionStatus()
    ])
    stats.value = statsData
    systemStatus.value = systemData
    connectionStatus.value = connectionData
    updateOverview(systemData)
    ElMessage.success('统计数据已更新')
  } catch (e) {
    ElMessage.error('加载统计数据失败: ' + e.message)
  } finally {
    loading.value = false
  }
}

function updateOverview(systemData) {
  overview.value.totalDevices = stats.value.totalDevices || 0
  overview.value.totalConnections = connectionStatus.value.currentConnections || 0
  overview.value.webSocketConnections = connectionStatus.value.webSocketConnections || 0
  overview.value.apiRequests = connectionStatus.value.apiRequests || 0
}

let systemStatusTimer = null

async function refreshSystemStatus() {
  try {
    const systemData = await getSystemStatus()
    systemStatus.value = systemData
    updateOverview(systemData)
  } catch (e) {
    console.error('刷新系统状态失败:', e.message)
  }
}

function startSystemStatusAutoRefresh() {
  if (systemStatusTimer) clearInterval(systemStatusTimer)
  systemStatusTimer = setInterval(refreshSystemStatus, 1000)
}

function stopSystemStatusAutoRefresh() {
  if (systemStatusTimer) {
    clearInterval(systemStatusTimer)
    systemStatusTimer = null
  }
}

onMounted(() => {
  loadStats()
  loadActivity()
  startSystemStatusAutoRefresh()
})

onUnmounted(() => {
  stopSystemStatusAutoRefresh()
})
</script>

<style scoped>
.home-page {
  width: 100%;
  padding: 20px;
  box-sizing: border-box;
  overflow: hidden;
}

.page-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 20px;
}

.page-header h2 {
  margin: 0;
  font-size: 24px;
  font-weight: 600;
  color: #303133;
}

.overview-cards {
  display: grid;
  grid-template-columns: repeat(4, 1fr);
  gap: 20px;
  margin-bottom: 20px;
}

.overview-card {
  display: flex;
  align-items: center;
  padding: 20px;
  border-radius: 12px;
  color: white;
  box-shadow: 0 4px 12px rgba(0, 0, 0, 0.1);
  transition: all 0.3s ease;
}

.overview-card:hover {
  transform: translateY(-4px);
  box-shadow: 0 8px 24px rgba(0, 0, 0, 0.15);
}

.overview-card.purple {
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
}

.overview-card.blue {
  background: linear-gradient(135deg, #4facfe 0%, #00f2fe 100%);
}

.overview-card.green {
  background: linear-gradient(135deg, #43e97b 0%, #38f9d7 100%);
}

.overview-card.orange {
  background: linear-gradient(135deg, #fa709a 0%, #fee140 100%);
}

.overview-icon {
  width: 60px;
  height: 60px;
  border-radius: 12px;
  background: rgba(255, 255, 255, 0.2);
  display: flex;
  align-items: center;
  justify-content: center;
  margin-right: 16px;
  font-size: 28px;
}

.overview-content {
  flex: 1;
}

.overview-value {
  font-size: 32px;
  font-weight: 700;
  margin-bottom: 4px;
}

.overview-label {
  font-size: 16px;
  font-weight: 500;
  margin-bottom: 4px;
  opacity: 0.95;
}

.overview-desc {
  font-size: 12px;
  opacity: 0.8;
}

.card-header {
  display: flex;
  align-items: center;
  gap: 8px;
  font-weight: 600;
}

.card-header .el-button {
  margin-left: auto;
}

.empty-state {
  padding: 40px 0;
  text-align: center;
}

@media (max-width: 1200px) {
  .overview-cards {
    grid-template-columns: repeat(2, 1fr);
  }
}

@media (max-width: 768px) {
  .overview-cards {
    grid-template-columns: 1fr;
  }

  .home-page {
    padding: 12px;
  }
}
</style>
