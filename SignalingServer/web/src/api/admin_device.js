function getApiBaseUrl() {
  const apiServer = localStorage.getItem('quickdesk_api_server') || 'http://localhost:8000'
  return `${apiServer}/api/v1`
}

function getAuthHeaders() {
  const token = localStorage.getItem('quickdesk_admin_token')
  return {
    'Content-Type': 'application/json',
    'Authorization': `Bearer ${token}`
  }
}

export async function getDevices() {
  const response = await fetch(`${getApiBaseUrl()}/admin/devices`, {
    method: 'GET',
    headers: getAuthHeaders()
  })
  return response.json()
}

export async function getDeviceStatus(deviceId) {
  const response = await fetch(`${getApiBaseUrl()}/devices/${deviceId}/status`)
  return response.json()
}
