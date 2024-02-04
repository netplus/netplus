#include <netp/string.hpp>
#include <netp/exception.hpp>

#if defined(_NETP_WIN) && (defined(_NETP_ARCH_X64) || defined(_NETP_ARCH_X86))
	#define _NETP_STACK_WALKER_AVAILABLE_HPP
#endif

#ifdef _NETP_STACK_WALKER_AVAILABLE_HPP
	#include "./../3rd/stack_walker/StackWalker.h"
#elif defined(_NETP_HAS_EXECINFO_H)
	#include <execinfo.h>
#elif defined(_NETP_ANDROID)
	#include <android/log.h>
#else
	#define _NETP_NO_STACK_TRACE
#endif

namespace netp {

#if defined(_NETP_ANDROID)
	void stack_trace(char* stack_buffer, u32_t s) {
		__android_log_print(ANDROID_LOG_FATAL, "NETP", "exception ..." );
	}

#elif defined(_NETP_HAS_EXECINFO_H)
	#define BUFFER_SIZE 128
	void stack_trace(char* stack_buffer, u32_t s) {
		void* callstack[BUFFER_SIZE];
		int i, frames = backtrace(callstack, BUFFER_SIZE);
		char** strs = backtrace_symbols(callstack, frames);
		if (strs == nullptr) {
			perror("backtrace_symbols failed");
			exit(EXIT_FAILURE);
		}
		u32_t p = 0;
		for (i = 0; i < frames; ++i) {
			int c = snprintf( stack_buffer+p, (s-p-1), "%s\n", strs[i] );
			if (c<0) {
				break;
			}
			if ((p + c) >= s) {
				p = s;
				break;
			}
			p += c;
		}
		stack_buffer[p] = '\0';
		free(strs);
		
		/*
		for (i = 1; i < frames; ++i) {
			printf("%s\n", strs[i]);
			int _address_begin = netp::strpos(strs[i], (char*)"[");
			int _address_end = netp::strpos(strs[i], (char*)"]");
			::memcpy((void*)&stack_buffer[current_stack_fill_pos], (void* const)(strs[i] + _address_begin), _address_end - _address_begin);
			current_stack_fill_pos += (_address_end - _address_begin);
			stack_buffer[current_stack_fill_pos++] = ']';
			stack_buffer[current_stack_fill_pos++] = ' ';

			int _f_begin = netp::strpos(strs[i], (char*)"(");
			int _f_end = netp::strpos(strs[i], (char*)")");
			::memcpy((void*)&stack_buffer[current_stack_fill_pos], (void* const)(strs[i] + _f_begin), _f_end - _f_begin);
			current_stack_fill_pos += (_f_end - _f_begin);
			stack_buffer[current_stack_fill_pos++] = ')';
			stack_buffer[current_stack_fill_pos++] = ' ';

			stack_buffer[current_stack_fill_pos++] = '\n';

			if (i == 1) {
				char const* ba = "binary: ";
				::memcpy((void*)&binary_name[0], (void* const)ba, strlen(ba));
				::memcpy((void*)&binary_name[strlen(ba)], (void* const)(strs[i]), _f_begin);
				binary_name[strlen(ba) + _f_begin] = '\0';
			}
		}

		::memcpy((void*)&stack_buffer[current_stack_fill_pos], (void* const)(binary_name), strlen(binary_name));
		current_stack_fill_pos += strlen(binary_name);

		stack_buffer[current_stack_fill_pos] = '\0';
		assert(current_stack_fill_pos <= s);
		::free(strs);
		*/
	}

#elif defined(_NETP_STACK_WALKER_AVAILABLE_HPP)

	class stack_walker : 
		public StackWalker 
	{
		//WARN: do not embed any complex object, such as std::string, this could affact the callstack, I don't know why
	public:
		char* output_str;
		u32_t output_str_len;
		u32_t buf_size;
		stack_walker( char* output_str_, u32_t buf_size_ ) :
			StackWalker(RetrieveNone),
			output_str(output_str_),
			output_str_len(0),
			buf_size(buf_size_)
		{}

	protected:
		virtual void OnOutput(LPCSTR szText)
		{
			u32_t szlen = u32_t(netp::strlen(szText));
			u32_t to_copy = szlen > (buf_size - output_str_len-1) ? (buf_size - output_str_len - 1) : szlen;
			std::memcpy(output_str + output_str_len, szText, to_copy);
			output_str_len += to_copy;
			StackWalker::OnOutput(szText);
		}
	};

	/*
	//bug version
	class stack_walker : public StackWalker {
	public:
		std::string __stack_info;
		stack_walker() : StackWalker(RetrieveNone), __stack_info() {
		}
	protected:
		virtual void OnOutput(LPCSTR szText)
		{
			__stack_info += std::string(szText);
			//StackWalker::OnOutput(szText);
		}
	};*/

