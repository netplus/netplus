#include <netp/core.hpp>
#include <netp/string.hpp>
#include <netp/logger_broker.hpp>

#ifdef NETP_ENABLE_ANDROID_LOG
	#include <netp/log/android_log_print.hpp>
#endif

#include <netp/logger/console_logger.hpp>
#include <netp/logger/file_logger.hpp>

#ifdef _NETP_GNU_LINUX
	#include <netp/logger/sys_logger.hpp>
#endif

#include <netp/thread.hpp>

namespace netp {

	logger_broker::logger_broker() :
		m_loggers(),
		m_isInited(false)
	{
		init();
	}

	logger_broker::~logger_broker() {
		deinit();
	}

	void logger_broker::init() {
		if( m_isInited ) {
			return ;
		}
		m_isInited = true;
	}

	void logger_broker::deinit() {
		m_isInited = false;
		m_loggers.clear();
	}

	void logger_broker::add(NRP<logger::logger_abstract> const& logger) {
		m_loggers.push_back(logger);
	}

	void logger_broker::remove(NRP<logger::logger_abstract> const& logger) {
		std::vector< NRP<logger::logger_abstract> >::iterator&& it = m_loggers.begin();

		while (it != m_loggers.end()) {
			if ((*it) == logger) {
				it = m_loggers.erase(it);
			} else {
				++it;
			}
		}
	}

#define LOG_BUFFER_SIZE_MAX		(8192)
#define TRACE_INFO_SIZE 1024

	void logger_broker::write(logger::log_mask mask, char const* const file, int line, char const* const func, ...) {

#if defined(_NETP_DEBUG)
		char __traceInfo[TRACE_INFO_SIZE] = { 0 };
		size_t tracelen = snprintf(__traceInfo, ((sizeof(__traceInfo) / sizeof(__traceInfo[0])) - 1), "TRACE: %s:[%d] %s", file, line, func);
		if (tracelen > TRACE_INFO_SIZE) {
			tracelen = TRACE_INFO_SIZE-1;
		}
#endif

		(void)file;
		(void)line;
		NETP_ASSERT(m_isInited);
		const netp::u64_t tid = netp::this_thread::get_id();
		const ::size_t lc = m_loggers.size();
		for(::size_t i=0;i<lc;++i) {
			if (!m_loggers[i]->test_mask(mask)) { continue; }
			NETP_ASSERT(m_loggers[i] != nullptr);

			char log_buffer[LOG_BUFFER_SIZE_MAX] = { 0 };
			std::string local_time_str;
			netp::curr_localtime_str(local_time_str);
			int idx_tid = 0;
			int snwrite = snprintf(log_buffer + idx_tid, LOG_BUFFER_SIZE_MAX - idx_tid, "[%s][%c][%llu]", local_time_str.c_str(), logger::__log_mask_char[mask], tid);
			if (snwrite == -1) {
				NETP_THROW("snprintf failed for loggerManager::write");
			}
			NETP_ASSERT(snwrite < (LOG_BUFFER_SIZE_MAX - idx_tid));
			idx_tid += snwrite;

			int idx_fmt = idx_tid;

			va_list valist;
			va_start(valist, func);
			char* fmt;
			fmt = va_arg(valist, char*);
			int fmtsize = vsnprintf(log_buffer + idx_fmt, LOG_BUFFER_SIZE_MAX - idx_fmt, fmt, valist);
			va_end(valist);

			NETP_ASSERT(fmtsize != -1);
			//@refer to: https://linux.die.net/man/3/vsnprintf
			if (fmtsize >= (LOG_BUFFER_SIZE_MAX-idx_fmt)) {
				//truncated
				fmtsize = (LOG_BUFFER_SIZE_MAX - idx_fmt) - 1;
			} 

			idx_fmt += fmtsize;
			NETP_ASSERT(idx_fmt < LOG_BUFFER_SIZE_MAX);

#if defined(_NETP_DEBUG)
			netp::size_t trace_len = netp::strlen(__traceInfo)+5; //...+\n+info+'\0' 
			NETP_ASSERT(LOG_BUFFER_SIZE_MAX > trace_len);
			if ( (netp::size_t)(LOG_BUFFER_SIZE_MAX) < (trace_len +idx_fmt)) {
				int tsnsize = snprintf((log_buffer + (LOG_BUFFER_SIZE_MAX- trace_len)+3 ), (trace_len), "\n%s", __traceInfo);
				(void)&tsnsize;

				log_buffer[LOG_BUFFER_SIZE_MAX - trace_len + 0] = '.';
				log_buffer[LOG_BUFFER_SIZE_MAX - trace_len + 1] = '.';
				log_buffer[LOG_BUFFER_SIZE_MAX - trace_len + 2] = '.';
				idx_fmt = (LOG_BUFFER_SIZE_MAX);
			} else {
				int tsnsize = snprintf((log_buffer + idx_fmt), LOG_BUFFER_SIZE_MAX - idx_fmt, "\n%s", __traceInfo);
				idx_fmt += tsnsize;
			}
#endif

			m_loggers[i]->write( mask, log_buffer, idx_fmt);

			(void)file;
			(void)func;
		}
	}
}
