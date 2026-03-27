package handler

import (
	"context"
	"crypto/rand"
	"encoding/hex"
	"fmt"
	"net/http"
	"quickdesk/signaling/internal/models"
	"quickdesk/signaling/internal/service"
	"strconv"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/redis/go-redis/v9"
	"golang.org/x/crypto/bcrypt"
	"gorm.io/gorm"
)

// UserHandler handles admin-facing user management CRUD operations.
type UserHandler struct {
	db *gorm.DB
}

// NewUserHandler creates a new UserHandler.
func NewUserHandler(db *gorm.DB) *UserHandler {
	return &UserHandler{db: db}
}

// GetUsers handles GET /admin/user-list
func (h *UserHandler) GetUsers(c *gin.Context) {
	var users []models.User
	if result := h.db.Find(&users); result.Error != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": result.Error.Error()})
		return
	}

	type UserWithDevices struct {
		models.User
		Devices []models.UserDevice `json:"devices"`
	}

	result := make([]UserWithDevices, 0, len(users))
	for _, user := range users {
		var devices []models.UserDevice
		h.db.Where("user_id = ? AND status = ?", user.ID, true).Find(&devices)
		result = append(result, UserWithDevices{User: user, Devices: devices})
	}

	c.JSON(http.StatusOK, gin.H{"users": result})
}

// GetUser handles GET /admin/user-list/:id
func (h *UserHandler) GetUser(c *gin.Context) {
	id := c.Param("id")
	var user models.User
	if result := h.db.First(&user, id); result.Error != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "用户不存在"})
		return
	}
	c.JSON(http.StatusOK, user)
}

// CreateUser handles POST /admin/user-list
func (h *UserHandler) CreateUser(c *gin.Context) {
	var req struct {
		Username    string `json:"username" binding:"required"`
		Phone       string `json:"phone"`
		Email       string `json:"email"`
		Password    string `json:"password" binding:"required"`
		Level       string `json:"level"`
		ChannelType string `json:"channelType"`
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	var existing models.User
	if result := h.db.Where("username = ?", req.Username).First(&existing); result.Error == nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "用户名已存在"})
		return
	}

	hashedPassword, err := bcrypt.GenerateFromPassword([]byte(req.Password), bcrypt.DefaultCost)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "密码加密失败"})
		return
	}

	level := req.Level
	if level == "" {
		level = "V1"
	}
	channelType := req.ChannelType
	if channelType == "" {
		channelType = "全球"
	}

	user := models.User{
		Username:    req.Username,
		Phone:       req.Phone,
		Email:       req.Email,
		Password:    string(hashedPassword),
		Level:       level,
		ChannelType: channelType,
		Status:      true,
	}

	if result := h.db.Create(&user); result.Error != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": result.Error.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{"message": "用户创建成功", "user": user})
}

// UpdateUser handles PUT /admin/user-list/:id
func (h *UserHandler) UpdateUser(c *gin.Context) {
	id := c.Param("id")
	var user models.User
	if result := h.db.First(&user, id); result.Error != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "用户不存在"})
		return
	}

	var req struct {
		Username    string `json:"username"`
		Phone       string `json:"phone"`
		Email       string `json:"email"`
		Password    string `json:"password"`
		Level       string `json:"level"`
		DeviceCount int    `json:"deviceCount"`
		ChannelType string `json:"channelType"`
		Status      *bool  `json:"status"`
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	if req.Username != "" {
		user.Username = req.Username
	}
	if req.Phone != "" {
		user.Phone = req.Phone
	}
	if req.Email != "" {
		user.Email = req.Email
	}
	if req.Password != "" {
		hashed, err := bcrypt.GenerateFromPassword([]byte(req.Password), bcrypt.DefaultCost)
		if err != nil {
			c.JSON(http.StatusInternalServerError, gin.H{"error": "密码加密失败"})
			return
		}
		user.Password = string(hashed)
	}
	if req.Level != "" {
		user.Level = req.Level
	}
	if req.ChannelType != "" {
		user.ChannelType = req.ChannelType
	}
	if req.Status != nil {
		user.Status = *req.Status
	}
	user.DeviceCount = req.DeviceCount

	if result := h.db.Save(&user); result.Error != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": result.Error.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{"message": "用户更新成功", "user": user})
}

// DeleteUser handles DELETE /admin/user-list/:id
func (h *UserHandler) DeleteUser(c *gin.Context) {
	id := c.Param("id")
	if result := h.db.Delete(&models.User{}, id); result.Error != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": result.Error.Error()})
		return
	}
	c.JSON(http.StatusOK, gin.H{"message": "用户删除成功"})
}

