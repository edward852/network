#pragma once

#include <stdint.h>

#include <string>
#include <thread>
#include <vector>

class TcpEchoSrv
{
    static const int MAX_LISTEN_NUM = 1024;
    static const int MAX_LINE_LEN = 1024;

    std::string ip;
    uint16_t port;
    std::vector<int> cliFds;
    std::thread srvThread;
    fd_set fds;
    int srvFd = -1;
    int efd = -1;

    void accept_client();
    void deal_with_client();
    void serve();
public:
    TcpEchoSrv(const std::string &ip, uint16_t port): ip(ip), port(port) {}
    ~TcpEchoSrv();

    void start();
    void stop();
};

