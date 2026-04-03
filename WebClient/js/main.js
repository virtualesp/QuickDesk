/**
 * main.js - QuickDesk Web Client entry
 *
 * Navigation, connection history, settings, user auth, device list.
 * Remote desktop sessions open in new tabs via window.open.
 */

import { ConnectionHistory } from './storage/connection-history.js';
import { t, applyI18n, getLocale, setLocale, getSupportedLocales } from './i18n.js';
import { userApi } from './api/user-api.js';
import { userSync } from './api/user-sync.js';

class QuickDeskApp {
    constructor() {
        this._currentPage = 'remote';
        this._myDevices = [];
        this._myFavorites = [];
        this._connectionLogs = [];
    }

    init() {
        this._initLangSelector();
        applyI18n();

        this._initNavigation();
        this._initConnectForm();
        this._initSettings();
        this._initUserAuth();
        this._renderHistory();

        this._loadSavedServerUrl();
        this._applyUrlParams();
        this._restoreSession();

        window.addEventListener('message', (e) => this._onMessage(e));
    }

    // ==================== Language ====================

    _initLangSelector() {
        const select = document.getElementById('langSelect');
        if (!select) return;
        select.value = getLocale();
        select.addEventListener('change', () => {
            setLocale(select.value);
            location.reload();
        });
    }

    // ==================== Navigation ====================

    _initNavigation() {
        document.querySelectorAll('.nav-item').forEach(item => {
            item.addEventListener('click', () => {
                const page = item.dataset.page;
                if (page) this._switchPage(page);
            });
        });
    }

    _switchPage(page) {
        this._currentPage = page;

        document.querySelectorAll('.nav-item').forEach(el => {
            el.classList.toggle('active', el.dataset.page === page);
        });

        document.querySelectorAll('.page').forEach(el => {
            el.classList.toggle('active', el.id === `page-${page}`);
        });
    }

    // ==================== Connect Form ====================

    _initConnectForm() {
        const connectBtn = document.getElementById('connectBtn');
        const deviceIdInput = document.getElementById('deviceId');
        const accessCodeInput = document.getElementById('accessCode');

        connectBtn?.addEventListener('click', () => this._onConnect());

        deviceIdInput?.addEventListener('keypress', (e) => {
            if (e.key === 'Enter') accessCodeInput?.focus();
        });

        accessCodeInput?.addEventListener('keypress', (e) => {
            if (e.key === 'Enter') this._onConnect();
        });
    }

    _loadSavedServerUrl() {
        const savedUrl = localStorage.getItem('quickdesk_signaling_url');
        const serverUrlInput = document.getElementById('serverUrl');
        if (savedUrl && serverUrlInput) {
            serverUrlInput.value = savedUrl;
        }
    }

    _applyUrlParams() {
        const params = new URLSearchParams(window.location.search);
        const server = params.get('server');
        const device = params.get('device');
        const code = params.get('code');

        if (server) {
            const serverUrlInput = document.getElementById('serverUrl');
            if (serverUrlInput) serverUrlInput.value = server;
        }
        if (device) {
            const deviceIdInput = document.getElementById('deviceId');
            if (deviceIdInput) deviceIdInput.value = device;
        }
        if (code) {
            const accessCodeInput = document.getElementById('accessCode');
            if (accessCodeInput) accessCodeInput.value = code;
        }
    }

    _onConnect() {
        const serverUrl = document.getElementById('serverUrl')?.value?.trim() || 'ws://localhost:8000';
        const deviceId = document.getElementById('deviceId')?.value?.trim();
        const accessCode = document.getElementById('accessCode')?.value?.trim();

        if (!deviceId || !accessCode) {
            this._showToast(t('connect.inputRequired'), 'error');
            return;
        }

        localStorage.setItem('quickdesk_signaling_url', serverUrl);

        const videoCodec = localStorage.getItem('quickdesk_video_codec') || 'H264';
        const params = new URLSearchParams({
            server: serverUrl,
            device: deviceId,
            code: accessCode,
            codec: videoCodec,
        });

        const remoteUrl = `remote.html?${params.toString()}`;
        const isMobile = this._detectMobile();
        if (isMobile) {
            window.location.href = remoteUrl;
        } else {
            window.open(remoteUrl, `quickdesk_${deviceId}`);
        }

        this._showToast(t('connect.connecting', { deviceId }), 'info');
    }

