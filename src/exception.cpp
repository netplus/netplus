#include <netp/string.hpp>
#include <netp/exception.hpp>

#if defined(_NETP_WIN)
	#include "./../3rd/stack_walker/StackWalker.h"
#elif defined(_NETP_HAS_EXECINFO_H)
	#include <execinfo.h>
#elif defined(_NETP_ANDROID)
	#include <android/log.h>
#else
	#error 
#endif

namespace netp {

#if defined(_NETP_ANDROID)
	void stack_trace(char stack_buffer[], u32_t const& s) {
		__android_log_print(ANDROID_LOG_FATAL, "NETP", "exception ..." );
	}

#elif defined(_NETP_HAS_EXECINFO_H)
	#define BUFFER_SIZE 128
	void stack_trace(char stack_buffer[], u32_t const& s) {
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

#elif defined(_NETP_WIN)
	class stack_walker : public StackWalker {
	public:
		std::string stack_info;
		stack_walker() : StackWalker(RetrieveNone), stack_info() {
		}
	protected:
		virtual void OnOutput(LPCSTR szText)
		{
			stack_info += std::string(szText);
			StackWalker::OnOutput(szText);
		}
	};

	void stack_trace(char stack_buffer[], netp::u32_t const& s) {
		stack_walker sw;
		sw.ShowCallstack();
		NETP_ASSERT(sw.stack_info.length() > 0);
		if (sw.stack_info.length() > (s - 1)) {
			::memcpy(stack_buffer, sw.stack_info.c_str(), s - 1);
			stack_buffer[s - 1] = '\0';
		} else {
			::memcpy(stack_buffer, sw.stack_info.c_str(), sw.stack_info.length());
			stack_buffer[sw.stack_info.length()] = '\0';
		}
	}
#endif
}

namespace netp {
	void __NETP_EXCEPTION_INIT__(netp::exception* e, int code_, char const* const sz_message_, char const* const sz_file_, int line_, char const* const sz_func_, char const* const stack_info_) {
		e->_code = code_;
		if (sz_message_ != 0) {
			netp::size_t len_1 = ::strlen(sz_message_);
			netp::size_t len_2 = sizeof(e->_message) / sizeof(e->_message[0]) - 1;
			netp::size_t copy_len = len_1 > len_2 ? len_2 : len_1;
			::memcpy(e->_message, sz_message_, copy_len);
			e->_message[copy_len] = '\0';
		} else {
			e->_message[0] = '\0';
		}

		if (sz_file_ != 0) {
			netp::size_t len_1 = ::strlen(sz_file_);
			netp::size_t len_2 = sizeof(e->_file) / sizeof(e->_file[0]) - 1;
			netp::size_t copy_len = len_1 > len_2 ? len_2 : len_1;
			::memcpy(e->_file, sz_file_, copy_len);
			e->_file[copy_len] = '\0';
		} else {
			e->_file[0] = '\0';
		}
		e->_line = line_;
		if (sz_func_ != 0) {
			netp::size_t len_1 = ::strlen(sz_func_);
			netp::size_t len_2 = sizeof(e->_func) / sizeof(e->_func[0]) - 1;
			netp::size_t copy_len = len_1 > len_2 ? len_2 : len_1;
			::memcpy(e->_func, sz_func_, copy_len);
			e->_func[copy_len] = '\0';
		} else {
			e->_func[0] = '\0';
		}
		if (stack_info_ != 0) {
			if (e->_callstack != 0) {
				netp::allocator<char>::free(e->_callstack);
			}

			netp::size_t info_len = ::strlen(stack_info_);
			e->_callstack = netp::allocator<char>::malloc(info_len + 1);
			::memcpy(e->_callstack, stack_info_, info_len);
			*((e->_callstack) + info_len) = '\0';
		} else {
			e->_callstack = 0;
		}
	}

	exception::exception(int code_, char const* const sz_message_, char const* const sz_file_, int const& line_, char const* const sz_func_) :
		_code(code_),
		_line(line_),
		_callstack(0)
	{
		const u32_t info_size = 1024*64;
		char info[info_size] = {0};
		stack_trace(info, info_size);
		__NETP_EXCEPTION_INIT__(this, code_, sz_message_, sz_file_, line_, sz_func_, info);

		//__android_log_print(ANDROID_LOG_INFO, "aaa", "zzzz");
	}

	exception::~exception() {
		if (_callstack != 0) {
			netp::allocator<char>::free(_callstack);
			_callstack = 0;
		}
	}

	exception::exception(exception const& other) :
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
