import { authFetch } from './auth.js'

const PUBLIC_URL = '/api/v1/settings'
const ADMIN_URL = '/api/v1/admin/settings'

export async function getSettings() {
  const res = await fetch(PUBLIC_URL)
  if (!res.ok) throw new Error(`HTTP ${res.status}`)
  return res.json()
}

export async function updateSettings(data) {
  const res = await authFetch(ADMIN_URL, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(data)
  })
  if (!res.ok) throw new Error(`HTTP ${res.status}`)
  return res.json()
}