    // ==================== User Authentication ====================

    _initUserAuth() {
        const userArea = document.getElementById('userArea');
        const loginDialog = document.getElementById('loginDialog');
        const loginSubmitBtn = document.getElementById('loginSubmitBtn');
        const loginModeSwitch = document.getElementById('loginModeSwitch');
        const loginCancelBtn = document.getElementById('loginCancelBtn');
        const userMenuLogout = document.getElementById('userMenuLogout');
        const refreshDevicesBtn = document.getElementById('refreshDevicesBtn');

        this._loginMode = 'login'; // 'login', 'register', or 'sms-login'
        this._smsEnabled = false;
        this._smsCountdown = 0;
        this._smsTimer = null;

        userArea?.addEventListener('click', (e) => {
            e.stopPropagation();
            if (userApi.isLoggedIn()) {
                this._showUserMenu();
            } else {
                this._showLoginDialog();
            }
        });

        loginSubmitBtn?.addEventListener('click', () => this._onLoginSubmit());
        loginModeSwitch?.addEventListener('click', () => this._toggleLoginMode());
        loginCancelBtn?.addEventListener('click', () => this._hideLoginDialog());
        loginDialog?.addEventListener('click', (e) => {
            if (e.target === loginDialog) this._hideLoginDialog();
        });

        userMenuLogout?.addEventListener('click', () => this._onLogout());
        refreshDevicesBtn?.addEventListener('click', () => this._refreshCloudData());

        // SMS login toggle
        document.getElementById('smsLoginSwitchBtn')?.addEventListener('click', () => {
            this._loginMode = this._loginMode === 'sms-login' ? 'login' : 'sms-login';
            this._updateLoginDialogMode();
            this._clearLoginMessages();
        });

        // SMS send buttons
        document.getElementById('smsLoginSendBtn')?.addEventListener('click', () => {
            this._sendSmsCode('smsLoginPhone', 'login');
        });
        document.getElementById('regSmsSendBtn')?.addEventListener('click', () => {
            this._sendSmsCode('loginPhone', 'register');
        });

        // Close user menu on outside click
        document.addEventListener('click', () => {
            const menu = document.getElementById('userMenuPopup');
            if (menu) menu.style.display = 'none';
        });

        // Enter key for login form
        document.getElementById('loginPassword')?.addEventListener('keypress', (e) => {
            if (e.key === 'Enter' && this._loginMode === 'login') this._onLoginSubmit();
        });
        document.getElementById('smsLoginCode')?.addEventListener('keypress', (e) => {
            if (e.key === 'Enter' && this._loginMode === 'sms-login') this._onLoginSubmit();
        });

        // Sync events
        userSync.addEventListener('devices-changed', () => this._fetchMyDevices());
        userSync.addEventListener('favorites-changed', () => this._fetchMyFavorites());
    }

    _getServerUrl() {
        return document.getElementById('serverUrl')?.value?.trim() ||
               localStorage.getItem('quickdesk_signaling_url') ||
               'ws://qdsignaling.quickcoder.cc:8000';
    }

    async _restoreSession() {
        userApi.setBaseUrl(this._getServerUrl());

        // Fetch features regardless of login state
        this._fetchFeatures();

        if (!userApi.isLoggedIn()) return;

        const result = await userApi.fetchMe();
        if (result.ok) {
            this._onLoginSuccess();
        } else {
            // Token expired
            userApi._clearSession();
            this._updateUserUI();
        }
    }

    async _fetchFeatures() {
        const result = await userApi.fetchFeatures();
        if (result.ok && result.data) {
            this._smsEnabled = !!result.data.sms_enabled;
        }
    }