	void stack_trace(char* stack_buffer, netp::u32_t s) {
		stack_walker sw(stack_buffer, s);
		sw.ShowCallstack();
		*(stack_buffer + sw.output_str_len) = '\0';

		//bug version
//		StackWalker sw;
		//stack_walker sw;
		//sw.ShowCallstack();
		/*
		* StackWalker related issues:
		* if we uncommet ss = s-1, it's ok, 
		* if we comment ss = s-1, callstack is always empty
		//NETP_ASSERT(sw.stack_info.length() > 0);
		if (sw.__stack_info.length() > (s - 1)) {
			//u32_t ss = s - 1;
			std::memcpy(stack_buffer, sw.__stack_info.c_str(), (s - 1));
			//stack_buffer[s - 1] = '\0';
		} else {
			//std::strncpy(stack_buffer, sw.ON_OUTPUT_STR.c_str(), sw.ON_OUTPUT_STR.length());
			//stack_buffer[sw.ON_OUTPUT_STR.length()] = '\0';
		}
		*/
	}

#elif defined(_NETP_NO_STACK_TRACE)
	void stack_trace(char* stack_buffer, netp::u32_t s) {
		*stack_buffer = '\0';
	}
#endif
}

namespace netp {
	void __NETP_EXCEPTION_INIT__(netp::exception* e, int code_, char const* const sz_message_, char const* const sz_file_, int line_, char const* const sz_func_, char* stack_info_) {
		e->_code = code_;
		if (sz_message_ != 0) {
			netp::size_t len_1 = std::strlen(sz_message_);
			netp::size_t len_2 = sizeof(e->_message) / sizeof(e->_message[0]) - 1;
			netp::size_t copy_len = len_1 > len_2 ? len_2 : len_1;
			std::memcpy(e->_message, sz_message_, copy_len);
			e->_message[copy_len] = '\0';
		} else {
			e->_message[0] = '\0';
		}

		if (sz_file_ != 0) {
			netp::size_t len_1 = std::strlen(sz_file_);
			netp::size_t len_2 = sizeof(e->_file) / sizeof(e->_file[0]) - 1;
			netp::size_t copy_len = len_1 > len_2 ? len_2 : len_1;
			std::memcpy(e->_file, sz_file_, copy_len);
			e->_file[copy_len] = '\0';
		} else {
			e->_file[0] = '\0';
		}
		e->_line = line_;
		if (sz_func_ != 0) {
			netp::size_t len_1 = std::strlen(sz_func_);
			netp::size_t len_2 = sizeof(e->_func) / sizeof(e->_func[0]) - 1;
			netp::size_t copy_len = len_1 > len_2 ? len_2 : len_1;
			std::memcpy(e->_func, sz_func_, copy_len);
			e->_func[copy_len] = '\0';
		} else {
			e->_func[0] = '\0';
		}

		if (stack_info_ != 0 && stack_info_ != e->_callstack) {
			if (e->_callstack != 0) {
				std::free(e->_callstack);
			}
			netp::size_t info_len = std::strlen(stack_info_);
			e->_callstack = (char*) std::malloc(info_len + 1);
			std::memcpy(e->_callstack, stack_info_, info_len);
			*((e->_callstack) + info_len) = '\0';
		}
	}

#define NETP_EXCEPTION_STACK_INFO_SIZE_MAX (32*1024)
	exception::exception(int code_, char const* const sz_file_, int const& line_, char const* const sz_func_, ... ) :
		std::exception(),
		_code(code_),
		_line(line_),
		_callstack(0)
	{
		/*@TODO: optimize memory usage for message slot*/
		char __message[NETP_EXCEPTION_MESSAGE_LENGTH_LIMIT] = { 0 };
		va_list argp;
		va_start(argp, sz_func_);
		/*
			for the return value, refer to:
			The functions snprintf() and vsnprintf() do not write more than size bytes (including the terminating null byte ('\0')).
			If the output was truncated due to this limit then the return value is the number of characters (excluding the terminating null byte) which would have been written to the final string if enough space had been available. Thus, a return value of size or more means that the output was truncated. (See also below under NOTES.)
			If an output error is encountered, a negative value is returned.
		*/
		vsnprintf(__message, NETP_EXCEPTION_MESSAGE_LENGTH_LIMIT, va_arg(argp, char*), argp);
		va_end(argp);

		_callstack = (char*) std::malloc( sizeof(char)*NETP_EXCEPTION_STACK_INFO_SIZE_MAX );
		stack_trace(_callstack, NETP_EXCEPTION_STACK_INFO_SIZE_MAX);
		__NETP_EXCEPTION_INIT__(this, code_, __message, sz_file_, line_, sz_func_, _callstack);
	}

	exception::~exception() {
		if (_callstack != 0) {
			std::free(_callstack);
			_callstack = 0;
		}
	}

	exception::exception(exception const& other) :
		std::exception(),
		_code(other._code),
		_line(other._line),
		_callstack(0)
	{
		__NETP_EXCEPTION_INIT__(this, other._code, other._message, other._file, other._line, other._func, other._callstack);
	}

	exception& exception::operator= (exception const& other) {
		__NETP_EXCEPTION_INIT__(this, other._code, other._message, other._file, other._line, other._func, other._callstack);
		return *this;
	}
}
