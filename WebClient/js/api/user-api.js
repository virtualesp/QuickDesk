// Copyright 2026 AnyControl. All rights reserved.
// User authentication and cloud device/favorite APIs for QuickDesk WebClient.

const TOKEN_KEY = 'quickdesk_user_token';
const USER_INFO_KEY = 'quickdesk_user_info';

class UserApi {
  constructor(baseUrl) {
    this._baseUrl = '';
    if (baseUrl) {
      this.setBaseUrl(baseUrl);
    }
  }

  /**
   * Set the signaling server base URL.
   * Converts ws:// to http:// and wss:// to https:// for HTTP API calls.
   * @param {string} url
   */
  setBaseUrl(url) {
    if (!url) {
      this._baseUrl = '';
      return;
    }
    let httpUrl = url.replace(/\/$/, '');
    if (httpUrl.startsWith('wss://')) {
      httpUrl = httpUrl.replace(/^wss:\/\//, 'https://');
    } else if (httpUrl.startsWith('ws://')) {
      httpUrl = httpUrl.replace(/^ws:\/\//, 'http://');
    }
    this._baseUrl = httpUrl;
  }

  // ---------------------------------------------------------------------------
  // Token / session helpers
  // ---------------------------------------------------------------------------

  getToken() {
    return localStorage.getItem(TOKEN_KEY);
  }

  isLoggedIn() {
    return !!this.getToken();
  }

  getUserInfo() {
    try {
      const raw = localStorage.getItem(USER_INFO_KEY);
      return raw ? JSON.parse(raw) : null;
    } catch {
      return null;
    }
  }

  _saveSession(token, user) {
    localStorage.setItem(TOKEN_KEY, token);
    if (user) {
      localStorage.setItem(
        USER_INFO_KEY,
        JSON.stringify({ id: user.id, username: user.username }),
      );
    }
  }

  _clearSession() {
    localStorage.removeItem(TOKEN_KEY);
    localStorage.removeItem(USER_INFO_KEY);
  }

  _authHeaders() {
    const headers = { 'Content-Type': 'application/json' };
    const token = this.getToken();
    if (token) {
      headers['Authorization'] = `Bearer ${token}`;
    }
    return headers;
  }

  // ---------------------------------------------------------------------------
  // Internal fetch wrapper — returns {ok, data, error}
  // ---------------------------------------------------------------------------

  async _request(method, path, body) {
    const url = `${this._baseUrl}${path}`;
    const opts = {
      method,
      headers: this._authHeaders(),
    };
    if (body !== undefined) {
      opts.body = JSON.stringify(body);
    }
    try {
      const resp = await fetch(url, opts);
      const data = await resp.json().catch(() => null);
      if (!resp.ok) {
        const msg =
          (data && (data.error || data.message)) ||
          `HTTP ${resp.status}`;
        return { ok: false, data, error: msg };
      }
      return { ok: true, data };
    } catch (err) {
      return { ok: false, data: null, error: err.message || String(err) };
    }
  }

  // ---------------------------------------------------------------------------
  // Feature flags
  // ---------------------------------------------------------------------------

  async fetchFeatures() {
    return this._request('GET', '/api/v1/features');
  }

  // ---------------------------------------------------------------------------
  // SMS methods
  // ---------------------------------------------------------------------------

  async sendSmsCode(phone, scene) {
    return this._request('POST', '/api/v1/sms/send', { phone, scene });
  }

  async loginWithSms(phone, smsCode) {
    const result = await this._request('POST', '/api/v1/user/login-sms', {
      phone,
      sms_code: smsCode,
    });
    if (result.ok && result.data) {
      this._saveSession(result.data.token, result.data.user);
    }
    return result;
  }

  // ---------------------------------------------------------------------------
  // Auth methods
  // ---------------------------------------------------------------------------

  async login(username, password) {
    const result = await this._request('POST', '/api/v1/user/login', {
      username,
      password,
    });
    if (result.ok && result.data) {
      this._saveSession(result.data.token, result.data.user);
    }
    return result;
  }

  async register(username, password, phone, email, smsCode) {
    const body = { username, password, phone, email };
    if (smsCode) body.sms_code = smsCode;
    return this._request('POST', '/api/v1/user/register', body);
  }

  async logout() {
    const result = await this._request('POST', '/api/v1/user/logout');
    this._clearSession();
    return result;
  }

  async fetchMe() {
    return this._request('GET', '/api/v1/user/me');
  }

  // ---------------------------------------------------------------------------
  // Device methods
  // ---------------------------------------------------------------------------

  async fetchMyDevices() {
    return this._request('GET', '/api/v1/user/devices');
  }

  async autoBindDevice(deviceId) {
    return this._request('POST', '/api/v1/user/devices/auto-bind', {
      device_id: deviceId,
    });
  }

  async unbindDevice(deviceId) {
    return this._request('POST', '/api/v1/user/devices/unbind', {
      device_id: deviceId,
    });
  }

  async setDeviceRemark(deviceId, remark) {
    return this._request(
      'PUT',
      `/api/v1/user/devices/${encodeURIComponent(deviceId)}/remark`,
      { remark },
    );
  }

  async recordConnection(deviceId, duration, status, errorMsg) {
    return this._request('POST', '/api/v1/user/devices/record', {
      device_id: deviceId,
      duration: duration || 0,
      status: status || 'success',
      error_msg: errorMsg || '',
    });
  }

  async fetchConnectionLogs() {
    return this._request('GET', '/api/v1/user/devices/logs');
  }

  // ---------------------------------------------------------------------------
  // Favorite methods
  // ---------------------------------------------------------------------------

  async fetchFavorites() {
    return this._request('GET', '/api/v1/user/favorites');
  }

  async addFavorite(deviceId, name, password) {
    return this._request('POST', '/api/v1/user/favorites', {
      device_id: deviceId,
      device_name: name,
      access_password: password,
    });
  }

  async updateFavorite(deviceId, name, password) {
    return this._request(
      'PUT',
      `/api/v1/user/favorites/${encodeURIComponent(deviceId)}`,
      { device_name: name, access_password: password },
    );
  }

  async removeFavorite(deviceId) {
    return this._request(
      'DELETE',
      `/api/v1/user/favorites/${encodeURIComponent(deviceId)}`,
    );
  }
}

// Singleton — call userApi.setBaseUrl(url) before first use.
export const userApi = new UserApi();
export { UserApi };
