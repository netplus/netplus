#ifndef _NETP_LOG_SYSLOG_LOGGER_HPP_
#define _NETP_LOG_SYSLOG_LOGGER_HPP_

#include <netp/core.hpp>

#ifdef _NETP_GNU_LINUX
#define NETP_IDENT_LENGTH 32

#include <netp/logger/logger_abstract.hpp>

	namespace netp { namespace logger {
		class sys_logger: public logger_abstract {

		public:

			sys_logger( char const* const ident );
			~sys_logger();

			/**
			 *  syslog has a 1024 len limit, so we'll split log into piece if it exceed 1024
			 */
			void write( log_mask level, char const* log, netp::u32_t len  ) = 0 ;

		private:
			static void Syslog( int level, char const* log, netp::u32_t len );
			char m_ident[NETP_IDENT_LENGTH] ;
		};
	}}

#endif
#endif