    _showLoginDialog() {
        const dialog = document.getElementById('loginDialog');
        if (dialog) {
            dialog.style.display = 'flex';
            document.getElementById('loginUsername')?.focus();
        }
        this._loginMode = 'login';
        this._updateLoginDialogMode();
        this._clearLoginMessages();
    }

    _hideLoginDialog() {
        const dialog = document.getElementById('loginDialog');
        if (dialog) dialog.style.display = 'none';
        this._clearLoginMessages();
    }

    _toggleLoginMode() {
        this._loginMode = this._loginMode === 'login' || this._loginMode === 'sms-login'
            ? 'register' : 'login';
        this._updateLoginDialogMode();
        this._clearLoginMessages();
    }

    _updateLoginDialogMode() {
        const title = document.getElementById('loginDialogTitle');
        const submitBtn = document.getElementById('loginSubmitBtn');
        const switchText = document.getElementById('loginModeSwitch');
        const loginFields = document.getElementById('loginFields');
        const smsLoginFields = document.getElementById('smsLoginFields');
        const registerFields = document.getElementById('registerFields');
        const smsLoginToggle = document.getElementById('smsLoginToggle');
        const smsLoginSwitchBtn = document.getElementById('smsLoginSwitchBtn');
        const regPhoneLabel = document.getElementById('regPhoneLabel');
        const regSmsCodeGroup = document.getElementById('regSmsCodeGroup');

        // Hide all field sections first
        if (loginFields) loginFields.style.display = 'none';
        if (smsLoginFields) smsLoginFields.style.display = 'none';
        if (registerFields) registerFields.style.display = 'none';

        if (this._loginMode === 'login') {
            if (title) title.textContent = t('user.login') || '登录';
            if (submitBtn) submitBtn.textContent = t('user.login') || '登录';
            if (switchText) switchText.textContent = t('user.noAccount') || '没有账号？注册';
            if (loginFields) loginFields.style.display = '';
            // Show SMS login toggle only when SMS is enabled
            if (smsLoginToggle) smsLoginToggle.style.display = this._smsEnabled ? '' : 'none';
            if (smsLoginSwitchBtn) smsLoginSwitchBtn.textContent = t('user.smsLogin') || '短信验证码登录';
        } else if (this._loginMode === 'sms-login') {
            if (title) title.textContent = t('user.smsLogin') || '短信验证码登录';
            if (submitBtn) submitBtn.textContent = t('user.login') || '登录';
            if (switchText) switchText.textContent = t('user.noAccount') || '没有账号？注册';
            if (smsLoginFields) smsLoginFields.style.display = '';
            if (smsLoginToggle) smsLoginToggle.style.display = '';
            if (smsLoginSwitchBtn) smsLoginSwitchBtn.textContent = t('user.passwordLogin') || '账号密码登录';
        } else {
            // register
            if (title) title.textContent = t('user.register') || '注册';
            if (submitBtn) submitBtn.textContent = t('user.register') || '注册';
            if (switchText) switchText.textContent = t('user.hasAccount') || '已有账号？登录';
            if (loginFields) loginFields.style.display = '';
            if (registerFields) registerFields.style.display = '';
            if (smsLoginToggle) smsLoginToggle.style.display = 'none';

            // When SMS is enabled, phone is required and show SMS code field
            if (this._smsEnabled) {
                if (regPhoneLabel) regPhoneLabel.textContent = t('user.phone') || '手机号';
                if (regSmsCodeGroup) regSmsCodeGroup.style.display = '';
            } else {
                if (regPhoneLabel) regPhoneLabel.textContent = (t('user.phone') || '手机号') + ' (' + (t('user.optional') || '可选') + ')';
                if (regSmsCodeGroup) regSmsCodeGroup.style.display = 'none';
            }
        }
    }

