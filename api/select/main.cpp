#include <signal.h>
#include <unistd.h>

#include <iostream>
#include "log.h"
#include "TcpEchoSrv.h"

using namespace std;
using namespace log;

Cfg g_log_cfg = { .level=ERROR, .prefix=true };
static bool l_interrupted = false;

void intHandler(int sigNum)
{
	LOG(WARN) << "interrupted! shutting down server...";
	l_interrupted = true;
}

int printHelp(char *prog)
{
	cout << "Usage: " << prog << " [option]" << endl;
	cout << "Options:" << endl;
	cout << "    -v verbose" << endl;

	return -1;
}

int main(int argc, char **argv)
{
	char ch = ' ';

	while ((ch = getopt(argc, argv, "v")) != -1)
	{
		switch (ch)
		{
		case 'v':
			g_log_cfg.level = INFO;
			break;
		default:
			return printHelp(argv[0]);
			break;
		}
	}

	struct sigaction act = {};

	act.sa_handler = intHandler;
	sigaction(SIGINT, &act, nullptr);

	TcpEchoSrv server("127.0.0.1", 1080);

	server.start();
	while (true)
	{
		if (l_interrupted)
		{
			server.stop();
			break;
		}
		sleep(1);
	}

    return 0;
}

