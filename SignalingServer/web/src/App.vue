<template>
  <!-- Login page: full screen, no layout -->
  <router-view v-if="isLoginPage" />

  <!-- Admin layout with sidebar -->
  <el-container v-else class="app-container">
    <el-aside :width="isMobile ? '64px' : '200px'" class="app-aside">
      <div class="logo">
        <h2 v-if="!isMobile && !settingsStore.loading">{{ settingsStore.siteName }}</h2>
        <h2 v-else-if="!isMobile">&nbsp;</h2>
        <h2 v-else-if="!settingsStore.loading">{{ settingsStore.siteName.charAt(0) }}</h2>
        <h2 v-else>&nbsp;</h2>
        <span v-if="!isMobile" class="subtitle">管理后台</span>
      </div>
      <el-menu
        :default-active="activeMenu"
        router
        class="app-menu"
        :collapse="isMobile"
      >
        <el-menu-item index="/home" :route="{ path: '/home' }">
          <el-icon><House /></el-icon>
          <span v-if="!isMobile">监控面板</span>
        </el-menu-item>
        <el-menu-item index="/devices" :route="{ path: '/devices' }">
          <el-icon><Monitor /></el-icon>
          <span v-if="!isMobile">设备列表</span>
        </el-menu-item>
        <el-menu-item index="/preset" :route="{ path: '/preset' }">
          <el-icon><Setting /></el-icon>
          <span v-if="!isMobile">预设管理</span>
        </el-menu-item>
        <el-menu-item index="/users" :route="{ path: '/users' }">
          <el-icon><UserFilled /></el-icon>
          <span v-if="!isMobile">用户管理</span>
        </el-menu-item>
        <el-menu-item index="/admin-users" :route="{ path: '/admin-users' }">
          <el-icon><User /></el-icon>
          <span v-if="!isMobile">管理员账户</span>
        </el-menu-item>
        <el-menu-item index="/settings" :route="{ path: '/settings' }">
          <el-icon><Tools /></el-icon>
          <span v-if="!isMobile">系统设置</span>
        </el-menu-item>
      </el-menu>
      <div class="aside-footer">
        <el-button text class="logout-btn" @click="handleLogout">
          <el-icon><SwitchButton /></el-icon>
          <span v-if="!isMobile">退出登录</span>
        </el-button>
      </div>
    </el-aside>
    <el-container>
      <el-header class="app-header">
        <h3>{{ currentTitle }}</h3>
      </el-header>
      <el-main class="app-main">
        <router-view />
      </el-main>
    </el-container>
  </el-container>
</template>

<script setup>
import { computed, ref, onMounted, onUnmounted } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import { useSettingsStore } from './stores/settings.js'
import { logout } from './api/auth.js'
import { House, Monitor, Setting, User, UserFilled, SwitchButton, Tools } from '@element-plus/icons-vue'

const route = useRoute()
const router = useRouter()
const settingsStore = useSettingsStore()
const activeMenu = computed(() => {
  if (route.path === '/') return '/home'
  return route.path
})
const currentTitle = computed(() => route.meta?.title || '')
const isLoginPage = computed(() => ['Login', 'UserLogin', 'Register', 'UserConsole', 'RemoteConsole', 'Profile'].includes(route.name))

const isMobile = ref(window.innerWidth < 768)

function checkMobile() {
  isMobile.value = window.innerWidth < 768
}

onMounted(() => {
  window.addEventListener('resize', checkMobile)
  settingsStore.loadSettings()
})

onUnmounted(() => {
  window.removeEventListener('resize', checkMobile)
})

function handleLogout() {
  logout()
  router.push('/login')
}
</script>

<style>
html, body, #app {
  margin: 0;
  padding: 0;
  height: 100%;
  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
  overflow: hidden;
}

.app-container {
  height: 100vh;
  display: flex;
}

:deep(.el-container) {
  height: 100%;
}

.app-aside {
  background: #1d1e1f;
  border-right: 1px solid #303133;
  display: flex;
  flex-direction: column;
  transition: width 0.3s ease;
}

.logo {
  padding: 20px 16px;
  border-bottom: 1px solid #303133;
  text-align: center;
}

.logo h2 {
  margin: 0;
  color: #409eff;
  font-size: 20px;
  transition: font-size 0.3s ease;
}

.logo .subtitle {
  color: #909399;
  font-size: 12px;
  transition: opacity 0.3s ease;
}

.app-menu {
  border-right: none;
  background: transparent;
  flex: 1;
}

.app-menu .el-menu-item {
  color: #c0c4cc;
  transition: padding 0.3s ease;
}

.app-menu .el-menu-item:hover {
  background: #262727;
}

.app-menu .el-menu-item.is-active {
  color: #409eff;
  background: #262727;
}

.aside-footer {
  padding: 12px 16px;
  border-top: 1px solid #303133;
}

.logout-btn {
  color: #909399 !important;
  width: 100%;
  justify-content: flex-start;
  transition: justify-content 0.3s ease;
}

.logout-btn:hover {
  color: #c0c4cc !important;
}

.app-header {
  display: flex;
  align-items: center;
  border-bottom: 1px solid #e4e7ed;
  background: #fff;
  padding: 0 16px;
}

.app-header h3 {
  margin: 0;
  font-size: 16px;
  color: #303133;
}

.app-main {
  background: #f5f7fa;
  flex: 1;
  overflow: auto !important;
  padding: 0;
  margin: 0;
  height: calc(100vh - 60px);
}

.app-main::-webkit-scrollbar {
  width: 8px;
}

.app-main::-webkit-scrollbar-track {
  background: #f1f1f1;
  border-radius: 4px;
}

.app-main::-webkit-scrollbar-thumb {
  background: #c1c1c1;
  border-radius: 4px;
}

.app-main::-webkit-scrollbar-thumb:hover {
  background: #a8a8a8;
}

:deep(.el-main) {
  overflow: auto !important;
}

@media (max-width: 768px) {
  .app-aside {
    width: 64px !important;
  }

  .logo h2 {
    font-size: 18px;
  }

  .app-menu .el-menu-item {
    padding-left: 20px !important;
  }

  .logout-btn {
    justify-content: center;
  }

  .app-header {
    padding: 0 12px;
  }

  .app-header h3 {
    font-size: 14px;
  }
}
</style>
