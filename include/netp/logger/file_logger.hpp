#ifndef _NETP_LOGGER_FILE_LOGGER_HPP_
#define _NETP_LOGGER_FILE_LOGGER_HPP_

#include <netp/logger/logger_abstract.hpp>

namespace netp { namespace logger {
	class file_logger: public logger_abstract {

	public:
		file_logger(string_t const& log_file );
		~file_logger();

		void write( log_mask mask, char const* log, netp::size_t len ) ;

	private:
		FILE* m_fp;
	};
}}
#endif