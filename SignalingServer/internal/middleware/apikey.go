package middleware

import (
	"net/http"
	"strings"

	"github.com/gin-gonic/gin"
)

type APIKeyAuth struct {
	apiKey         string
	allowedOrigins map[string]bool
}

func NewAPIKeyAuth(apiKey string, allowedOrigins []string) *APIKeyAuth {
	origins := make(map[string]bool, len(allowedOrigins))
	for _, o := range allowedOrigins {
		origins[strings.TrimRight(strings.ToLower(o), "/")] = true
	}
	return &APIKeyAuth{apiKey: apiKey, allowedOrigins: origins}
}

func (a *APIKeyAuth) Enabled() bool {
	return a.apiKey != "" || len(a.allowedOrigins) > 0
}

func (a *APIKeyAuth) validateAPIKey(c *gin.Context) bool {
	if a.apiKey == "" {
		return false
	}
	clientKey := c.GetHeader("X-API-Key")
	if clientKey == "" {
		clientKey = c.Query("api_key")
	}
	return clientKey == a.apiKey
}

func (a *APIKeyAuth) validateOrigin(c *gin.Context) bool {
	if len(a.allowedOrigins) == 0 {
		return false
	}
	origin := strings.TrimRight(strings.ToLower(c.GetHeader("Origin")), "/")
	return origin != "" && a.allowedOrigins[origin]
}

// Required returns a middleware that rejects requests without a valid API key
// or an allowed Origin header. If neither API key nor allowed origins are
// configured, all requests are allowed.
func (a *APIKeyAuth) Required() gin.HandlerFunc {
	return func(c *gin.Context) {
		if !a.Enabled() {
			c.Next()
			return
		}

		if a.validateAPIKey(c) || a.validateOrigin(c) {
			c.Next()
			return
		}

		c.AbortWithStatusJSON(http.StatusForbidden, gin.H{
			"error":   "ACCESS_DENIED",
			"message": "Invalid or missing API key / origin not allowed",
		})
	}
}

// ValidateRequest checks the API key or Origin without aborting
// (for WebSocket pre-upgrade check).
func (a *APIKeyAuth) ValidateRequest(c *gin.Context) bool {
	if !a.Enabled() {
		return true
	}
	return a.validateAPIKey(c) || a.validateOrigin(c)
}
