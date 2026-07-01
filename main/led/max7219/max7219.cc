#include "max7219.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char* TAG = "Max7219";

// Expression bitmap patterns (8x8 LED matrix)
// Each uint8_t represents one row (bit 0-7 = LED 0-7)

// Neutral eyes (•_•)
static const uint8_t PATTERN_NEUTRAL_LEFT[8] = {
    0b01111110,  // Row 0: -XXXXX-
    0b11000011,  // Row 1: XX---XX
    0b10011001,  // Row 2: X--XX--X
    0b10011001,  // Row 3: X--XX--X
    0b10011001,  // Row 4: X--XX--X
    0b11000011,  // Row 5: XX---XX
    0b01111110,  // Row 6: -XXXXX-
    0b00000000   // Row 7: --------
};

static const uint8_t PATTERN_NEUTRAL_RIGHT[8] = {
    0b01111110,
    0b11000011,
    0b10011001,
    0b10011001,
    0b10011001,
    0b11000011,
    0b01111110,
    0b00000000
};

// Listening eyes (o_o - focused)
static const uint8_t PATTERN_LISTENING_LEFT[8] = {
    0b01111110,
    0b11000011,
    0b10111101,  // X-XXX-X (wide eyes)
    0b10111101,
    0b10111101,
    0b11000011,
    0b01111110,
    0b00000000
};

static const uint8_t PATTERN_LISTENING_RIGHT[8] = {
    0b01111110,
    0b11000011,
    0b10111101,
    0b10111101,
    0b10111101,
    0b11000011,
    0b01111110,
    0b00000000
};

// Thinking/blinking eyes frame 1 (closed)
static const uint8_t PATTERN_THINKING_CLOSED[8] = {
    0b01111110,
    0b11111111,  // XXXXXXXX (closed)
    0b11111111,  // XXXXXXXX (closed)
    0b11111111,  // XXXXXXXX
    0b11111111,
    0b11111111,
    0b01111110,
    0b00000000
};

// Speaking eyes (^ ^ - excited)
static const uint8_t PATTERN_SPEAKING[8] = {
    0b01111110,
    0b10111101,  // X-XXX-X
    0b10000001,  // X-----X (wide open)
    0b10000001,
    0b10000001,
    0b10111101,
    0b01111110,
    0b00000000
};

// Happy eyes (^ _^ - smiling)
static const uint8_t PATTERN_HAPPY[8] = {
    0b00000000,
    0b01111110,
    0b10011001,
    0b10011001,
    0b10000001,  // X-----X (smile line)
    0b01100110,  // -XX--XX-
    0b00111100,  // --XXXX--
    0b00000000
};

// Error/alert eyes (X X - alert)
static const uint8_t PATTERN_ERROR[8] = {
    0b00000000,
    0b11000011,  // XX---XX
    0b10100101,  // X-X-X-X
    0b10100101,
    0b10100101,
    0b11000011,
    0b00000000,
    0b00000000
};

Max7219::Max7219() {}

Max7219::~Max7219() {
    if (spi_handle_) {
        spi_bus_remove_device(spi_handle_);
        spi_handle_ = nullptr;
    }
}

bool Max7219::Initialize(int cs_pin, int clk_pin, int din_pin) {
    ESP_LOGI(TAG, "Initializing MAX7219 with CS=%d, CLK=%d, DIN=%d", cs_pin, clk_pin, din_pin);

    // SPI bus configuration
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = din_pin,
        .miso_io_num = -1,
        .sclk_io_num = clk_pin,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 32,
    };

    // Initialize SPI bus
    esp_err_t ret = spi_bus_initialize(SPI3_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "SPI bus initialization failed: %s", esp_err_to_name(ret));
        return false;
    }

    // Device configuration
    spi_device_interface_config_t dev_cfg = {
        .command_bits = 0,
        .address_bits = 0,
        .dummy_bits = 0,
        .mode = 0,
        .duty_cycle_pos = 0,
        .cs_ena_pretrans = 0,
        .cs_ena_posttrans = 0,
        .clock_speed_hz = 1000000,  // 1 MHz
        .input_delay_ns = 0,
        .spics_io_num = cs_pin,
        .flags = 0,
        .queue_size = 1,
    };

    ret = spi_bus_add_device(SPI3_HOST, &dev_cfg, &spi_handle_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add SPI device: %s", esp_err_to_name(ret));
        return false;
    }

    // Initialize MAX7219 registers
    // Decode mode: no decode
    WriteRegister(0x09, 0x00);
    // Intensity: medium brightness
    WriteRegister(0x0A, 0x08);
    // Scan limit: all 8 digits
    WriteRegister(0x0B, 0x07);
    // Shutdown: normal operation
    WriteRegister(0x0C, 0x01);
    // Display test: off
    WriteRegister(0x0F, 0x00);

    TurnOff();

    ESP_LOGI(TAG, "MAX7219 initialized successfully");
    return true;
}