    _clearLoginMessages() {
        const err = document.getElementById('loginError');
        const succ = document.getElementById('loginSuccess');
        if (err) { err.style.display = 'none'; err.textContent = ''; }
        if (succ) { succ.style.display = 'none'; succ.textContent = ''; }
    }

    _showLoginError(msg) {
        const err = document.getElementById('loginError');
        if (err) { err.style.display = 'block'; err.textContent = msg; }
    }

    _showLoginSuccess(msg) {
        const succ = document.getElementById('loginSuccess');
        if (succ) { succ.style.display = 'block'; succ.textContent = msg; }
    }

    async _onLoginSubmit() {
        userApi.setBaseUrl(this._getServerUrl());
        this._clearLoginMessages();

        if (this._loginMode === 'sms-login') {
            const phone = document.getElementById('smsLoginPhone')?.value?.trim();
            const code = document.getElementById('smsLoginCode')?.value?.trim();
            if (!phone || !code) {
                this._showLoginError(t('user.phoneCodeRequired') || '请输入手机号和验证码');
                return;
            }
            const result = await userApi.loginWithSms(phone, code);
            if (result.ok) {
                this._hideLoginDialog();
                this._onLoginSuccess();
                this._showToast(t('user.loginSuccess') || '登录成功', 'success');
            } else {
                this._showLoginError(result.error || '登录失败');
            }
            return;
        }

        const username = document.getElementById('loginUsername')?.value?.trim();
        const password = document.getElementById('loginPassword')?.value?.trim();

        if (!username || !password) {
            this._showLoginError(t('user.inputRequired') || '请输入用户名和密码');
            return;
        }

        if (this._loginMode === 'login') {
            const result = await userApi.login(username, password);
            if (result.ok) {
                this._hideLoginDialog();
                this._onLoginSuccess();
                this._showToast(t('user.loginSuccess') || '登录成功', 'success');
            } else {
                this._showLoginError(result.error || '登录失败');
            }
        } else {
            const phone = document.getElementById('loginPhone')?.value?.trim() || '';
            const email = document.getElementById('loginEmail')?.value?.trim() || '';
            const smsCode = document.getElementById('regSmsCode')?.value?.trim() || '';

            if (this._smsEnabled && (!phone || !smsCode)) {
                this._showLoginError(t('user.phoneCodeRequired') || '请输入手机号和验证码');
                return;
            }

            const result = await userApi.register(username, password, phone, email, smsCode);
            if (result.ok) {
                this._showLoginSuccess(t('user.registerSuccess') || '注册成功，请登录');
                this._loginMode = 'login';
                this._updateLoginDialogMode();
            } else {
                this._showLoginError(result.error || '注册失败');
            }
        }
    }

    async _sendSmsCode(phoneInputId, scene) {
        const phone = document.getElementById(phoneInputId)?.value?.trim();
        if (!phone) {
            this._showLoginError(t('user.phoneRequired') || '请输入手机号');
            return;
        }
        if (this._smsCountdown > 0) return;

        userApi.setBaseUrl(this._getServerUrl());
        this._clearLoginMessages();

        const result = await userApi.sendSmsCode(phone, scene);
        if (result.ok) {
            this._showLoginSuccess(t('user.codeSent') || '验证码已发送');
            this._startSmsCountdown();
        } else {
            this._showLoginError(result.error || '发送失败');
        }
    }

    _startSmsCountdown() {
        this._smsCountdown = 60;
        this._updateSmsBtnText();
        if (this._smsTimer) clearInterval(this._smsTimer);
        this._smsTimer = setInterval(() => {
            this._smsCountdown--;
            this._updateSmsBtnText();
            if (this._smsCountdown <= 0) {
                clearInterval(this._smsTimer);
                this._smsTimer = null;
            }
        }, 1000);
    }

    _updateSmsBtnText() {
        const btns = [
            document.getElementById('smsLoginSendBtn'),
            document.getElementById('regSmsSendBtn'),
        ];
        for (const btn of btns) {
            if (!btn) continue;
            if (this._smsCountdown > 0) {
                btn.textContent = `${this._smsCountdown}s`;
                btn.disabled = true;
            } else {
                btn.textContent = t('user.sendCode') || '发送验证码';
                btn.disabled = false;
            }
        }
    }

