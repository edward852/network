#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/ip.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <thread>
#include <iostream>

#include "log.h"
#include "TcpEchoSrv.h"

using namespace std;
using namespace log;

static void adjust_event(int epollFd, int op, int fd, int event)
{
    struct epoll_event ev = {};

    ev.events = event;
    ev.data.fd = fd;
    epoll_ctl(epollFd, op, fd, &ev);
}

static inline void add_event(int epollFd, int fd, int event)
{
	return adjust_event(epollFd, EPOLL_CTL_ADD, fd, event);
}

static inline void del_event(int epollFd, int fd, int event)
{
	return adjust_event(epollFd, EPOLL_CTL_DEL, fd, event);
}

static inline void mod_event(int epollFd, int fd, int event)
{
	return adjust_event(epollFd, EPOLL_CTL_MOD, fd, event);
}

void TcpEchoSrv::cleanup_client(int fd, bool erase)
{
	LOG(INFO) << "cleanup for client fd: " << fd;

	auto ctx = clients[fd];

	del_event(epollFd, fd, ctx->event);
	close(fd);

	pool.push_back(ctx);

	if (erase)
	{
		clients.erase(fd);
	}
}

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
	if (0 < epollFd)
	{
		LOG(INFO) << "cleanup epoll fd: " << epollFd;
		close(epollFd);
		epollFd = -1;
	}
	for (auto kv: clients)
	{
		cleanup_client(kv.first, false);
	}

	for (auto ctx: pool)
	{
		LOG(INFO) << "releasing memory of pool";
		delete ctx;
	}
}

TcpEchoSrv::client_ctx *TcpEchoSrv::get_client_ctx()
{
	client_ctx *buf = nullptr;

	if (!pool.empty())
	{
		buf = pool.back();
		pool.pop_back();

		LOG(INFO) << "allocated client_ctx from pool";
		return buf;
	}

	buf = new (nothrow) client_ctx;

	LOG(INFO) << "allocated client_ctx using new";
	return buf;
}

void TcpEchoSrv::accept_client()
{
	struct sockaddr_in addr = {};
	socklen_t len = sizeof(addr);
	int fd = -1;
	client_ctx *ctx = get_client_ctx();

	if (!ctx)
	{
		LOG(ERROR) << "allocate client context failed";
		return;
	}

	fd = accept(srvFd, (struct sockaddr *)&addr, &len);
	if (0 > fd)
	{
		pool.push_back(ctx); // for later use
		LOG(ERROR) << "accept client failed for " << strerror(errno);
		return ;
	}

	LOG(INFO) << "accepted client fd: " << fd;

	int	flags = fcntl(fd, F_GETFL, 0);
	fcntl(fd, F_SETFL, flags | O_NONBLOCK);

	ctx->event = EPOLLIN;
	clients[fd] = ctx;
	add_event(epollFd, fd, EPOLLIN);
}

void TcpEchoSrv::do_read(int fd)
{
	client_ctx *ctx = clients[fd];
	char *rbuf = ctx->buf;

	LOG(INFO) << "readable client fd: " << fd;
	int num = read(fd, rbuf, MAX_LINE_LEN-1);

	if (0 >= num)
	{
		LOG(WARN) << (0>num? "read failed. ": "") << "closed client fd: " << fd;

		cleanup_client(fd);
		return ;
	}

	rbuf[num] = '\0';
	LOG(INFO) << "received client fd: " << fd << " content: "  << rbuf;

	ctx->len = num;
	ctx->event = EPOLLOUT;
	mod_event(epollFd, fd, EPOLLOUT);
}

void TcpEchoSrv::do_write(int fd)
{
	LOG(INFO) << "writable client fd: " << fd;

	auto ctx = clients[fd];
	char *wbuf = ctx->buf;
	int len = ctx->len;

	while (0 < len)
	{
		int num = write(fd, wbuf, len);

		if (0 >= num)
		{
			if (EWOULDBLOCK == errno)
			{
				LOG(WARN) << "write later for client fd: " << fd;
				return;
			}

			LOG(ERROR) << "write failed for " << strerror(errno);
			cleanup_client(fd);
			return ;
		}

		if (num == len)
		{
			LOG(INFO) << "write finished for client fd: " << fd;
			ctx->event = EPOLLIN;
			mod_event(epollFd, fd, EPOLLIN);
			return ;
		}

		wbuf += num;
		len -= num;
	}
}

void TcpEchoSrv::serve()
{
	array<struct epoll_event, MAX_EVENTS> events = {};

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

	epollFd = epoll_create1(0);
	if (0 > epollFd)
	{
		LOG(ERROR) << "create epoll fd failed for " << strerror(errno);
		return ;
	}
	LOG(INFO) << "epoll fd: " << epollFd;

	int opt = 1;
	setsockopt(srvFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	int	flags = fcntl(srvFd, F_GETFL, 0);
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

	add_event(epollFd, srvFd, EPOLLIN);
	add_event(epollFd, efd, EPOLLIN);
    while (true)
	{
		int ret = epoll_wait(epollFd, &events[0], MAX_EVENTS, -1);

		LOG(INFO) << "epoll return " << ret;
		if (0 == ret) continue; // timeout

		if (0 > ret)
		{
			LOG(ERROR) << "epoll failed for " << strerror(errno);
			break;
		}

		for (auto &event: events)
		{
			int fd = event.data.fd;

			if (fd==srvFd && (event.events & EPOLLIN))
			{
				accept_client();
			}
			else if (fd==efd && (event.events & EPOLLIN))
			{
				LOG(WARN) << "received shutdown event";
				return ;
			}
			else
			{
				if (event.events & EPOLLIN)
				{
					do_read(fd);
				}
				if (event.events & EPOLLOUT)
				{
					do_write(fd);
				}
			}
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
