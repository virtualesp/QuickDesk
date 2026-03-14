<template>
  <div class="device-list-page" v-loading="loading">
    <el-card shadow="never" class="section-card">
      <template #header>
        <div class="card-header">
          <el-icon><Monitor /></el-icon>
          <span>设备列表</span>
          <el-button
            type="primary"
            size="small"
            @click="loadDevices"
            :icon="Refresh"
          >
            刷新
          </el-button>
        </div>
      </template>

      <div class="table-container">
        <el-table :data="devices" stripe style="width: 100%" size="small">
          <el-table-column prop="device_id" label="设备ID" min-width="100" />
          <el-table-column prop="device_uuid" label="设备UUID" min-width="180" show-overflow-tooltip />
          <el-table-column label="系统信息" min-width="150">
            <template #default="{ row }">
              <div>{{ row.os }} {{ row.os_version }}</div>
            </template>
          </el-table-column>
          <el-table-column prop="app_version" label="应用版本" min-width="100" />
          <el-table-column label="在线状态" min-width="80">
            <template #default="{ row }">
              <el-tag :type="row.online ? 'success' : 'info'" size="small">
                {{ row.online ? '在线' : '离线' }}
              </el-tag>
            </template>
          </el-table-column>
          <el-table-column label="最后活跃" min-width="160">
            <template #default="{ row }">
              {{ formatDate(row.last_seen) }}
            </template>
          </el-table-column>
          <el-table-column label="注册时间" min-width="160">
            <template #default="{ row }">
              {{ formatDate(row.created_at) }}
            </template>
          </el-table-column>
        </el-table>
      </div>

      <div v-if="devices.length === 0 && !loading" class="empty-state">
        <el-empty description="暂无设备" />
      </div>
    </el-card>
  </div>
</template>

<script setup>
import { ref, onMounted } from 'vue'
import { ElMessage } from 'element-plus'
import { getDevices } from '../api/admin_device.js'
import { Monitor, Refresh } from '@element-plus/icons-vue'

const loading = ref(false)
const devices = ref([])

function formatDate(dateStr) {
  if (!dateStr) return '-'
  try {
    return new Date(dateStr).toLocaleString('zh-CN')
  } catch {
    return dateStr
  }
}

async function loadDevices() {
  loading.value = true
  try {
    const data = await getDevices()
    devices.value = data.devices || []
  } catch (e) {
    ElMessage.error('加载设备列表失败: ' + e.message)
  } finally {
    loading.value = false
  }
}

onMounted(loadDevices)
</script>

<style scoped>
.device-list-page {
  width: 100%;
  padding: 0 16px;
  box-sizing: border-box;
}

.section-card {
  margin-bottom: 20px;
  width: 100%;
}

.card-header {
  display: flex;
  align-items: center;
  gap: 8px;
  font-weight: 600;
  flex-wrap: wrap;
}

.card-header .el-button {
  margin-left: auto;
  margin-top: 8px;
}

.table-container {
  overflow-x: hidden;
  width: 100%;
}

.empty-state {
  padding: 40px 0;
  text-align: center;
}

@media (max-width: 768px) {
  .device-list-page {
    padding: 0 12px;
  }

  .card-header {
    flex-direction: column;
    align-items: flex-start;
    gap: 12px;
  }

  .card-header .el-button {
    margin-left: 0;
    margin-top: 0;
    align-self: flex-start;
  }
}
</style>
