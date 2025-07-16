#pragma once
#include "serial_device.hpp"
#include <string>
#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

class SerialDeviceNetwork : public SerialDevice {
public:
    SerialDeviceNetwork(const std::string& ip, int port, bool is_server);
    ~SerialDeviceNetwork();
    bool send() override;
    void receive(bool bit) override;

private:
    int sockfd;
    bool is_server;
    uint8_t last_bit;
};
