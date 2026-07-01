#ifndef ANIMATED_EYES_H
#define ANIMATED_EYES_H

#include "device_state.h"
#include "max7219/max7219.h"
#include <memory>

/**
 * AnimatedEyes - Manages animated LED eyes based on device state
 * Uses MAX7219 LED matrix driver
 */
class AnimatedEyes {
public:
    AnimatedEyes();
    ~AnimatedEyes();

    /**
     * Initialize animated eyes component
     * @param cs_pin Chip Select GPIO
     * @param clk_pin Clock GPIO
     * @param din_pin Data In GPIO
     * @return true if successful
     */
    bool Initialize(int cs_pin, int clk_pin, int din_pin);

    /**
     * Update eye expression based on device state
     * @param state Current device state
     */
    void OnStateChanged(DeviceState old_state, DeviceState new_state);

    /**
     * Update animation (call from main loop)
     */
    void Update();

    /**
     * Set brightness (0-15)
     */
    void SetBrightness(uint8_t brightness);

    /**
     * Turn off eyes
     */
    void TurnOff();

    /**
     * Check if eyes are enabled
     */
    bool IsEnabled() const { return enabled_; }

private:
    std::unique_ptr<Max7219> matrix_;
    bool enabled_ = false;

    /**
     * Convert device state to eye expression
     */
    Max7219::EyeExpression GetExpressionForState(DeviceState state);
};

#endif // ANIMATED_EYES_H