    _onLoginSuccess() {
        this._updateUserUI();
        this._updateDevicePageVisibility();

        // Start sync WebSocket
        const serverUrl = this._getServerUrl();
        const token = userApi.getToken();
        if (token) {
            userSync.connect(serverUrl, token);
        }

        // Fetch cloud data
        this._refreshCloudData();
    }

    async _onLogout() {
        document.getElementById('userMenuPopup').style.display = 'none';
        await userApi.logout();
        userSync.disconnect();
        this._myDevices = [];
        this._myFavorites = [];
        this._connectionLogs = [];
        this._updateUserUI();
        this._updateDevicePageVisibility();
        this._renderMyDevices();
        this._renderMyFavorites();
        this._renderConnectionLogs();
        this._showToast(t('user.logoutSuccess') || '已退出登录', 'info');
    }

    _showUserMenu() {
        const menu = document.getElementById('userMenuPopup');
        const userArea = document.getElementById('userArea');
        if (!menu || !userArea) return;

        const rect = userArea.getBoundingClientRect();
        menu.style.left = rect.right + 4 + 'px';
        menu.style.top = rect.top + 'px';
        menu.style.display = 'block';
    }

    _updateUserUI() {
        const loggedOut = document.getElementById('userLoggedOut');
        const loggedIn = document.getElementById('userLoggedIn');
        const avatar = document.getElementById('userAvatar');
        const displayName = document.getElementById('userDisplayName');

        if (userApi.isLoggedIn()) {
            const info = userApi.getUserInfo();
            const name = info?.username || 'User';
            if (loggedOut) loggedOut.style.display = 'none';
            if (loggedIn) loggedIn.style.display = 'flex';
            if (avatar) avatar.textContent = name.charAt(0).toUpperCase();
            if (displayName) displayName.textContent = name;
        } else {
            if (loggedOut) loggedOut.style.display = 'flex';
            if (loggedIn) loggedIn.style.display = 'none';
        }
    }

    // ==================== Device List ====================

    _updateDevicePageVisibility() {
        const notLoggedIn = document.getElementById('devicesNotLoggedIn');
        const content = document.getElementById('devicesContent');
        if (userApi.isLoggedIn()) {
            if (notLoggedIn) notLoggedIn.style.display = 'none';
            if (content) content.style.display = '';
        } else {
            if (notLoggedIn) notLoggedIn.style.display = '';
            if (content) content.style.display = 'none';
        }
    }

    async _refreshCloudData() {
        await Promise.all([this._fetchMyDevices(), this._fetchMyFavorites(), this._fetchConnectionLogs()]);
    }

    async _fetchMyDevices() {
        const result = await userApi.fetchMyDevices();
        if (result.ok && result.data) {
            this._myDevices = result.data.devices || [];
            this._renderMyDevices();
        }
    }

    async _fetchMyFavorites() {
        const result = await userApi.fetchFavorites();
        if (result.ok && result.data) {
            this._myFavorites = result.data.favorites || [];
            this._renderMyFavorites();
            // Also re-render history to update star states
            this._renderHistory();
        }
    }

