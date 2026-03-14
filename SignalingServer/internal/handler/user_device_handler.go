package handler

import (
	"net/http"
	"quickdesk/signaling/internal/models"
	"time"

	"github.com/gin-gonic/gin"
	"gorm.io/gorm"
)

// UserDeviceHandler manages user-device binding operations.
type UserDeviceHandler struct {
	db *gorm.DB
}

// NewUserDeviceHandler creates a new UserDeviceHandler.
func NewUserDeviceHandler(db *gorm.DB) *UserDeviceHandler {
	return &UserDeviceHandler{db: db}
}

// BindDevice handles POST /api/v1/user/devices/bind
// Creates or updates a user-device binding after a successful connection.
func (h *UserDeviceHandler) BindDevice(c *gin.Context) {
	var req struct {
		DeviceID   string `json:"device_id" binding:"required"`
		DeviceName string `json:"device_name"`
		BindType   string `json:"bind_type"`
	}
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}
	userIDVal, _ := c.Get("authed_user_id")
	authedUserID := userIDVal.(uint)

	var user models.User
	if result := h.db.First(&user, authedUserID); result.Error != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "用户不存在"})
		return
	}

	var device models.Device
	if result := h.db.Where("device_id = ?", req.DeviceID).First(&device); result.Error != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "设备不存在"})
		return
	}

	var existing models.UserDevice
	result := h.db.Where("user_id = ? AND device_id = ? AND status = ?", authedUserID, req.DeviceID, true).First(&existing)
	if result.Error == nil {
		existing.LastConnect = time.Now()
		existing.ConnectCount++
		h.db.Save(&existing)
		h.logConnection(authedUserID, req.DeviceID, existing.DeviceName, "success", "", c.ClientIP())
		c.JSON(http.StatusOK, gin.H{"message": "设备已绑定，更新连接记录", "binding": existing})
		return
	}

	bindType := req.BindType
	if bindType == "" {
		bindType = "auto"
	}
	deviceName := req.DeviceName
	if deviceName == "" {
		deviceName = "设备-" + req.DeviceID
	}

	binding := models.UserDevice{
		UserID:       authedUserID,
		DeviceID:     req.DeviceID,
		DeviceName:   deviceName,
		BindType:     bindType,
		Status:       true,
		LastConnect:  time.Now(),
		ConnectCount: 1,
	}
	if result := h.db.Create(&binding); result.Error != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "绑定设备失败: " + result.Error.Error()})
		return
	}

	recomputeDeviceCount(h.db, authedUserID)
	h.logConnection(authedUserID, req.DeviceID, deviceName, "success", "", c.ClientIP())

	c.JSON(http.StatusOK, gin.H{"message": "设备绑定成功", "binding": binding})
}

// UnbindDevice handles POST /api/v1/user/devices/unbind
func (h *UserDeviceHandler) UnbindDevice(c *gin.Context) {
	var req struct {
		DeviceID string `json:"device_id" binding:"required"`
	}
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}
	userIDVal, _ := c.Get("authed_user_id")
	authedUserID := userIDVal.(uint)

	var binding models.UserDevice
	if result := h.db.Where("user_id = ? AND device_id = ? AND status = ?", authedUserID, req.DeviceID, true).First(&binding); result.Error != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "绑定记录不存在"})
		return
	}

	binding.Status = false
	h.db.Save(&binding)
	recomputeDeviceCount(h.db, authedUserID)

	c.JSON(http.StatusOK, gin.H{"message": "设备解绑成功"})
}

// GetUserDevices handles GET /api/v1/user/devices
// Returns all devices bound to a user (from the devices table).
func (h *UserDeviceHandler) GetUserDevices(c *gin.Context) {
	userID, _ := c.Get("authed_user_id")

	var devices []models.Device
	if result := h.db.Where("user_id = ?", userID).Find(&devices); result.Error != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": result.Error.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{"devices": devices, "count": len(devices)})
}

// GetUserDeviceLogs handles GET /api/v1/user/devices/logs
// Returns connection history for a user over the last 3 days.
func (h *UserDeviceHandler) GetUserDeviceLogs(c *gin.Context) {
	userID, _ := c.Get("authed_user_id")

	threeDaysAgo := time.Now().AddDate(0, 0, -3)
	var logs []models.ConnectionHistory
	if result := h.db.Where("user_id = ? AND created_at >= ?", userID, threeDaysAgo).
		Order("created_at DESC").
		Limit(100).
		Find(&logs); result.Error != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": result.Error.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{"logs": logs, "count": len(logs)})
}

