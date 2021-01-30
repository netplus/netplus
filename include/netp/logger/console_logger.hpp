#ifndef _NETP_LOGGER_CONSOLE_HPP_
#define _NETP_LOGGER_CONSOLE_HPP_

#include <netp/logger/logger_abstract.hpp>

namespace netp { namespace logger {
	class console_logger : public logger_abstract {
		NETP_DECLARE_NONCOPYABLE(console_logger)

	public:
		console_logger();
		~console_logger();

		void write( log_mask mask, char const* log, netp::size_t len ) ;
	};
}}

#endif //_NETP_CONSOLE_LOGGER_H_
