#ifndef _NETP_LOG_LOGGER_MANAGER_HPP_
#define _NETP_LOG_LOGGER_MANAGER_HPP_

#include <vector>

#include <netp/singleton.hpp>
#include <netp/smart_ptr.hpp>
#include <netp/logger/logger_abstract.hpp>

namespace netp {

	class logger_broker:
		public netp::singleton<netp::logger_broker>
	{
	public:
		logger_broker();
		virtual ~logger_broker();

		void init(); //ident for linux
		void deinit();

		void add( NRP<logger::logger_abstract> const& logger);
		void remove(NRP<logger::logger_abstract> const& logger);

		void write(logger::log_mask mask,char const* const file, int line, char const* const func, ... );
	private:
		std::vector< NRP<logger::logger_abstract> > m_loggers;
		bool m_isInited;
	};
}

#define NETP_LOGGER_BROKER	( netp::logger_broker::instance() )

#define	NETP_DEBUG(...)			(NETP_LOGGER_BROKER->write( netp::logger::LOG_MASK_DEBUG, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__ ))
#define	NETP_INFO(...)				(NETP_LOGGER_BROKER->write( netp::logger::LOG_MASK_INFO, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__ ))
#define	NETP_WARN(...)			(NETP_LOGGER_BROKER->write( netp::logger::LOG_MASK_WARN, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__ ))
#define	NETP_ERR(...)				(NETP_LOGGER_BROKER->write( netp::logger::LOG_MASK_ERR, __FILE__, __LINE__, __FUNCTION__,__VA_ARGS__ ))

#ifndef _NETP_DEBUG
	#undef NETP_DEBUG
	#define NETP_DEBUG(...)
#endif

#endif