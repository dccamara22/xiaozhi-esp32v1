#include "dfplayer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char* TAG = "dfplayer_task";

extern "C" void dfplayer_task(void* arg) {
    // UART1 pins for ESP32-S3 DevKit — adjust if your board requires different pins
    const int tx_pin = 17; // connect DFPlayer RX to this pin (with level shifter if needed)
    const int rx_pin = 16; // connect DFPlayer TX to this pin

    DFPlayer player(UART_NUM_1, tx_pin, rx_pin);
    if (!player.Init(9600)) {
        ESP_LOGE(TAG, "DFPlayer init failed");
        vTaskDelete(NULL);
        return;
    }

    // Ajusta el volumen (0-30)
    player.SetVolume(20);

    // Espera a que el DFPlayer esté listo si es necesario
    vTaskDelay(pdMS_TO_TICKS(500));

    // Reproducir pista #1 (archivo 0001.mp3 en la TF card)
    player.PlayTrack(1);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

extern "C" void start_dfplayer_task(void) {
    xTaskCreate(dfplayer_task, "dfplayer_task", 4096, NULL, 5, NULL);
}
