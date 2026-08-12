#include "dfplayer.h"
#include <driver/uart.h>
#include <esp_log.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>

static const char* TAG = "DFPlayer";

DFPlayer::DFPlayer(uart_port_t uart_num, int tx_io, int rx_io)
    : uart_num_(uart_num), tx_io_(tx_io), rx_io_(rx_io) {}

DFPlayer::~DFPlayer() {}

bool DFPlayer::Init(int baud_rate) {
    uart_config_t uart_config = {
        .baud_rate = baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    if (uart_param_config(uart_num_, &uart_config) != ESP_OK) {
        ESP_LOGE(TAG, "uart_param_config failed");
        return false;
    }
    if (uart_set_pin(uart_num_, tx_io_, rx_io_, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK) {
        ESP_LOGE(TAG, "uart_set_pin failed");
        return false;
    }
    // install driver with RX/TX buffers
    const int rx_buffer_size = 1024;
    const int tx_buffer_size = 1024;
    if (uart_driver_install(uart_num_, rx_buffer_size, tx_buffer_size, 0, NULL, 0) != ESP_OK) {
        ESP_LOGE(TAG, "uart_driver_install failed");
        return false;
    }
    ESP_LOGI(TAG, "DFPlayer UART initialized (uart=%d, tx=%d, rx=%d)", uart_num_, tx_io_, rx_io_);
    return true;
}

uint16_t DFPlayer::Checksum(uint8_t *packet, int len) {
    // checksum = 0 - (version + length + command + feedback + paramHi + paramLo)
    uint16_t sum = 0;
    // Ensure we don't read past provided len; packet layout expects indices 1..6
    int end = (len >= 7) ? 7 : len;
    for (int i = 1; i < end; ++i) { // indices 1..6 for version..paramLo (packet layout below)
        sum += packet[i];
    }
    uint16_t chk = 0 - sum;
    return chk;
}

void DFPlayer::SendCommand(uint8_t command, uint16_t parameter) {
    uint8_t packet[10];
    packet[0] = 0x7E;           // start
    packet[1] = 0xFF;           // version
    packet[2] = 0x06;           // length
    packet[3] = command;        // command
    packet[4] = 0x00;           // no feedback
    packet[5] = (parameter >> 8) & 0xFF; // param high
    packet[6] = parameter & 0xFF;        // param low
    uint16_t chk = Checksum(packet, 7);
    packet[7] = (chk >> 8) & 0xFF;
    packet[8] = chk & 0xFF;
    packet[9] = 0xEF;           // end

    // Write packet
    int written = uart_write_bytes(uart_num_, reinterpret_cast<const char*>(packet), sizeof(packet));
    if (written != sizeof(packet)) {
        ESP_LOGW(TAG, "uart_write_bytes wrote %d of %d", written, (int)sizeof(packet));
    }
    // small delay to let DFPlayer process
    vTaskDelay(pdMS_TO_TICKS(50));
}

bool DFPlayer::PlayTrack(uint16_t index) {
    // command 0x03 = play track by index
    SendCommand(0x03, index);
    ESP_LOGI(TAG, "PlayTrack %u", index);
    return true;
}

bool DFPlayer::Stop() {
    SendCommand(0x16, 0);
    return true;
}

bool DFPlayer::Pause() {
    SendCommand(0x0E, 0);
    return true;
}

bool DFPlayer::Resume() {
    SendCommand(0x0D, 0);
    return true;
}

bool DFPlayer::SetVolume(uint8_t volume) {
    if (volume > 30) volume = 30;
    SendCommand(0x06, volume);
    return true;
}
