#include "serial_device_network.hpp"
#include <iostream>

SerialDeviceNetwork::SerialDeviceNetwork(const std::string& ip, int port, bool is_server)
    : is_server(is_server), last_bit(1)
{
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2,2), &wsaData);
#endif

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) throw std::runtime_error("socket failed");
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip.c_str());

    if (is_server) {
        bind(sockfd, (sockaddr*)&addr, sizeof(addr));
        listen(sockfd, 1);
        int clientfd = accept(sockfd, nullptr, nullptr);
        if (clientfd < 0) throw std::runtime_error("accept failed");
        // Use clientfd for comms, close sockfd later
        sockfd = clientfd;
    } else {
        if (connect(sockfd, (sockaddr*)&addr, sizeof(addr)) < 0)
            throw std::runtime_error("connect failed");
    }
}

SerialDeviceNetwork::~SerialDeviceNetwork() {
#ifdef _WIN32
    closesocket(sockfd);
    WSACleanup();
#else
    close(sockfd);
#endif
}

bool SerialDeviceNetwork::send() {
    // Send last_bit to peer
    send(sockfd, (char*)&last_bit, 1, 0);
    // Receive bit from peer
    uint8_t peer_bit = 1;
    recv(sockfd, (char*)&peer_bit, 1, MSG_WAITALL);
    return peer_bit;
}

void SerialDeviceNetwork::receive(bool bit) {
    last_bit = bit;
}