    _renderMyDevices() {
        const container = document.getElementById('myDevicesList');
        if (!container) return;

        if (!this._myDevices || this._myDevices.length === 0) {
            container.innerHTML = `<div class="empty-state"><div class="empty-icon">📱</div><p>${t('devices.noDevices') || '暂无绑定设备'}</p></div>`;
            return;
        }

        container.innerHTML = '';
        for (const device of this._myDevices) {
            const item = document.createElement('div');
            item.className = 'device-item';
            const isOnline = device.online === true;
            const name = device.remark || device.device_name || t('devices.device') || '设备';
            const deviceId = device.device_id || '';

            item.innerHTML = `
                <div class="device-status ${isOnline ? 'online' : 'offline'}"></div>
                <div class="device-info">
                    <div class="device-name">${this._escapeHtml(name)}</div>
                    <div class="device-id">${this._escapeHtml(deviceId)}</div>
                </div>
                <div class="device-actions">
                    ${isOnline ? `<button class="btn btn-primary btn-sm" data-action="connect">${t('devices.connect') || '连接'}</button>` : ''}
                </div>`;

            if (isOnline) {
                item.querySelector('[data-action="connect"]')?.addEventListener('click', () => {
                    this._connectToCloudDevice(deviceId, device.access_code);
                });
            }

            // Right-click context menu (simple prompt for remark)
            item.addEventListener('contextmenu', (e) => {
                e.preventDefault();
                const newRemark = prompt(t('devices.setRemark') || '设置备注名:', name);
                if (newRemark !== null && newRemark !== name) {
                    userApi.setDeviceRemark(deviceId, newRemark).then(() => this._fetchMyDevices());
                }
            });

            container.appendChild(item);
        }
    }

    _renderMyFavorites() {
        const container = document.getElementById('myFavoritesList');
        if (!container) return;

        if (!this._myFavorites || this._myFavorites.length === 0) {
            container.innerHTML = `<div class="empty-state"><div class="empty-icon">⭐</div><p>${t('devices.noFavorites') || '暂无收藏设备'}</p></div>`;
            return;
        }

        container.innerHTML = '';
        for (const fav of this._myFavorites) {
            const item = document.createElement('div');
            item.className = 'device-item';
            const name = fav.device_name || fav.device_id || '';
            const deviceId = fav.device_id || '';

            item.innerHTML = `
                <span style="font-size:16px;flex-shrink:0;">⭐</span>
                <div class="device-info">
                    <div class="device-name">${this._escapeHtml(name)}</div>
                    <div class="device-id">${this._escapeHtml(deviceId)}</div>
                </div>
                <div class="device-actions">
                    <button class="btn btn-primary btn-sm" data-action="connect">${t('devices.connect') || '连接'}</button>
                    <button class="icon-btn danger" data-action="remove" title="${t('devices.removeFavorite') || '移除收藏'}">✕</button>
                </div>`;

            item.querySelector('[data-action="connect"]')?.addEventListener('click', () => {
                this._connectToCloudDevice(deviceId, fav.access_password);
            });

            item.querySelector('[data-action="remove"]')?.addEventListener('click', () => {
                userApi.removeFavorite(deviceId).then(() => this._fetchMyFavorites());
            });

            // Right-click to edit favorite name/password
            item.addEventListener('contextmenu', (e) => {
                e.preventDefault();
                const newName = prompt(t('devices.setRemark') || '设置备注名:', name);
                if (newName !== null && newName !== name) {
                    userApi.updateFavorite(deviceId, newName, fav.access_password || '').then(() => this._fetchMyFavorites());
                }
            });

            container.appendChild(item);
        }
    }

    _connectToCloudDevice(deviceId, accessCode) {
        if (!accessCode) {
            this._showToast(t('devices.noAccessCode') || '访问码不可用', 'error');
            return;
        }

        const serverUrl = this._getServerUrl();
        localStorage.setItem('quickdesk_signaling_url', serverUrl);

        const videoCodec = localStorage.getItem('quickdesk_video_codec') || 'H264';
        const params = new URLSearchParams({
            server: serverUrl,
            device: deviceId,
            code: accessCode,
            codec: videoCodec,
        });

        const remoteUrl = `remote.html?${params.toString()}`;
        if (this._detectMobile()) {
            window.location.href = remoteUrl;
        } else {
            window.open(remoteUrl, `quickdesk_${deviceId}`);
        }

        this._showToast(t('connect.connecting', { deviceId }) || `正在连接 ${deviceId}...`, 'info');
    }

    _isFavorite(deviceId) {
        return this._myFavorites.some(f => f.device_id === deviceId);
    }

