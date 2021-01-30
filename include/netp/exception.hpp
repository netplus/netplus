#ifndef _NETP_EXCEPTION_HPP_
#define _NETP_EXCEPTION_HPP_

#include <netp/core/platform.hpp>

#ifdef _NETP_GNU_LINUX
	#include <execinfo.h>
#endif

#define NETP_EXCEPTION_MESSAGE_LENGTH_LIMIT 1024
#define NETP_EXCEPTION_FILE_LENGTH_LIMIT 512
#define NETP_EXCEPTION_FUNC_LENGTH_LIMIT 512

namespace netp {

	extern void stack_trace( char stack_buffer[], u32_t const& s );

	class exception;
	void __NETP_EXCEPTION_INIT__(netp::exception* e, int code_, char const* const sz_message_, char const* const sz_file_, int line_, char const* const sz_func_, char const* const stack_info_);

	class exception {
	protected:
		int _code;
		int _line;
		char _message[NETP_EXCEPTION_MESSAGE_LENGTH_LIMIT];
		char _file[NETP_EXCEPTION_FILE_LENGTH_LIMIT];
		char _func[NETP_EXCEPTION_FUNC_LENGTH_LIMIT];

		char* _callstack;

		friend void __NETP_EXCEPTION_INIT__(netp::exception* e, int code_, char const* const sz_message_, char const* const sz_file_, int line_, char const* const sz_func_, char const* const stack_info_);
	public:
		explicit exception(int code, char const* const sz_message_, char const* const sz_file_ , int const& line_ , char const* const sz_func_ );
		virtual ~exception();

		exception(exception const& other);
		exception& operator= (exception const& other);

		inline const int code() const {
			return _code;
		}
		inline const int line() const {
			return _line;
		}
		inline const char* what() const {
			return _message;
		}
		inline const char* file() const {
			return _file;
		}
		inline const char* function() const {
			return _func;
		}
		inline const char* callstack() const {
			return _callstack;
		}
	};
}

#define NETP_THROW2(code,msg) throw netp::exception(code,msg,__FILE__, __LINE__,__FUNCTION__ );
#define NETP_THROW(msg) NETP_THROW2(netp::E_NETP_THROW,msg)
#define NETP_TODO(msg) NETP_THROW2(netp::E_TODO, "TODO:" #msg)

#define NETP_CONDITION_CHECK(statement) \
	do { \
		if(!(statement)) { \
			NETP_THROW( "statement ("#statement ") check failed exception" ); \
		} \
	} while(0);
#endif