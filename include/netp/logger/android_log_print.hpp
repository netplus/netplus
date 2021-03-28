#ifndef _NETP_ANDROID_LOG_PRINT_HPP_
#define _NETP_ANDROID_LOG_PRINT_HPP_

#include <android/log.h>
#include <netp/logger/logger_abstract.hpp>

namespace netp { namespace logger {
	class android_log_print: public logger_abstract {

	public:

		android_log_print();
		~android_log_print();

		void write( log_mask const& mask, char const* log, netp::u32_t len ) ;
	};
}}
#endif