    async _fetchConnectionLogs() {
        const result = await userApi.fetchConnectionLogs();
        if (result.ok && result.data) {
            this._connectionLogs = result.data.logs || [];
            this._renderConnectionLogs();
        }
    }

    _renderConnectionLogs() {
        const container = document.getElementById('connectionLogsList');
        if (!container) return;

        if (!this._connectionLogs || this._connectionLogs.length === 0) {
            container.innerHTML = `<div class="empty-state"><div class="empty-icon">📋</div><p>${t('devices.noLogs')}</p></div>`;
            return;
        }

        container.innerHTML = '';
        for (const log of this._connectionLogs.slice(0, 20)) {
            const item = document.createElement('div');
            item.className = 'device-item';
            const isSuccess = log.status === 'success';
            const deviceId = log.device_id || '';
            const time = log.created_at ? new Date(log.created_at).toLocaleString() : '';
            const duration = log.duration ? `${Math.floor(log.duration / 60)}m${log.duration % 60}s` : '';

            item.innerHTML = `
                <div class="device-status ${isSuccess ? 'online' : 'offline'}"></div>
                <div class="device-info">
                    <div class="device-name">${this._escapeHtml(deviceId)}</div>
                    <div class="device-id">${this._escapeHtml(time)}${duration ? ' · ' + duration : ''}${log.error_msg ? ' · ' + this._escapeHtml(log.error_msg) : ''}</div>
                </div>
                <div class="device-actions">
                    <button class="btn btn-primary btn-sm" data-action="connect">${t('devices.connect')}</button>
                </div>`;

            item.querySelector('[data-action="connect"]')?.addEventListener('click', () => {
                const deviceIdInput = document.getElementById('deviceId');
                if (deviceIdInput) deviceIdInput.value = deviceId;
                this._switchPage('remote');
                document.getElementById('accessCode')?.focus();
            });

            container.appendChild(item);
        }
    }

    // ==================== Connection History ====================

    _renderHistory() {
        const container = document.getElementById('historyList');
        if (!container) return;

        const devices = ConnectionHistory.getAll();
        if (devices.length === 0) {
            container.innerHTML = `
                <div class="empty-state">
                    <div class="empty-icon">📋</div>
                    <p>${t('history.empty')}</p>
                </div>`;
            return;
        }

        const isLoggedIn = userApi.isLoggedIn();
        container.innerHTML = '';
        for (const device of devices) {
            const isFav = isLoggedIn && this._isFavorite(device.deviceId);
            const item = document.createElement('div');
            item.className = 'history-item';
            item.innerHTML = `
                <div class="history-icon">🖥️</div>
                <div class="history-info">
                    <div class="history-device">${this._escapeHtml(device.deviceId)}</div>
                    <div class="history-meta">
                        ${ConnectionHistory.formatTime(device.lastConnected)}
                        · ${t('history.connectCount', { count: device.connectCount || 1 })}
                    </div>
                </div>
                <div class="history-actions">
                    ${isLoggedIn ? `<button class="fav-star" data-action="fav" title="${isFav ? (t('devices.removeFavorite') || '移除收藏') : (t('devices.addFavorite') || '添加收藏')}">${isFav ? '⭐' : '☆'}</button>` : ''}
                    <button class="icon-btn" data-action="fill" title="${t('history.fill')}">↗</button>
                    <button class="icon-btn danger" data-action="delete" title="${t('history.delete')}">✕</button>
                </div>`;

            item.querySelector('[data-action="fill"]').addEventListener('click', (e) => {
                e.stopPropagation();
                this._fillFromHistory(device);
            });

            item.querySelector('[data-action="delete"]').addEventListener('click', (e) => {
                e.stopPropagation();
                ConnectionHistory.remove(device.deviceId);
                this._renderHistory();
                this._showToast(t('history.deleted'), 'info');
            });

            const favBtn = item.querySelector('[data-action="fav"]');
            if (favBtn) {
                favBtn.addEventListener('click', (e) => {
                    e.stopPropagation();
                    if (this._isFavorite(device.deviceId)) {
                        userApi.removeFavorite(device.deviceId).then(() => this._fetchMyFavorites());
                    } else {
                        const didInput = document.getElementById('deviceId');
                        const codeInput = document.getElementById('accessCode');
                        const pwd = (didInput && didInput.value === device.deviceId && codeInput) ? codeInput.value : '';
                        userApi.addFavorite(device.deviceId, '', pwd).then(() => this._fetchMyFavorites());
                    }
                });
            }

            item.addEventListener('click', () => this._fillFromHistory(device));

            container.appendChild(item);
        }
    }

