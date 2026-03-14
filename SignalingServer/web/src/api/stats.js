import { authFetch } from './auth.js'

const BASE_URL = '/api/v1/admin'

export async function getStats() {
  const res = await authFetch(`${BASE_URL}/stats`)
  if (!res.ok) throw new Error(`HTTP ${res.status}`)
  return res.json()
}

export async function getSystemStatus() {
  const res = await authFetch(`${BASE_URL}/system/status`)
  if (!res.ok) throw new Error(`HTTP ${res.status}`)
  return res.json()
}

export async function getConnectionStatus() {
  const res = await authFetch(`${BASE_URL}/connections`)
  if (!res.ok) throw new Error(`HTTP ${res.status}`)
  return res.json()
}

export async function getActivity() {
  const res = await authFetch(`${BASE_URL}/activity`)
  if (!res.ok) throw new Error(`HTTP ${res.status}`)
  return res.json()
}