// UpdateUserDeviceCount handles PUT /admin/user-list/:id/device-count
func (h *UserHandler) UpdateUserDeviceCount(c *gin.Context) {
	id := c.Param("id")
	var req struct {
		DeviceCount int `json:"deviceCount"`
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	if result := h.db.Model(&models.User{}).Where("id = ?", id).Update("device_count", req.DeviceCount); result.Error != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": result.Error.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{"message": "设备数量更新成功"})
}

// ---------------------------------------------------------------------------
// UserAuth: Redis-backed token authentication for end-users (7-day TTL).
// ---------------------------------------------------------------------------

const userTokenTTL = 7 * 24 * time.Hour

// UserAuth manages user session tokens in Redis.
type UserAuth struct {
	db  *gorm.DB
	rdb *redis.Client
	sms *service.SmsService
}

// NewUserAuth creates a new UserAuth instance.
func NewUserAuth(db *gorm.DB, rdb *redis.Client) *UserAuth {
	return &UserAuth{db: db, rdb: rdb}
}

// SetSmsService injects the SMS service (may be nil if SMS is disabled).
func (a *UserAuth) SetSmsService(sms *service.SmsService) {
	a.sms = sms
}

// CleanupLoop is a no-op kept for API compatibility. Redis TTL handles expiry automatically.
func (a *UserAuth) CleanupLoop() {}

func (a *UserAuth) generateToken() string {
	b := make([]byte, 32)
	rand.Read(b)
	return hex.EncodeToString(b)
}

func (a *UserAuth) redisKey(token string) string {
	return fmt.Sprintf("user_token:%s", token)
}

// Register handles POST /api/v1/user/register
func (a *UserAuth) Register(c *gin.Context) {
	var req struct {
		Username    string `json:"username" binding:"required"`
		Password    string `json:"password" binding:"required"`
		Phone       string `json:"phone"`
		SmsCode     string `json:"sms_code"`
		Email       string `json:"email"`
		Level       string `json:"level"`
		ChannelType string `json:"channelType"`
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid request"})
		return
	}

	// When SMS is enabled, phone + sms_code are mandatory
	smsEnabled := a.sms != nil && a.sms.IsEnabled()
	if smsEnabled {
		if req.Phone == "" || req.SmsCode == "" {
			c.JSON(http.StatusBadRequest, gin.H{"error": "手机号和验证码为必填项"})
			return
		}
		if !service.ValidatePhone(req.Phone) {
			c.JSON(http.StatusBadRequest, gin.H{"error": "手机号格式不正确"})
			return
		}
		// Verify SMS code
		if err := a.sms.VerifyCode(c.Request.Context(), req.Phone, req.SmsCode); err != nil {
			c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
			return
		}
	}

	var existing models.User
	if result := a.db.Where("username = ?", req.Username).First(&existing); result.Error == nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "用户名已存在"})
		return
	}

	// Check phone uniqueness (when phone is provided)
	if req.Phone != "" {
		var phoneUser models.User
		if result := a.db.Where("phone = ?", req.Phone).First(&phoneUser); result.Error == nil {
			c.JSON(http.StatusConflict, gin.H{"error": "该手机号已注册"})
			return
		}
	}

	hashedPassword, err := bcrypt.GenerateFromPassword([]byte(req.Password), bcrypt.DefaultCost)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "密码加密失败"})
		return
	}

	user := models.User{
		Username:    req.Username,
		Phone:       req.Phone,
		Email:       req.Email,
		Password:    string(hashedPassword),
		Level:       "V1",
		ChannelType: "全球",
		Status:      true,
	}

	if result := a.db.Create(&user); result.Error != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "创建用户失败"})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"message": "注册成功",
		"user": gin.H{
			"id":          user.ID,
			"username":    user.Username,
			"phone":       user.Phone,
			"email":       user.Email,
			"level":       user.Level,
			"deviceCount": user.DeviceCount,
			"channelType": user.ChannelType,
			"status":      user.Status,
			"createdAt":   user.CreatedAt,
			"updatedAt":   user.UpdatedAt,
		},
	})
}

// Login handles POST /api/v1/user/login
func (a *UserAuth) Login(c *gin.Context) {
	var req struct {
		Username string `json:"username" binding:"required"`
		Password string `json:"password" binding:"required"`
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid request"})
		return
	}

	var user models.User
	if result := a.db.Where("username = ?", req.Username).First(&user); result.Error != nil {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "用户名或密码错误"})
		return
	}

	if err := bcrypt.CompareHashAndPassword([]byte(user.Password), []byte(req.Password)); err != nil {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "用户名或密码错误"})
		return
	}

	if !user.Status {
		c.JSON(http.StatusForbidden, gin.H{"error": "账号已被禁用"})
		return
	}

	token := a.generateToken()
	ctx := context.Background()
	if err := a.rdb.Set(ctx, a.redisKey(token), user.ID, userTokenTTL).Err(); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "session 存储失败"})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"token": token,
		"user": gin.H{
			"id":          user.ID,
			"username":    user.Username,
			"phone":       user.Phone,
			"email":       user.Email,
			"level":       user.Level,
			"deviceCount": user.DeviceCount,
			"channelType": user.ChannelType,
			"status":      user.Status,
			"createdAt":   user.CreatedAt,
			"updatedAt":   user.UpdatedAt,
		},
	})
}

