#include "dfplayer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "sdkconfig.h"

static const char* TAG = "dfplayer_task";

#if CONFIG_DFPLAYER_ENABLE

static uart_port_t uart_num_from_cfg(int num) {
    if (num == 2) return UART_NUM_2;
    return UART_NUM_1;
}

extern "C" void dfplayer_task(void* arg) {
    // Get config from sdkconfig
    const int tx_pin = CONFIG_DFPLAYER_TX_PIN; // ESP TX -> DFPlayer RX
    const int rx_pin = CONFIG_DFPLAYER_RX_PIN; // ESP RX <- DFPlayer TX
    const int baud = CONFIG_DFPLAYER_BAUD;
    const int uart_cfg = CONFIG_DFPLAYER_UART_NUM;
    const int volume = CONFIG_DFPLAYER_VOLUME;

    uart_port_t uart_num = uart_num_from_cfg(uart_cfg);

    DFPlayer player(uart_num, tx_pin, rx_pin);
    if (!player.Init(baud)) {
        ESP_LOGE(TAG, "DFPlayer init failed");
        vTaskDelete(NULL);
        return;
    }

    // Ajusta el volumen (0-30)
    player.SetVolume(volume);

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

#else

// Stubs when disabled
extern "C" void start_dfplayer_task(void) {}

#endif // CONFIG_DFPLAYER_ENABLE
