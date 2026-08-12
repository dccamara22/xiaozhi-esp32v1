#ifndef DFPLAYER_H_
#define DFPLAYER_H_

#include <driver/uart.h>
#include <cstdint>

class DFPlayer {
public:
    DFPlayer(uart_port_t uart_num = UART_NUM_1, int tx_io = 17, int rx_io = 16);
    ~DFPlayer();

    bool Init(int baud_rate = 9600);
    // Commands return true on success (if feedback requested and validated)
    bool PlayTrack(uint16_t index);   // 1-based track index
    bool Stop();
    bool Pause();
    bool Resume();
    bool SetVolume(uint8_t volume);   // 0-30

    // Low-level send command with optional feedback and retries
    bool SendCommand(uint8_t command, uint16_t parameter, bool feedback = false, int retries = 3, int timeout_ms = 200);

private:
    uart_port_t uart_num_;
    int tx_io_;
    int rx_io_;

    uint16_t Checksum(const uint8_t *packet, int len);
    bool ReadResponse(uint8_t *out_packet, int expected_len, int timeout_ms);
};

#endif // DFPLAYER_H_