// LoginWithSms handles POST /api/v1/user/login-sms
func (a *UserAuth) LoginWithSms(c *gin.Context) {
	if a.sms == nil || !a.sms.IsEnabled() {
		c.JSON(http.StatusServiceUnavailable, gin.H{"error": "短信服务未启用"})
		return
	}

	var req struct {
		Phone   string `json:"phone" binding:"required"`
		SmsCode string `json:"sms_code" binding:"required"`
	}
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "请提供手机号和验证码"})
		return
	}

	if !service.ValidatePhone(req.Phone) {
		c.JSON(http.StatusBadRequest, gin.H{"error": "手机号格式不正确"})
		return
	}

	// Verify SMS code
	if err := a.sms.VerifyCode(c.Request.Context(), req.Phone, req.SmsCode); err != nil {
		c.JSON(http.StatusUnauthorized, gin.H{"error": err.Error()})
		return
	}

	// Find user by phone
	var user models.User
	if result := a.db.Where("phone = ?", req.Phone).First(&user); result.Error != nil {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "该手机号未注册"})
		return
	}

	if !user.Status {
		c.JSON(http.StatusForbidden, gin.H{"error": "账号已被禁用"})
		return
	}

	token := a.generateToken()
	ctx := context.Background()
	if err := a.rdb.Set(ctx, a.redisKey(token), user.ID, userTokenTTL).Err(); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "session 存储失败"})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"token": token,
		"user": gin.H{
			"id":          user.ID,
			"username":    user.Username,
			"phone":       user.Phone,
			"email":       user.Email,
			"level":       user.Level,
			"deviceCount": user.DeviceCount,
			"channelType": user.ChannelType,
			"status":      user.Status,
			"createdAt":   user.CreatedAt,
			"updatedAt":   user.UpdatedAt,
		},
	})
}

// AuthRequired returns a Gin middleware that requires a valid user token.
func (a *UserAuth) AuthRequired() gin.HandlerFunc {
	return func(c *gin.Context) {
		token := ""
		auth := c.GetHeader("Authorization")
		if len(auth) > 7 && auth[:7] == "Bearer " {
			token = auth[7:]
		}
		if token == "" {
			token = c.Query("token")
		}

		if token == "" {
			c.AbortWithStatusJSON(http.StatusUnauthorized, gin.H{"error": "未授权"})
			return
		}

		ctx := context.Background()
		val, err := a.rdb.Get(ctx, a.redisKey(token)).Result()
		if err != nil {
			c.AbortWithStatusJSON(http.StatusUnauthorized, gin.H{"error": "token已过期"})
			return
		}

		userID, _ := strconv.ParseUint(val, 10, 64)
		c.Set("authed_user_id", uint(userID))
		c.Next()
	}
}

// GetUserIDFromToken extracts the user ID from the request token.
// Returns 0 if the token is missing or invalid.
func (a *UserAuth) GetUserIDFromToken(c *gin.Context) uint {
	token := ""
	auth := c.GetHeader("Authorization")
	if len(auth) > 7 && auth[:7] == "Bearer " {
		token = auth[7:]
	}
	if token == "" {
		token = c.Query("token")
	}

	if token == "" {
		return 0
	}

	val, err := a.rdb.Get(context.Background(), a.redisKey(token)).Result()
	if err != nil {
		return 0
	}
	userID, _ := strconv.ParseUint(val, 10, 64)
	return uint(userID)
}

// Logout handles POST /api/v1/user/logout
func (a *UserAuth) Logout(c *gin.Context) {
	token := ""
	auth := c.GetHeader("Authorization")
	if len(auth) > 7 && auth[:7] == "Bearer " {
		token = auth[7:]
	}
	if token == "" {
		token = c.Query("token")
	}

	if token == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "token不能为空"})
		return
	}

	ctx := context.Background()
	a.rdb.Del(ctx, a.redisKey(token))

	c.JSON(http.StatusOK, gin.H{"message": "退出登录成功"})
}

// GetMe handles GET /api/v1/user/me
func (a *UserAuth) GetMe(c *gin.Context) {
	userID, _ := c.Get("authed_user_id")

	var user models.User
	if result := a.db.First(&user, userID); result.Error != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "用户不存在"})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"user": gin.H{
			"id":          user.ID,
			"username":    user.Username,
			"phone":       user.Phone,
			"email":       user.Email,
			"level":       user.Level,
			"deviceCount": user.DeviceCount,
			"channelType": user.ChannelType,
			"status":      user.Status,
			"createdAt":   user.CreatedAt,
			"updatedAt":   user.UpdatedAt,
		},
	})
}