    _fillFromHistory(device) {
        const deviceIdInput = document.getElementById('deviceId');
        const serverUrlInput = document.getElementById('serverUrl');
        const accessCodeInput = document.getElementById('accessCode');

        if (deviceIdInput) deviceIdInput.value = device.deviceId;
        if (serverUrlInput && device.serverUrl) serverUrlInput.value = device.serverUrl;
        if (accessCodeInput) {
            accessCodeInput.value = '';
            accessCodeInput.focus();
        }
    }

    // ==================== Settings ====================

    _initSettings() {
        const settingsServerUrl = document.getElementById('settingsServerUrl');
        const connectServerUrl = document.getElementById('serverUrl');
        if (settingsServerUrl) {
            const saved = localStorage.getItem('quickdesk_signaling_url') || '';
            settingsServerUrl.value = saved || (connectServerUrl?.value || '');
            settingsServerUrl.addEventListener('change', () => {
                const url = settingsServerUrl.value.trim();
                if (url) {
                    localStorage.setItem('quickdesk_signaling_url', url);
                    if (connectServerUrl) connectServerUrl.value = url;
                    this._showToast(t('settings.signalingServerSaved'), 'info');
                }
            });
        }

        const codecSelect = document.getElementById('videoCodecSelect');
        if (codecSelect) {
            const saved = localStorage.getItem('quickdesk_video_codec') || 'AV1';
            codecSelect.value = saved;
            codecSelect.addEventListener('change', () => {
                localStorage.setItem('quickdesk_video_codec', codecSelect.value);
                this._showToast(t('settings.codecChanged', { codec: codecSelect.value }), 'info');
            });
        }
    }

    // ==================== Messages from remote tabs ====================

    _onMessage(event) {
        if (!event.data || event.data.type !== 'quickdesk-connected') return;

        const { deviceId, serverUrl } = event.data;
        if (deviceId) {
            ConnectionHistory.save(deviceId, serverUrl);
            this._renderHistory();
        }
    }

    // ==================== Device Detection ====================

    _detectMobile() {
        const ua = navigator.userAgent || '';
        if (/Android|iPhone|iPad|iPod/i.test(ua)) {
            return true;
        }
        if (navigator.platform === 'MacIntel' && navigator.maxTouchPoints > 1) {
            return true;
        }
        if (window.matchMedia) {
            const coarse = window.matchMedia('(pointer: coarse)').matches;
            const fine = window.matchMedia('(any-pointer: fine)').matches;
            if (coarse && !fine) {
                return true;
            }
        }
        if (navigator.maxTouchPoints > 0 && window.innerWidth <= 768) {
            return true;
        }
        return false;
    }

    // ==================== Utils ====================

    _showToast(message, type = 'info') {
        const toast = document.getElementById('toast');
        if (!toast) return;

        toast.textContent = message;
        toast.className = `toast ${type} show`;

        clearTimeout(this._toastTimer);
        this._toastTimer = setTimeout(() => {
            toast.classList.remove('show');
        }, 3000);
    }

    _escapeHtml(str) {
        const div = document.createElement('div');
        div.textContent = str;
        return div.innerHTML;
    }
}

// ==================== App Startup ====================

document.addEventListener('DOMContentLoaded', () => {
    const app = new QuickDeskApp();
    app.init();
    window.quickdeskApp = app;
});
