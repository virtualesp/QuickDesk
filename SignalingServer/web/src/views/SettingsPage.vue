<template>
  <div class="settings-page" v-loading="loading">
    <div class="page-header">
      <h2>系统设置</h2>
      <el-button
        type="primary"
        size="small"
        @click="handleSave"
        :loading="saving"
        :icon="Check"
      >
        保存设置
      </el-button>
    </div>

    <div class="settings-container">
      <!-- 基础配置 -->
      <el-card class="settings-card" shadow="never">
        <template #header>
          <div class="card-header">
            <el-icon><Setting /></el-icon>
            <span>基础配置</span>
          </div>
        </template>

        <el-form label-width="120px" label-position="top">
          <el-form-item label="站点开启">
            <el-switch
              v-model="form.siteEnabled"
              active-text="开启"
              inactive-text="关闭"
            />
            <div class="form-tip">关闭后站点将无法访问</div>
          </el-form-item>

          <el-form-item label="网站名称">
            <el-input
              v-model="form.siteName"
              placeholder="请输入网站名称"
              style="width: 100%; max-width: 400px"
            />
            <div class="form-tip">显示在浏览器标题栏和页面顶部</div>
          </el-form-item>
        </el-form>
      </el-card>
    </div>
  </div>
</template>

<script setup>
import { ref, reactive, onMounted } from 'vue'
import { ElMessage } from 'element-plus'
import { Setting, Check } from '@element-plus/icons-vue'
import { useSettingsStore } from '../stores/settings.js'
import { getSettings, updateSettings } from '../api/settings.js'

const settingsStore = useSettingsStore()
const loading = ref(false)
const saving = ref(false)

const form = reactive({
  siteEnabled: true,
  siteName: 'QuickDesk'
})

async function loadSettings() {
  loading.value = true
  try {
    const data = await getSettings()
    Object.assign(form, data)
  } catch (e) {
    ElMessage.error('加载设置失败: ' + e.message)
  } finally {
    loading.value = false
  }
}

async function handleSave() {
  saving.value = true
  try {
    await updateSettings(form)
    settingsStore.updateSettings(form)
    ElMessage.success('设置已保存')
    if (form.siteName) {
      document.title = form.siteName + ' Admin'
    }
  } catch (e) {
    ElMessage.error('保存设置失败: ' + e.message)
  } finally {
    saving.value = false
  }
}

onMounted(() => {
  loadSettings()
})
</script>

<style scoped>
.settings-page {
  width: 100%;
  padding: 20px;
  box-sizing: border-box;
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

.settings-container {
  display: flex;
  flex-direction: column;
  gap: 20px;
}

.settings-card {
  border-radius: 8px;
}

.card-header {
  display: flex;
  align-items: center;
  gap: 8px;
  font-weight: 600;
}

.form-tip {
  margin-top: 8px;
  color: #909399;
  font-size: 13px;
}

@media (max-width: 768px) {
  .settings-page {
    padding: 10px;
  }

  .page-header {
    flex-direction: column;
    align-items: flex-start;
    gap: 10px;
  }
}
</style>