// CheckDeviceBinding handles GET /api/v1/user/devices/check
func (h *UserDeviceHandler) CheckDeviceBinding(c *gin.Context) {
	userID, _ := c.Get("authed_user_id")
	deviceID := c.Query("device_id")
	if deviceID == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "设备ID不能为空"})
		return
	}

	var binding models.UserDevice
	result := h.db.Where("user_id = ? AND device_id = ? AND status = ?", userID, deviceID, true).First(&binding)
	c.JSON(http.StatusOK, gin.H{"is_bound": result.Error == nil, "binding": binding})
}

// QuickConnectBind handles POST /api/v1/user/devices/quick-connect
// Binds a device to a user by updating the devices table entry directly.
func (h *UserDeviceHandler) QuickConnectBind(c *gin.Context) {
	var req struct {
		DeviceID   string `json:"device_id" binding:"required"`
		DeviceName string `json:"device_name"`
		AccessCode string `json:"access_code"`
	}
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}
	userIDVal, _ := c.Get("authed_user_id")
	authedUserID := userIDVal.(uint)

	var user models.User
	if result := h.db.First(&user, authedUserID); result.Error != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "用户不存在"})
		return
	}

	var device models.Device
	if result := h.db.Where("device_id = ?", req.DeviceID).First(&device); result.Error != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "设备不存在"})
		return
	}

	updates := map[string]interface{}{"user_id": authedUserID}
	if req.DeviceName != "" {
		updates["device_name"] = req.DeviceName
	} else if device.DeviceName == "" {
		updates["device_name"] = "设备-" + req.DeviceID
	}
	if req.AccessCode != "" {
		updates["access_code"] = req.AccessCode
	}

	if result := h.db.Model(&models.Device{}).Where("device_id = ?", req.DeviceID).Updates(updates); result.Error != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "绑定设备失败: " + result.Error.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{"message": "设备绑定成功", "device": device})
}

// RecordConnection handles POST /api/v1/user/devices/record
// Called by WebClient after a connection attempt to persist its result.
func (h *UserDeviceHandler) RecordConnection(c *gin.Context) {
	var req struct {
		DeviceID string `json:"device_id" binding:"required"`
		Duration int    `json:"duration"` // seconds
		Status   string `json:"status"`   // success / failed / timeout
		ErrorMsg string `json:"error_msg"`
	}
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}
	userIDVal, _ := c.Get("authed_user_id")
	authedUserID := userIDVal.(uint)

	entry := models.ConnectionHistory{
		UserID:    authedUserID,
		DeviceID:  req.DeviceID,
		ConnectIP: c.ClientIP(),
		Status:    req.Status,
		ErrorMsg:  req.ErrorMsg,
		Duration:  req.Duration,
	}
	h.db.Create(&entry)

	if req.Status == "success" {
		h.db.Model(&models.UserDevice{}).
			Where("user_id = ? AND device_id = ? AND status = ?", authedUserID, req.DeviceID, true).
			Updates(map[string]interface{}{
				"last_connect":  time.Now(),
				"connect_count": gorm.Expr("connect_count + 1"),
			})
	}

	c.JSON(http.StatusOK, gin.H{"message": "连接记录已保存"})
}

// GetAllBindings handles GET /admin/device-bindings — admin only.
func (h *UserDeviceHandler) GetAllBindings(c *gin.Context) {
	var bindings []models.UserDevice
	if result := h.db.Preload("User").Find(&bindings); result.Error != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": result.Error.Error()})
		return
	}
	c.JSON(http.StatusOK, gin.H{"bindings": bindings, "count": len(bindings)})
}

// recomputeDeviceCount recalculates the user's device_count from the actual user_devices table.
func recomputeDeviceCount(db *gorm.DB, userID uint) {
	var count int64
	db.Model(&models.UserDevice{}).Where("user_id = ? AND status = ?", userID, true).Count(&count)
	db.Model(&models.User{}).Where("id = ?", userID).Update("device_count", count)
}

// logConnection persists a connection event to connection_histories.
func (h *UserDeviceHandler) logConnection(userID uint, deviceID, deviceName, status, errorMsg, connectIP string) {
	entry := models.ConnectionHistory{
		UserID:     userID,
		DeviceID:   deviceID,
		DeviceName: deviceName,
		ConnectIP:  connectIP,
		Status:     status,
		ErrorMsg:   errorMsg,
	}
	h.db.Create(&entry)
}
