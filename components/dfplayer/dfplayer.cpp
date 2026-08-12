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

uint16_t DFPlayer::Checksum(const uint8_t *packet, int len) {
    // checksum = 0 - (version + length + command + feedback + paramHi + paramLo)
    uint16_t sum = 0;
    // ensure len is at least 7 to include indices 1..6
    int end = (len >= 7) ? 7 : len;
    for (int i = 1; i < end; ++i) {
        sum += packet[i];
    }
    uint16_t chk = 0 - sum;
    return chk;
}

bool DFPlayer::ReadResponse(uint8_t *out_packet, int expected_len, int timeout_ms) {
    if (expected_len <= 0) return false;
    // We'll try to read expected_len bytes within timeout_ms
    int to_read = expected_len;
    int total_read = 0;
    const TickType_t tick_timeout = pdMS_TO_TICKS(timeout_ms);
    // Read once with timeout; uart_read_bytes blocks up to timeout
    int r = uart_read_bytes(uart_num_, out_packet, to_read, tick_timeout);
    if (r <= 0) {
        ESP_LOGW(TAG, "ReadResponse: no data (r=%d)", r);
        return false;
    }
    total_read = r;
    if (total_read != expected_len) {
        ESP_LOGW(TAG, "ReadResponse: expected %d bytes but got %d", expected_len, total_read);
        // still try to validate partial packet if possible
    }
    // Basic validation: start and end bytes
    if (out_packet[0] != 0x7E) {
        ESP_LOGW(TAG, "ReadResponse: invalid start byte: 0x%02X", out_packet[0]);
        return false;
    }
    if (out_packet[total_read-1] != 0xEF) {
        ESP_LOGW(TAG, "ReadResponse: invalid end byte: 0x%02X", out_packet[total_read-1]);
        return false;
    }
    // validate checksum if we have at least 9 bytes
    if (total_read >= 9) {
        uint16_t recv_chk = ((uint16_t)out_packet[7] << 8) | out_packet[8];
        uint16_t calc = Checksum(out_packet, 7);
        if (recv_chk != calc) {
            ESP_LOGW(TAG, "ReadResponse: checksum mismatch recv=0x%04X calc=0x%04X", recv_chk, calc);
            return false;
        }
    }
    return true;
}

bool DFPlayer::SendCommand(uint8_t command, uint16_t parameter, bool feedback, int retries, int timeout_ms) {
    uint8_t packet[10];
    packet[0] = 0x7E;           // start
    packet[1] = 0xFF;           // version
    packet[2] = 0x06;           // length
    packet[3] = command;        // command
    packet[4] = feedback ? 0x01 : 0x00; // feedback flag
    packet[5] = (parameter >> 8) & 0xFF; // param high
    packet[6] = parameter & 0xFF;        // param low
    uint16_t chk = Checksum(packet, 7);
    packet[7] = (chk >> 8) & 0xFF;
    packet[8] = chk & 0xFF;
    packet[9] = 0xEF;           // end

    for (int attempt = 0; attempt < retries; ++attempt) {
        int written = uart_write_bytes(uart_num_, reinterpret_cast<const char*>(packet), sizeof(packet));
        if (written != sizeof(packet)) {
            ESP_LOGW(TAG, "SendCommand: uart_write_bytes wrote %d of %d (attempt %d)", written, (int)sizeof(packet), attempt+1);
            vTaskDelay(pdMS_TO_TICKS(50));
            continue; // retry
        }

        if (feedback) {
            uint8_t resp[16];
            // Expect 10 bytes response
            bool ok = ReadResponse(resp, 10, timeout_ms);
            if (ok) {
                ESP_LOGI(TAG, "SendCommand: got valid response for cmd 0x%02X", command);
                return true;
            } else {
                ESP_LOGW(TAG, "SendCommand: invalid response, attempt %d", attempt+1);
                vTaskDelay(pdMS_TO_TICKS(50));
                continue; // retry
            }
        } else {
            // No feedback requested; assume success
            return true;
        }
    }
    ESP_LOGE(TAG, "SendCommand: failed after %d attempts", retries);
    return false;
}

bool DFPlayer::PlayTrack(uint16_t index) {
    // command 0x03 = play track by index
    bool ok = SendCommand(0x03, index, true, 3, 300);
    ESP_LOGI(TAG, "PlayTrack %u -> %s", index, ok ? "OK" : "ERR");
    return ok;
}

bool DFPlayer::Stop() {
    return SendCommand(0x16, 0, true, 3, 200);
}

bool DFPlayer::Pause() {
    return SendCommand(0x0E, 0, true, 3, 200);
}

bool DFPlayer::Resume() {
    return SendCommand(0x0D, 0, true, 3, 200);
}

bool DFPlayer::SetVolume(uint8_t volume) {
    if (volume > 30) volume = 30;
    return SendCommand(0x06, volume, true, 3, 200);
}
