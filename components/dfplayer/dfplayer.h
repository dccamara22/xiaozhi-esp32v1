#ifndef DFPLAYER_H_
#define DFPLAYER_H_

#include <driver/uart.h>
#include <cstdint>

class DFPlayer {
public:
    DFPlayer(uart_port_t uart_num = UART_NUM_1, int tx_io = 17, int rx_io = 16);
    ~DFPlayer();

    bool Init(int baud_rate = 9600);
    bool PlayTrack(uint16_t index);   // 1-based track index
    bool Stop();
    bool Pause();
    bool Resume();
    bool SetVolume(uint8_t volume);   // 0-30

private:
    uart_port_t uart_num_;
    int tx_io_;
    int rx_io_;

    void SendCommand(uint8_t command, uint16_t parameter);
    uint16_t Checksum(uint8_t *packet, int len);
};

#endif // DFPLAYER_H_
