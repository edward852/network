#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/ip.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <thread>
#include <iostream>

#include "log.h"
#include "TcpEchoSrv.h"

using namespace std;
using namespace log;

TcpEchoSrv::~TcpEchoSrv()
{
    if (0 < srvFd)
    {
        LOG(INFO) << "cleanup server fd: " << srvFd;
        close(srvFd);
        srvFd = -1;
    }
    if (0 < efd)
    {
        LOG(INFO) << "cleanup event fd: " << efd;
        close(efd);
        efd = -1;
    }
    for (auto fd: cliFds)
    {
        LOG(INFO) << "cleanup client fd: " << fd;
        close(fd);
    }
}

void TcpEchoSrv::accept_client()
{
    struct sockaddr_in addr = {};
    socklen_t len = sizeof(addr);
    int fd = -1;

    fd = accept(srvFd, (struct sockaddr *)&addr, &len);
    if (0 > fd)
    {
        LOG(ERROR) << "accept client failed for " << strerror(errno);
        return ;
    }
    LOG(INFO) << "accepted client fd: " << fd;

    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    cliFds.push_back(fd);
}

void TcpEchoSrv::deal_with_client()
{
    for (int idx=0; idx<cliFds.size(); idx++)
    {
        int fd = cliFds[idx];
        char buffer[MAX_LINE_LEN] = {};

        if (!FD_ISSET(fd, &fds)) continue;

        LOG(INFO) << "readable client fd: " << fd;
        int num = read(fd, buffer, sizeof(buffer)-1);

        if (0 >= num)
        {
            LOG(WARN) << "closed client fd: " << fd;
            close(fd);
            cliFds[idx] = cliFds.back();
            cliFds.pop_back();
            idx--;
        }
        else
        {
            write(fd, buffer, num);
            // todo: check write return value
            buffer[num] = '\0';
            LOG(INFO) << "responsed client fd: " << fd << " content: "  << buffer;
        }
    }
}

void TcpEchoSrv::serve()
{
    // cleanup will be done in ~TcpEchoSrv()
    srvFd = socket(AF_INET, SOCK_STREAM, 0);
    if (0 > srvFd)
    {
        LOG(ERROR) << "create server socket failed for " << strerror(errno);
        return ;
    }
    LOG(INFO) << "server socket: " << srvFd;

    efd = eventfd(0, 0);
    if (0 > efd)
    {
        LOG(ERROR) << "create event fd failed for " << strerror(errno);
        return ;
    }
    LOG(INFO) << "event fd: " << efd;

    int opt = 1;
    setsockopt(srvFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    int flags = fcntl(srvFd, F_GETFL, 0);
    fcntl(srvFd, F_SETFL, flags | O_NONBLOCK);

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);
    addr.sin_port = htons(port);

    if (0 > bind(srvFd, (struct sockaddr*)&addr, sizeof(addr)))
    {
        LOG(ERROR) << "bind failed for " << strerror(errno);
        return ;
    }

    if (0 > listen(srvFd, MAX_LISTEN_NUM))
    {
        LOG(ERROR) << "listen failed for " << strerror(errno);
        return ;
    }
    LOG(INFO) << "listening " << ip << ":" << port;

    while (true)
    {
        struct timeval tv = { .tv_sec=30 };
        int maxFd = -1;
        int ret = 0;

        FD_ZERO(&fds);

        FD_SET(srvFd, &fds);
        maxFd = srvFd;

        FD_SET(efd, &fds);
        maxFd = max(maxFd, efd);

        for (auto fd: cliFds)
        {
            FD_SET(fd, &fds);
            maxFd = max(maxFd, fd);
        }

        ret = select(maxFd+1, &fds, nullptr, nullptr, &tv);
        LOG(INFO) << "select return " << ret;
        if (0 == ret) continue; // timeout

        if (0 > ret)
        {
            LOG(ERROR) << "select failed for " << strerror(errno);
            break;
        }

        if (FD_ISSET(srvFd, &fds))
        {
            accept_client();
        }
        else if (FD_ISSET(efd, &fds))
        {
            LOG(WARN) << "received shutdown event";
            break;
        }
        else
        {
            deal_with_client();
        }
    }
}

void TcpEchoSrv::start()
{
    srvThread = thread(&TcpEchoSrv::serve, ref(*this));
}

void TcpEchoSrv::stop()
{
    uint64_t num = 1;

    write(efd, &num, sizeof(num));
    srvThread.join();
}
