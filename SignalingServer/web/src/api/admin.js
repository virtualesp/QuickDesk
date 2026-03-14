import { authFetch } from './auth.js'

const BASE_URL = '/api/v1/admin'

export async function getAdminUsers() {
  const res = await authFetch(`${BASE_URL}/users`)
  if (!res.ok) throw new Error(`HTTP ${res.status}`)
  return res.json()
}

export async function createAdminUser(userData) {
  const res = await authFetch(`${BASE_URL}/users`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(userData)
  })
  if (!res.ok) {
    const error = await res.json()
    throw new Error(error.error || `HTTP ${res.status}`)
  }
  return res.json()
}

export async function updateAdminUser(id, userData) {
  const res = await authFetch(`${BASE_URL}/users/${id}`, {
    method: 'PUT',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(userData)
  })
  if (!res.ok) {
    const error = await res.json()
    throw new Error(error.error || `HTTP ${res.status}`)
  }
  return res.json()
}

export async function deleteAdminUser(id) {
  const res = await authFetch(`${BASE_URL}/users/${id}`, {
    method: 'DELETE'
  })
  if (!res.ok) throw new Error(`HTTP ${res.status}`)
  return res.json()
}
