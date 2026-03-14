package middleware

import (
	"context"
	"crypto/rand"
	"encoding/hex"
	"fmt"
	"net/http"
	"strings"
	"time"

	"quickdesk/signaling/internal/service"

	"github.com/gin-gonic/gin"
	"github.com/redis/go-redis/v9"
)

const adminTokenTTL = 24 * time.Hour

type AdminAuth struct {
	service *service.AdminUserService
	rdb     *redis.Client
}

func NewAdminAuth(adminUserService *service.AdminUserService, rdb *redis.Client) *AdminAuth {
	return &AdminAuth{service: adminUserService, rdb: rdb}
}

func (a *AdminAuth) redisKey(token string) string {
	return fmt.Sprintf("admin_token:%s", token)
}

func (a *AdminAuth) Login(c *gin.Context) {
	var req struct {
		User     string `json:"user" binding:"required"`
		Password string `json:"password" binding:"required"`
	}
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid request"})
		return
	}

	user, err := a.service.ValidateCredentials(context.Background(), req.User, req.Password)
	if err != nil {
		c.JSON(http.StatusUnauthorized, gin.H{"error": err.Error()})
		return
	}

	token := generateToken()
	if err := a.rdb.Set(context.Background(), a.redisKey(token), user.Username, adminTokenTTL).Err(); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "session 存储失败"})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"token": token,
		"user":  user.ToResponse(),
	})
}

// AuthRequired is a Gin middleware that verifies the admin token.
func (a *AdminAuth) AuthRequired() gin.HandlerFunc {
	return func(c *gin.Context) {
		token := ""
		auth := c.GetHeader("Authorization")
		if strings.HasPrefix(auth, "Bearer ") {
			token = strings.TrimPrefix(auth, "Bearer ")
		}
		if token == "" {
			token = c.Query("token")
		}

		if token == "" {
			c.AbortWithStatusJSON(http.StatusUnauthorized, gin.H{"error": "unauthorized"})
			return
		}

		if err := a.rdb.Exists(context.Background(), a.redisKey(token)).Err(); err != nil {
			c.AbortWithStatusJSON(http.StatusUnauthorized, gin.H{"error": "token expired"})
			return
		}

		c.Next()
	}
}

func generateToken() string {
	b := make([]byte, 32)
	rand.Read(b)
	return hex.EncodeToString(b)
}
