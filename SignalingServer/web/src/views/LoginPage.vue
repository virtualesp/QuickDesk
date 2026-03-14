<template>
  <div class="login-container">
    <div class="login-card">
      <div class="login-header">
        <h1 v-if="!loading">{{ siteName }}</h1>
        <h1 v-else>&nbsp;</h1>
        <p>管理后台</p>
      </div>
      <el-form
        ref="formRef"
        :model="form"
        :rules="rules"
        @submit.prevent="handleLogin"
        class="login-form"
      >
        <el-form-item prop="user">
          <el-input
            v-model="form.user"
            placeholder="用户名"
            size="large"
            :prefix-icon="User"
          />
        </el-form-item>
        <el-form-item prop="password">
          <el-input
            v-model="form.password"
            type="password"
            placeholder="密码"
            size="large"
            :prefix-icon="Lock"
            show-password
            @keyup.enter="handleLogin"
          />
        </el-form-item>
        <el-form-item>
          <el-button
            type="primary"
            size="large"
            :loading="loading"
            @click="handleLogin"
            class="login-btn"
          >
            登 录
          </el-button>
        </el-form-item>
      </el-form>
      <div class="cache-actions">
        <el-button
          text
          size="small"
          @click="clearCache"
          class="clear-cache-btn"
        >
          <el-icon><Delete /></el-icon>
          清除缓存
        </el-button>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, reactive, onMounted } from 'vue'
import { useRouter } from 'vue-router'
import { User, Lock, Delete } from '@element-plus/icons-vue'
import { ElMessage } from 'element-plus'
import { login } from '../api/auth.js'
import { getSettings } from '../api/settings.js'

const router = useRouter()
const formRef = ref(null)
const loading = ref(true)
const siteName = ref('')

const form = reactive({
  user: '',
  password: ''
})

const rules = {
  user: [{ required: true, message: '请输入用户名', trigger: 'blur' }],
  password: [{ required: true, message: '请输入密码', trigger: 'blur' }]
}

async function loadSiteName() {
  loading.value = true
  try {
    const data = await getSettings()
    if (data.siteName) {
      siteName.value = data.siteName
      document.title = data.siteName + ' Admin'
    }
  } catch (e) {
    console.error('加载网站名称失败:', e)
    siteName.value = 'QuickDesk'
  } finally {
    loading.value = false
  }
}

onMounted(() => {
  loadSiteName()
})

async function handleLogin() {
  const valid = await formRef.value?.validate().catch(() => false)
  if (!valid) return

  loading.value = true
  try {
    await login(form.user, form.password)
    ElMessage.success('登录成功')
    router.push('/home')
  } catch (e) {
    ElMessage.error('用户名或密码错误')
  } finally {
    loading.value = false
  }
}

function clearCache() {
  try {
    localStorage.clear()
    sessionStorage.clear()
    document.cookie.split(';').forEach(cookie => {
      const [name] = cookie.split('=')
      document.cookie = `${name.trim()}=; expires=Thu, 01 Jan 1970 00:00:00 UTC; path=/;`
    })
    if ('caches' in window) {
      caches.keys().then(names => {
        names.forEach(name => caches.delete(name))
      })
    }
    ElMessage.success('缓存已清除，请刷新页面')
    setTimeout(() => {
      window.location.reload(true)
    }, 1000)
  } catch (e) {
    console.error('清除缓存失败:', e)
    ElMessage.error('清除缓存失败')
  }
}
</script>

<style scoped>
.login-container {
  height: 100vh;
  display: flex;
  align-items: center;
  justify-content: center;
  background: linear-gradient(135deg, #1d1e1f 0%, #2c3e50 100%);
  padding: 0 16px;
  box-sizing: border-box;
}

.login-card {
  width: 100%;
  max-width: 400px;
  padding: 40px;
  background: #fff;
  border-radius: 12px;
  box-shadow: 0 8px 32px rgba(0, 0, 0, 0.3);
  box-sizing: border-box;
}

.login-header {
  text-align: center;
  margin-bottom: 32px;
}

.login-header h1 {
  margin: 0;
  font-size: 28px;
  color: #409eff;
}

.login-header p {
  margin: 8px 0 0;
  color: #909399;
  font-size: 14px;
}

.login-btn {
  width: 100%;
}

.cache-actions {
  margin-top: 16px;
  text-align: center;
}

.clear-cache-btn {
  color: #909399;
}

.clear-cache-btn:hover {
  color: #409eff;
}

@media (max-width: 768px) {
  .login-card {
    padding: 30px 24px;
  }

  .login-header {
    margin-bottom: 24px;
  }

  .login-header h1 {
    font-size: 24px;
  }

  .login-header p {
    font-size: 13px;
  }
}

@media (max-width: 480px) {
  .login-container {
    padding: 0 12px;
  }

  .login-card {
    padding: 24px 20px;
  }

  .login-header h1 {
    font-size: 22px;
  }
}
</style>
