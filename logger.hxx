#ifndef _LOGGER_HXX_
#define _LOGGER_HXX_

namespace cornerstone {
	class logger {
	public:
		logger() {}

		virtual ~logger() {}

		__nocopy__(logger)

	public:
		virtual void debug(const std::string& log_line) = 0;
		virtual void info(const std::string& log_line) = 0;
		virtual void warn(const std::string& log_line) = 0;
		virtual void err(const std::string& log_line) = 0;
	};
}

#endif //_LOGGER_HXX_