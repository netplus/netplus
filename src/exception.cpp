#include <netp/exception.hpp>
#include <netp/string.hpp>

#ifdef _NETP_ANDROID
	#include <android/log.h>
#elif defined(_NETP_WIN)
	#include "./../3rd/stack_walker/StackWalker.h"
#else
#endif
namespace netp {

#ifdef _NETP_ANDROID
	void stack_trace(char stack_buffer[], u32_t const& s) {
		__android_log_print(ANDROID_LOG_FATAL, "WAWO", "exception ..." );
	}
#elif defined(_NETP_GNU_LINUX)
	void stack_trace(char stack_buffer[], u32_t const& s) {
		int j, nptrs;
		u32_t current_stack_fill_pos = 0;

#define BUFFER_SIZE 100

		char binary_name[256];

		void* buffer[BUFFER_SIZE];
		char** strings;
		nptrs = backtrace(buffer, BUFFER_SIZE);

		//printf("backtrace() return %d address\n", nptrs );

		strings = backtrace_symbols(buffer, nptrs);
		if (strings == nullptr) {
			perror("backtrace_symbol");
			exit(EXIT_FAILURE);
		}

		for (j = 1; j < nptrs; j++) {

			int _address_begin = netp::strpos(strings[j], (char*)"[");
			int _address_end = netp::strpos(strings[j], (char*)"]");
			::memcpy((void*)&stack_buffer[current_stack_fill_pos], (void* const)(strings[j] + _address_begin), _address_end - _address_begin);
			current_stack_fill_pos += (_address_end - _address_begin);
			stack_buffer[current_stack_fill_pos++] = ']';
			stack_buffer[current_stack_fill_pos++] = ' ';

			int _f_begin = netp::strpos(strings[j], (char*)"(");
			int _f_end = netp::strpos(strings[j], (char*)")");
			::memcpy((void*)&stack_buffer[current_stack_fill_pos], (void* const)(strings[j] + _f_begin), _f_end - _f_begin);
			current_stack_fill_pos += (_f_end - _f_begin);
			stack_buffer[current_stack_fill_pos++] = ')';
			stack_buffer[current_stack_fill_pos++] = ' ';

			stack_buffer[current_stack_fill_pos++] = '\n';

			if (j == 1) {
				char const* ba = "binary: ";
				::memcpy((void*)&binary_name[0], (void* const)ba, strlen(ba));
				::memcpy((void*)&binary_name[strlen(ba)], (void* const)(strings[j]), _f_begin);
				binary_name[strlen(ba) + _f_begin] = '\0';
			}
		}

		::memcpy((void*)&stack_buffer[current_stack_fill_pos], (void* const)(binary_name), strlen(binary_name));
		current_stack_fill_pos += strlen(binary_name);

		stack_buffer[current_stack_fill_pos] = '\0';
		assert(current_stack_fill_pos <= s);
		::free(strings);
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
		}
		else {
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
				::free(e->_callstack);
			}

			netp::size_t info_len = ::strlen(stack_info_);
			e->_callstack = (char*)::malloc(info_len + 1);
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
			::free(_callstack);
			_callstack = nullptr;
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