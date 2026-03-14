import { createRouter, createWebHashHistory } from 'vue-router'
import { getToken } from '../api/auth.js'
import LoginPage from '../views/LoginPage.vue'
import HomePage from '../views/HomePage.vue'
import PresetPage from '../views/PresetPage.vue'
import DeviceListPage from '../views/DeviceListPage.vue'
import UsersPage from '../views/UsersPage.vue'
import AdminUserPage from '../views/AdminUserPage.vue'
import SettingsPage from '../views/SettingsPage.vue'

const routes = [
  { path: '/login', name: 'Login', component: LoginPage, meta: { public: true } },
  { path: '/', redirect: '/home' },
  { path: '/home', name: 'Home', component: HomePage, meta: { title: '监控面板' } },
  { path: '/preset', name: 'Preset', component: PresetPage, meta: { title: '预设管理' } },
  { path: '/devices', name: 'Devices', component: DeviceListPage, meta: { title: '设备列表' } },
  { path: '/users', name: 'Users', component: UsersPage, meta: { title: '用户管理' } },
  { path: '/admin-users', name: 'AdminUsers', component: AdminUserPage, meta: { title: '管理员账户' } },
  { path: '/settings', name: 'Settings', component: SettingsPage, meta: { title: '系统设置' } }
]

const router = createRouter({
  history: createWebHashHistory(),
  routes
})

router.beforeEach((to) => {
  if (!to.meta.public && !getToken()) {
    return '/login'
  }
})

export default router
