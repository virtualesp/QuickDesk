package handler

import (
	"net/http"
	"quickdesk/signaling/internal/models"
	"quickdesk/signaling/internal/service"

	"github.com/gin-gonic/gin"
	"gorm.io/gorm"
)

// SmsHandler handles SMS verification code endpoints.
type SmsHandler struct {
	sms *service.SmsService
	db  *gorm.DB
}

func NewSmsHandler(sms *service.SmsService, db *gorm.DB) *SmsHandler {
	return &SmsHandler{sms: sms, db: db}
}

// SendCode handles POST /api/v1/sms/send
func (h *SmsHandler) SendCode(c *gin.Context) {
	if !h.sms.IsEnabled() {
		c.JSON(http.StatusServiceUnavailable, gin.H{"error": "短信服务未启用"})
		return
	}

	var req struct {
		Phone string `json:"phone" binding:"required"`
		Scene string `json:"scene" binding:"required"` // "register" or "login"
	}
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "请提供手机号和场景"})
		return
	}

	if !service.ValidatePhone(req.Phone) {
		c.JSON(http.StatusBadRequest, gin.H{"error": "手机号格式不正确"})
		return
	}

	// Scene-specific validation
	var existing models.User
	phoneExists := h.db.Where("phone = ?", req.Phone).First(&existing).Error == nil

	switch req.Scene {
	case "register":
		if phoneExists {
			c.JSON(http.StatusConflict, gin.H{"error": "该手机号已注册"})
			return
		}
	case "login":
		if !phoneExists {
			c.JSON(http.StatusConflict, gin.H{"error": "该手机号未注册"})
			return
		}
	default:
		c.JSON(http.StatusBadRequest, gin.H{"error": "无效的场景参数"})
		return
	}

	if err := h.sms.SendCode(c.Request.Context(), req.Phone); err != nil {
		// Distinguish rate-limit errors from internal errors
		statusCode := http.StatusInternalServerError
		errMsg := err.Error()
		if errMsg == "发送太频繁，请稍后再试" || errMsg == "今日验证码发送次数已达上限" {
			statusCode = http.StatusTooManyRequests
		}
		c.JSON(statusCode, gin.H{"error": errMsg})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"message":    "验证码已发送",
		"expires_in": 300,
	})
}