void Max7219::WriteRegister(uint8_t address, uint8_t data) {
    uint8_t tx_data[4] = {
        address, data,
        address, data  // Send to both matrices
    };
    SendSPI(tx_data, 4);
}

void Max7219::SendSPI(const uint8_t* data, int len) {
    if (!spi_handle_) return;

    spi_transaction_t t = {
        .length = len * 8,
        .tx_buffer = data,
        .rx_buffer = nullptr,
    };
    spi_device_transmit(spi_handle_, &t);
}

void Max7219::SetExpression(EyeExpression expression) {
    SetExpression(expression, expression);
}

void Max7219::SetExpression(EyeExpression left_eye, EyeExpression right_eye) {
    current_expression_ = left_eye;  // Store primary expression
    animation_frame_ = 0;
    animation_counter_ = 0;

    uint8_t left_pattern[8];
    uint8_t right_pattern[8];

    GetExpressionPattern(left_eye, 0, true, left_pattern);
    GetExpressionPattern(right_eye, 0, false, right_pattern);

    DisplayBitmap(0, left_pattern);
    DisplayBitmap(1, right_pattern);

    ESP_LOGI(TAG, "Set expression - Left: %d, Right: %d", left_eye, right_eye);
}

bool Max7219::Update() {
    if (current_expression_ == kExpressionOff) {
        return false;
    }

    animation_counter_++;

    // Update animation every 10 calls (adjust speed here)
    if (animation_counter_ < 10) {
        return true;
    }

    animation_counter_ = 0;
    animation_frame_ = (animation_frame_ + 1) % 4;

    uint8_t pattern[8];
    GetExpressionPattern(current_expression_, animation_frame_, true, pattern);
    DisplayBitmap(0, pattern);
    DisplayBitmap(1, pattern);

    return true;
}

void Max7219::SetBrightness(uint8_t brightness) {
    if (brightness > 15) brightness = 15;
    WriteRegister(0x0A, brightness);
}

void Max7219::TurnOff() {
    memset(current_frame_, 0, sizeof(current_frame_));
    for (int row = 0; row < 8; row++) {
        WriteRegister(0x01 + row, 0x00);
    }
}

void Max7219::DisplayBitmap(int matrix_index, const uint8_t* bitmap) {
    if (!spi_handle_ || !bitmap) return;

    for (int row = 0; row < 8; row++) {
        uint8_t register_addr = 0x01 + row;  // Row registers: 0x01-0x08
        uint8_t tx_data[4];

        if (matrix_index == 0) {
            // Left matrix
            tx_data[0] = register_addr;
            tx_data[1] = bitmap[row];
            tx_data[2] = register_addr;
            tx_data[3] = 0x00;  // Right matrix off
        } else {
            // Right matrix
            tx_data[0] = register_addr;
            tx_data[1] = 0x00;  // Left matrix off
            tx_data[2] = register_addr;
            tx_data[3] = bitmap[row];
        }

        SendSPI(tx_data, 4);
    }
}

void Max7219::GetExpressionPattern(EyeExpression expression, int frame, bool is_left, uint8_t* pattern) {
    if (!pattern) return;

    switch (expression) {
        case kExpressionNeutral:
            memcpy(pattern, is_left ? PATTERN_NEUTRAL_LEFT : PATTERN_NEUTRAL_RIGHT, 8);
            break;

        case kExpressionListening:
            memcpy(pattern, is_left ? PATTERN_LISTENING_LEFT : PATTERN_LISTENING_RIGHT, 8);
            break;

        case kExpressionThinking:
            // Blink animation
            if (frame % 2 == 0) {
                memcpy(pattern, is_left ? PATTERN_NEUTRAL_LEFT : PATTERN_NEUTRAL_RIGHT, 8);
            } else {
                memcpy(pattern, PATTERN_THINKING_CLOSED, 8);
            }
            break;

        case kExpressionSpeaking:
            // Animate speaking
            if (frame % 2 == 0) {
                memcpy(pattern, PATTERN_SPEAKING, 8);
            } else {
                memcpy(pattern, is_left ? PATTERN_NEUTRAL_LEFT : PATTERN_NEUTRAL_RIGHT, 8);
            }
            break;

        case kExpressionHappy:
            memcpy(pattern, PATTERN_HAPPY, 8);
            break;

        case kExpressionError:
            memcpy(pattern, PATTERN_ERROR, 8);
            break;

        case kExpressionOff:
        default:
            memset(pattern, 0x00, 8);
            break;
    }
}
