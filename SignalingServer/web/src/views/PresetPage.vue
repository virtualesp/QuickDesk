<template>
  <div class="preset-page" v-loading="loading">
    <el-form label-width="120px" label-position="top">

      <!-- 最低版本号 -->
      <el-card shadow="never" class="section-card">
        <template #header>
          <div class="card-header">
            <el-icon><Warning /></el-icon>
            <span>版本控制</span>
          </div>
        </template>
        <el-form-item label="最低允许版本号">
          <el-input
            v-model="form.minVersion"
            placeholder="留空表示不限制，例如 1.0.0"
            style="width: 100%; max-width: 300px"
            clearable
          />
          <div class="form-tip">低于此版本的客户端将被强制要求升级</div>
        </el-form-item>
      </el-card>

      <!-- 公告 -->
      <el-card shadow="never" class="section-card">
        <template #header>
          <div class="card-header">
            <el-icon><Bell /></el-icon>
            <span>公告管理</span>
          </div>
        </template>
        <el-tabs>
          <el-tab-pane label="中文 (zh_CN)">
            <el-form-item label="公告内容">
              <el-input
                v-model="form.notice.zh_CN"
                type="textarea"
                :rows="3"
                placeholder="支持 HTML，如：维护通知 <a href='https://...'>查看详情</a>"
                style="width: 100%"
              />
            </el-form-item>
          </el-tab-pane>
          <el-tab-pane label="English (en_US)">
            <el-form-item label="Announcement">
              <el-input
                v-model="form.notice.en_US"
                type="textarea"
                :rows="3"
                placeholder="Supports HTML, e.g.: Maintenance <a href='https://...'>Details</a>"
                style="width: 100%"
              />
            </el-form-item>
          </el-tab-pane>
        </el-tabs>
        <div class="form-tip">留空则不显示公告栏。支持 HTML 超链接标签。</div>
      </el-card>

      <!-- 导航链接 -->
      <el-card shadow="never" class="section-card">
        <template #header>
          <div class="card-header">
            <el-icon><Link /></el-icon>
            <span>导航链接</span>
          </div>
        </template>
        <el-tabs>
          <el-tab-pane label="中文 (zh_CN)">
            <LinkEditor v-model="form.links.zh_CN" />
          </el-tab-pane>
          <el-tab-pane label="English (en_US)">
            <LinkEditor v-model="form.links.en_US" />
          </el-tab-pane>
        </el-tabs>
        <div class="form-tip">显示在客户端导航栏底部，点击后在浏览器打开链接。</div>
      </el-card>

      <!-- 操作按钮 -->
      <div class="action-bar">
        <el-button type="primary" :loading="saving" @click="handleSave" size="large">
          <el-icon><Check /></el-icon>
          保存配置
        </el-button>
        <el-button @click="handleReset" size="large">
          <el-icon><RefreshLeft /></el-icon>
          重置
        </el-button>
        <span v-if="lastUpdated" class="last-updated">
          上次更新：{{ lastUpdated }}
        </span>
      </div>
    </el-form>
  </div>
</template>

<script setup>
import { ref, reactive, onMounted } from 'vue'
import { ElMessage } from 'element-plus'
import { getPreset, updatePreset } from '../api/preset.js'
import LinkEditor from './LinkEditor.vue'

const loading = ref(false)
const saving = ref(false)
const lastUpdated = ref('')

const emptyForm = () => ({
  minVersion: '',
  notice: { zh_CN: '', en_US: '' },
  links: { zh_CN: [], en_US: [] }
})

const form = reactive(emptyForm())
let serverSnapshot = null

function parseJsonField(raw, fallback) {
  if (!raw || raw === '') return fallback
  try {
    return JSON.parse(raw)
  } catch {
    return fallback
  }
}

async function loadPreset() {
  loading.value = true
  try {
    const data = await getPreset()
    const notice = parseJsonField(data.notice, { zh_CN: '', en_US: '' })
    const links = parseJsonField(data.links, { zh_CN: [], en_US: [] })

    form.minVersion = data.min_version || ''
    form.notice.zh_CN = notice.zh_CN || ''
    form.notice.en_US = notice.en_US || ''
    form.links.zh_CN = links.zh_CN || []
    form.links.en_US = links.en_US || []

    if (data.updated_at) {
      lastUpdated.value = new Date(data.updated_at).toLocaleString('zh-CN')
    }
    serverSnapshot = JSON.parse(JSON.stringify(form))
  } catch (e) {
    ElMessage.error('加载配置失败: ' + e.message)
  } finally {
    loading.value = false
  }
}

async function handleSave() {
  saving.value = true
  try {
    const payload = {
      notice: JSON.stringify(form.notice),
      links: JSON.stringify(form.links),
      min_version: form.minVersion
    }
    await updatePreset(payload)
    serverSnapshot = JSON.parse(JSON.stringify(form))
    lastUpdated.value = new Date().toLocaleString('zh-CN')
    ElMessage.success('配置已保存')
  } catch (e) {
    ElMessage.error('保存失败: ' + e.message)
  } finally {
    saving.value = false
  }
}

function handleReset() {
  if (serverSnapshot) {
    Object.assign(form, JSON.parse(JSON.stringify(serverSnapshot)))
    ElMessage.info('已重置为上次保存的配置')
  }
}

onMounted(loadPreset)
</script>

<style scoped>
.preset-page {
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

.form-tip {
  color: #909399;
  font-size: 12px;
  margin-top: 4px;
}

.action-bar {
  display: flex;
  align-items: center;
  gap: 12px;
  padding: 16px 0;
  flex-wrap: wrap;
}

.last-updated {
  color: #909399;
  font-size: 13px;
  margin-left: auto;
}

@media (max-width: 768px) {
  .preset-page {
    padding: 0 12px;
  }

  .action-bar {
    flex-direction: column;
    align-items: flex-start;
    gap: 8px;
  }

  .last-updated {
    margin-left: 0;
    font-size: 12px;
  }
}
</style>
