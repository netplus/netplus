#include <stdio.h>
#include <fcntl.h>
#include <netp/string.hpp>
#include <netp/logger/file_logger.hpp>

namespace netp { namespace logger {

	file_logger::file_logger(string_t const& log_file ) :
		logger_abstract(),
		m_fp(nullptr)
	{
		m_fp = fopen(log_file.c_str() , "a+b" );
		if( m_fp == nullptr ) {
			char err_msg[1024] = {0};
			snprintf(err_msg, 1024,"fopen(%s)=%d", log_file.c_str(), netp_last_errno() );
			NETP_THROW(err_msg);
		}
	}

	file_logger::~file_logger() {
		if(m_fp) {
			fclose(m_fp);
		}
	}

	void file_logger::write( log_mask mask, char const* log, netp::u32_t len ) {
		NETP_ASSERT(test_mask(mask));
		::size_t nbytes = fprintf(m_fp, "%s\n", log);
		//::size_t nbytes = fwrite( log,sizeof(char), len, m_fp );
		//fwrite("\n", sizeof(char), netp::strlen("\n"), m_fp);
		fflush(m_fp);
		(void)nbytes;
		(void)len;
	}
}}