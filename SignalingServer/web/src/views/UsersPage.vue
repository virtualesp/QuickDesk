<template>
  <div class="users-page">
    <div class="page-header">
      <h2>用户管理</h2>
      <el-button type="primary" @click="handleAdd">
        <el-icon><Plus /></el-icon>
        新增用户
      </el-button>
    </div>

    <el-card class="users-card">
      <el-table
        :data="users"
        v-loading="loading"
        style="width: 100%"
        :header-cell-style="{ background: '#f5f7fa' }"
      >
        <el-table-column prop="id" label="ID" width="60" />
        <el-table-column prop="username" label="用户名" min-width="120" />
        <el-table-column prop="phone" label="手机号" min-width="120" />
        <el-table-column prop="email" label="邮箱号" min-width="180" />
        <el-table-column prop="level" label="权限等级" width="100">
          <template #default="{ row }">
            <el-tag :type="getLevelType(row.level)">{{ row.level }}</el-tag>
          </template>
        </el-table-column>
        <el-table-column prop="deviceCount" label="设备数量" width="100" />
        <el-table-column prop="channelType" label="通道类型" width="100">
          <template #default="{ row }">
            <el-tag :type="row.channelType === '全球' ? 'success' : 'warning'">
              {{ row.channelType }}
            </el-tag>
          </template>
        </el-table-column>
        <el-table-column prop="status" label="状态" width="80">
          <template #default="{ row }">
            <el-switch
              v-model="row.status"
              @change="(val) => handleStatusChange(row, val)"
            />
          </template>
        </el-table-column>
        <el-table-column label="操作" width="180" fixed="right">
          <template #default="{ row }">
            <el-button link type="primary" @click="handleEdit(row)">
              <el-icon><Edit /></el-icon>
              编辑
            </el-button>
            <el-button link type="danger" @click="handleDelete(row)">
              <el-icon><Delete /></el-icon>
              删除
            </el-button>
          </template>
        </el-table-column>
      </el-table>

      <el-empty v-if="!loading && users.length === 0" description="暂无用户数据" />
    </el-card>

    <!-- 新增/编辑对话框 -->
    <el-dialog
      v-model="dialogVisible"
      :title="isEdit ? '编辑用户' : '新增用户'"
      width="500px"
      destroy-on-close
    >
      <el-form
        ref="formRef"
        :model="form"
        :rules="rules"
        label-width="100px"
      >
        <el-form-item label="用户名" prop="username">
          <el-input v-model="form.username" placeholder="请输入用户名" :disabled="isEdit" />
        </el-form-item>
        <el-form-item label="手机号" prop="phone">
          <el-input v-model="form.phone" placeholder="请输入手机号" />
        </el-form-item>
        <el-form-item label="邮箱号" prop="email">
          <el-input v-model="form.email" placeholder="请输入邮箱号" />
        </el-form-item>
        <el-form-item label="密码" prop="password" v-if="!isEdit">
          <el-input v-model="form.password" type="password" placeholder="请输入密码" show-password />
        </el-form-item>
        <el-form-item label="密码" prop="password" v-else>
          <el-input v-model="form.password" type="password" placeholder="不修改请留空" show-password />
        </el-form-item>
        <el-form-item label="权限等级" prop="level">
          <el-select v-model="form.level" placeholder="请选择权限等级" style="width: 100%">
            <el-option label="V1" value="V1" />
            <el-option label="V2" value="V2" />
            <el-option label="V3" value="V3" />
            <el-option label="V4" value="V4" />
            <el-option label="V5" value="V5" />
          </el-select>
        </el-form-item>
        <el-form-item label="设备数量" prop="deviceCount">
          <el-input-number v-model="form.deviceCount" :min="0" style="width: 100%" />
        </el-form-item>
        <el-form-item label="通道类型" prop="channelType">
          <el-select v-model="form.channelType" placeholder="请选择通道类型" style="width: 100%">
            <el-option label="全球" value="全球" />
            <el-option label="中国大陆" value="中国大陆" />
          </el-select>
        </el-form-item>
        <el-form-item label="状态" prop="status">
          <el-switch v-model="form.status" />
        </el-form-item>
      </el-form>
      <template #footer>
        <el-button @click="dialogVisible = false">取消</el-button>
        <el-button type="primary" @click="handleSubmit" :loading="submitting">
          确定
        </el-button>
      </template>
    </el-dialog>
  </div>
</template>

<script setup>
import { ref, reactive, onMounted } from 'vue'
import { ElMessage, ElMessageBox } from 'element-plus'
import { Plus, Edit, Delete } from '@element-plus/icons-vue'
import { getUsers, createUser, updateUser, deleteUser } from '../api/users.js'

const loading = ref(false)
const users = ref([])
const dialogVisible = ref(false)
const isEdit = ref(false)
const submitting = ref(false)
const formRef = ref(null)

const form = reactive({
  id: null,
  username: '',
  phone: '',
  email: '',
  password: '',
  level: 'V1',
  deviceCount: 0,
  channelType: '全球',
  status: true
})

const rules = {
  username: [{ required: true, message: '请输入用户名', trigger: 'blur' }],
  password: [{ required: !isEdit.value, message: '请输入密码', trigger: 'blur' }]
}

async function loadUsers() {
  loading.value = true
  try {
    const data = await getUsers()
    users.value = data.users || []
  } catch (e) {
    ElMessage.error('加载用户列表失败: ' + e.message)
  } finally {
    loading.value = false
  }
}

function getLevelType(level) {
  const types = { 'V1': '', 'V2': 'success', 'V3': 'warning', 'V4': 'danger', 'V5': 'info' }
  return types[level] || ''
}

function handleAdd() {
  isEdit.value = false
  Object.assign(form, {
    id: null,
    username: '',
    phone: '',
    email: '',
    password: '',
    level: 'V1',
    deviceCount: 0,
    channelType: '全球',
    status: true
  })
  dialogVisible.value = true
}

function handleEdit(row) {
  isEdit.value = true
  Object.assign(form, {
    id: row.id,
    username: row.username,
    phone: row.phone,
    email: row.email,
    password: '',
    level: row.level,
    deviceCount: row.deviceCount,
    channelType: row.channelType,
    status: row.status
  })
  dialogVisible.value = true
}

async function handleSubmit() {
  const valid = await formRef.value?.validate().catch(() => false)
  if (!valid) return

  submitting.value = true
  try {
    if (isEdit.value) {
      await updateUser(form.id, form)
      ElMessage.success('用户更新成功')
    } else {
      await createUser(form)
      ElMessage.success('用户创建成功')
    }
    dialogVisible.value = false
    loadUsers()
  } catch (e) {
    ElMessage.error(e.message)
  } finally {
    submitting.value = false
  }
}

async function handleDelete(row) {
  try {
    await ElMessageBox.confirm('确定要删除该用户吗？', '提示', { type: 'warning' })
    await deleteUser(row.id)
    ElMessage.success('用户删除成功')
    loadUsers()
  } catch (e) {
    if (e !== 'cancel') {
      ElMessage.error('删除失败: ' + e.message)
    }
  }
}

async function handleStatusChange(row, status) {
  try {
    await updateUser(row.id, { status })
    ElMessage.success('状态更新成功')
  } catch (e) {
    ElMessage.error('状态更新失败: ' + e.message)
    row.status = !status
  }
}

onMounted(() => {
  loadUsers()
})
</script>

<style scoped>
.users-page {
  padding: 20px;
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
  color: #303133;
}

.users-card {
  min-height: 400px;
}
</style>
