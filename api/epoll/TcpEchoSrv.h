#pragma once

#include <stdint.h>

#include <string>
#include <thread>
#include <vector>
#include <unordered_map>

class TcpEchoSrv
{
    static const int MAX_LISTEN_NUM = 1024;
    static const int MAX_LINE_LEN = 1024;
    static const int MAX_EVENTS = 1024;

    typedef struct
    {
        int event = 0;
        int len = 0;
        char buf[MAX_LINE_LEN] = {};
    } client_ctx;

    std::string ip;
    uint16_t port;
    std::unordered_map<int, client_ctx *> clients;
    std::vector<client_ctx *> pool;
    std::thread srvThread;
    int srvFd = -1;
    int efd = -1;
    int epollFd = -1;

    client_ctx *get_client_ctx();
    void accept_client();
    void do_read(int fd);
    void do_write(int fd);
    void cleanup_client(int fd, bool erase=true);
    void serve();

public:
    TcpEchoSrv(const std::string &ip, uint16_t port): ip(ip), port(port) {}
    ~TcpEchoSrv();

    void start();
    void stop();
};

