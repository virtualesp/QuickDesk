package service

import (
	"context"
	"encoding/json"
	"fmt"
	"log"
	"math/rand"
	"regexp"
	"time"

	openapi "github.com/alibabacloud-go/darabonba-openapi/v2/client"
	dysmsapi "github.com/alibabacloud-go/dysmsapi-20170525/v4/client"
	"github.com/alibabacloud-go/tea/tea"
	"github.com/redis/go-redis/v9"
)

var phoneRegex = regexp.MustCompile(`^1[3-9]\d{9}$`)

const (
	smsCodeTTL    = 5 * time.Minute  // verification code validity
	smsRateTTL    = 60 * time.Second // per-number cooldown
	smsDailyTTL   = 24 * time.Hour   // daily limit window
	smsDailyLimit = 10               // max codes per phone per day
	smsMaxAttempts = 3               // max wrong attempts before code is invalidated
)

type smsCodeData struct {
	Code     string `json:"code"`
	Attempts int    `json:"attempts"`
}

type SmsService struct {
	rdb       *redis.Client
	smsClient *dysmsapi.Client
	signName  string
	template  string
	enabled   bool
}

func NewSmsService(rdb *redis.Client, accessKeyID, accessKeySecret, signName, templateCode string, enabled bool) *SmsService {
	s := &SmsService{
		rdb:      rdb,
		signName: signName,
		template: templateCode,
		enabled:  enabled,
	}

	if enabled {
		cfg := &openapi.Config{
			AccessKeyId:     tea.String(accessKeyID),
			AccessKeySecret: tea.String(accessKeySecret),
			Endpoint:        tea.String("dysmsapi.aliyuncs.com"),
		}
		client, err := dysmsapi.NewClient(cfg)
		if err != nil {
			log.Printf("[SmsService] Failed to create Aliyun SMS client: %v (SMS disabled)", err)
			s.enabled = false
		} else {
			s.smsClient = client
			log.Println("[SmsService] Aliyun SMS client initialized")
		}
	} else {
		log.Println("[SmsService] SMS disabled (Aliyun credentials not configured)")
	}

	return s
}

func (s *SmsService) IsEnabled() bool {
	return s.enabled
}

func ValidatePhone(phone string) bool {
	return phoneRegex.MatchString(phone)
}

// SendCode generates a 4-digit code, stores it in Redis, and sends via Aliyun SMS.
func (s *SmsService) SendCode(ctx context.Context, phone string) error {
	if !s.enabled {
		return fmt.Errorf("SMS service is not enabled")
	}

	// Rate limit: 60s cooldown
	rateKey := fmt.Sprintf("sms_rate:%s", phone)
	if s.rdb.Exists(ctx, rateKey).Val() > 0 {
		return fmt.Errorf("发送太频繁，请稍后再试")
	}

	// Daily limit
	dailyKey := fmt.Sprintf("sms_daily:%s", phone)
	count, _ := s.rdb.Get(ctx, dailyKey).Int()
	if count >= smsDailyLimit {
		return fmt.Errorf("今日验证码发送次数已达上限")
	}

	// Generate 4-digit code
	code := fmt.Sprintf("%04d", rand.Intn(10000))

	// Send via Aliyun
	templateParam, _ := json.Marshal(map[string]string{"code": code})
	req := &dysmsapi.SendSmsRequest{
		PhoneNumbers:  tea.String(phone),
		SignName:      tea.String(s.signName),
		TemplateCode:  tea.String(s.template),
		TemplateParam: tea.String(string(templateParam)),
	}

	resp, err := s.smsClient.SendSms(req)
	if err != nil {
		log.Printf("[SmsService] Aliyun SendSms error: %v", err)
		return fmt.Errorf("短信发送失败")
	}
	if resp.Body != nil && resp.Body.Code != nil && *resp.Body.Code != "OK" {
		log.Printf("[SmsService] Aliyun SendSms rejected: code=%s msg=%s", tea.StringValue(resp.Body.Code), tea.StringValue(resp.Body.Message))
		return fmt.Errorf("短信发送失败: %s", tea.StringValue(resp.Body.Message))
	}

	// Store code in Redis
	codeKey := fmt.Sprintf("sms_code:%s", phone)
	data, _ := json.Marshal(smsCodeData{Code: code, Attempts: 0})
	s.rdb.Set(ctx, codeKey, string(data), smsCodeTTL)

	// Set rate limit
	s.rdb.Set(ctx, rateKey, "1", smsRateTTL)

	// Increment daily counter
	pipe := s.rdb.Pipeline()
	pipe.Incr(ctx, dailyKey)
	pipe.Expire(ctx, dailyKey, smsDailyTTL)
	pipe.Exec(ctx)

	log.Printf("[SmsService] Code sent to %s", phone)
	return nil
}

// VerifyCode checks the code and returns nil on success.
// On failure it increments the attempt counter; after 3 wrong attempts the code is deleted.
func (s *SmsService) VerifyCode(ctx context.Context, phone, code string) error {
	codeKey := fmt.Sprintf("sms_code:%s", phone)
	val, err := s.rdb.Get(ctx, codeKey).Result()
	if err != nil {
		return fmt.Errorf("验证码已过期，请重新获取")
	}

	var data smsCodeData
	if err := json.Unmarshal([]byte(val), &data); err != nil {
		s.rdb.Del(ctx, codeKey)
		return fmt.Errorf("验证码已过期，请重新获取")
	}

	if data.Attempts >= smsMaxAttempts {
		s.rdb.Del(ctx, codeKey)
		return fmt.Errorf("错误次数过多，请重新获取验证码")
	}

	if data.Code != code {
		data.Attempts++
		updated, _ := json.Marshal(data)
		ttl := s.rdb.TTL(ctx, codeKey).Val()
		if ttl > 0 {
			s.rdb.Set(ctx, codeKey, string(updated), ttl)
		}
		remaining := smsMaxAttempts - data.Attempts
		if remaining <= 0 {
			s.rdb.Del(ctx, codeKey)
			return fmt.Errorf("错误次数过多，请重新获取验证码")
		}
		return fmt.Errorf("验证码错误，还可尝试%d次", remaining)
	}

	// Success – delete the code so it can't be reused
	s.rdb.Del(ctx, codeKey)
	return nil
}
