#ifndef MAX7219_H
#define MAX7219_H

#include <cstdint>
#include <cstring>
#include <driver/spi_master.h>

/**
 * MAX7219 LED Matrix Driver
 * Controls 8x8 LED matrices via SPI
 * Support for 2 matrices (left and right eyes)
 */
class Max7219 {
public:
    // Eye expressions
    enum EyeExpression {
        kExpressionNeutral = 0,    // Neutral/resting
        kExpressionListening,      // Focused/listening
        kExpressionThinking,       // Blinking/thinking
        kExpressionSpeaking,       // Speaking/animated
        kExpressionHappy,          // Happy/smiling
        kExpressionError,          // Error/alert
        kExpressionOff             // Turned off
    };

    Max7219();
    ~Max7219();

    /**
     * Initialize MAX7219 with SPI pins
     * @param cs_pin Chip Select GPIO
     * @param clk_pin Clock GPIO
     * @param din_pin Data In GPIO
     * @return true if successful
     */
    bool Initialize(int cs_pin, int clk_pin, int din_pin);

    /**
     * Set expression for both eyes
     * @param expression Expression enum
     */
    void SetExpression(EyeExpression expression);

    /**
     * Set expression for specific eye
     * @param left_eye Left eye expression
     * @param right_eye Right eye expression
     */
    void SetExpression(EyeExpression left_eye, EyeExpression right_eye);

    /**
     * Update animation frame (call periodically for animations)
     * @return true if animation is running
     */
    bool Update();

    /**
     * Set brightness (0-15)
     * @param brightness Level 0-15
     */
    void SetBrightness(uint8_t brightness);

    /**
     * Turn off all LEDs
     */
    void TurnOff();

private:
    spi_device_handle_t spi_handle_ = nullptr;
    uint8_t current_frame_[16] = {};
    EyeExpression current_expression_ = kExpressionOff;
    int animation_frame_ = 0;
    int animation_counter_ = 0;

    /**
     * Write data to MAX7219 register
     * @param address Register address
     * @param data Data to write
     */
    void WriteRegister(uint8_t address, uint8_t data);

    /**
     * Send SPI data
     * @param data Data buffer (2 bytes per matrix)
     * @param len Length of data
     */
    void SendSPI(const uint8_t* data, int len);

    /**
     * Get bitmap pattern for expression
     * @param expression Expression type
     * @param frame Animation frame number
     * @param is_left Is left eye (vs right)
     * @return 8-byte bitmap pattern
     */
    void GetExpressionPattern(EyeExpression expression, int frame, bool is_left, uint8_t* pattern);

    /**
     * Display bitmap on matrix
     * @param matrix_index 0 for left, 1 for right
     * @param bitmap 8 bytes representing 8 rows
     */
    void DisplayBitmap(int matrix_index, const uint8_t* bitmap);
};

#endif // MAX7219_H
