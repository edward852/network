#pragma once

#include <iostream>

namespace log
{
	enum Type
	{
		INFO,
		WARN,
		ERROR,
		NONE
	};
	typedef struct
	{
		Type level = ERROR;
		bool prefix = false;
	} Cfg;
};
extern log::Cfg g_log_cfg;

namespace log
{
	class LOG
	{
		const char *color = "";
		const char *prefix = "";
		Type level = INFO;
		bool printed = false;

		void set_color_prefix()
		{
			switch (level)
			{
			case INFO:
				color = "\x1B[32m";
				prefix = "[INFO]";
				break;
			case WARN:
				color = "\x1B[33m";
				prefix = "[WARN]";
				break;
			case ERROR:
				color = "\x1B[31m";
				prefix = "[ERROR]";
				break;
			default:
				break;
			}
		}

	public:
		LOG() {}
		LOG(Type type)
		{
			level = type;
			set_color_prefix();
		}

		~LOG()
		{
			if (printed)
			{
				std::cout << "\033[0m" << std::endl;
			}
		}

		template<class T>
		LOG &operator<<(const T &msg)
		{
			if (level >= g_log_cfg.level)
			{
				std::cout << color << prefix << msg;

				printed = true;
				color = "";
				prefix = "";
			}

			return *this;
		}
	};
};
