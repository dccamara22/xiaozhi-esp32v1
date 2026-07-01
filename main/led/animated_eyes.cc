#include "animated_eyes.h"
#include <esp_log.h>

static const char* TAG = "AnimatedEyes";

AnimatedEyes::AnimatedEyes() {}

AnimatedEyes::~AnimatedEyes() {
    if (matrix_) {
        matrix_->TurnOff();
    }
}

bool AnimatedEyes::Initialize(int cs_pin, int clk_pin, int din_pin) {
    ESP_LOGI(TAG, "Initializing Animated Eyes");

    matrix_ = std::make_unique<Max7219>();
    if (!matrix_) {
        ESP_LOGE(TAG, "Failed to create Max7219 instance");
        return false;
    }

    if (!matrix_->Initialize(cs_pin, clk_pin, din_pin)) {
        ESP_LOGE(TAG, "Failed to initialize Max7219 driver");
        matrix_.reset();
        return false;
    }

    enabled_ = true;
    matrix_->SetExpression(Max7219::kExpressionNeutral);

    ESP_LOGI(TAG, "Animated Eyes initialized successfully");
    return true;
}

Max7219::EyeExpression AnimatedEyes::GetExpressionForState(DeviceState state) {
    switch (state) {
        case kDeviceStateUnknown:
        case kDeviceStateStarting:
            return Max7219::kExpressionNeutral;

        case kDeviceStateWifiConfiguring:
        case kDeviceStateActivating:
            return Max7219::kExpressionThinking;

        case kDeviceStateIdle:
            return Max7219::kExpressionNeutral;

        case kDeviceStateConnecting:
            return Max7219::kExpressionThinking;

        case kDeviceStateListening:
            return Max7219::kExpressionListening;

        case kDeviceStateSpeaking:
            return Max7219::kExpressionSpeaking;

        case kDeviceStateUpgrading:
            return Max7219::kExpressionThinking;

        case kDeviceStateAudioTesting:
            return Max7219::kExpressionListening;

        case kDeviceStateFatalError:
            return Max7219::kExpressionError;

        default:
            return Max7219::kExpressionNeutral;
    }
}

void AnimatedEyes::OnStateChanged(DeviceState old_state, DeviceState new_state) {
    if (!enabled_ || !matrix_) return;

    Max7219::EyeExpression expression = GetExpressionForState(new_state);
    matrix_->SetExpression(expression);

    ESP_LOGI(TAG, "State changed: %d -> %d, Expression: %d", old_state, new_state, expression);
}

void AnimatedEyes::Update() {
    if (!enabled_ || !matrix_) return;
    matrix_->Update();
}

void AnimatedEyes::SetBrightness(uint8_t brightness) {
    if (!enabled_ || !matrix_) return;
    matrix_->SetBrightness(brightness);
}

void AnimatedEyes::TurnOff() {
    if (!matrix_) return;
    matrix_->TurnOff();
    enabled_ = false;
}
