#ifndef _NETP_HTTP_CHUNK_ENCODER_HPP
#define _NETP_HTTP_CHUNK_ENCODER_HPP

#include <stack>
#include <netp/core.hpp>

namespace netp { namespace http {

	inline int int_to_hex_string(int n, char* const hex_string, netp::u32_t len) {
		char* start = hex_string;
		static char _HEX_CHAR_[] = {
			'0','1','2','3',
			'4','5','6','7',
			'8','9','A','B',
			'C','D','E','F'
		};

		std::stack<char> char_stack;
		while (n != 0) {
			netp::u32_t mode_v = n % 16;
			char_stack.push(_HEX_CHAR_[mode_v]);
			n /= 16;
		}
		netp::u32_t i = 0;
		while (char_stack.size()) {
			NETP_ASSERT(i < len);
			*(start + i++) = char_stack.top();
			char_stack.pop();
		}
		return i;
	}

	inline int hex_string_to_int(char const* hex, netp::u32_t len) {
		netp::u32_t _t = 0;
		netp::u32_t _i = 0;
		netp::u32_t _b = 1;

		while (_i < len) {
			char tt = *(hex + ((len - 1) - _i));

			if (tt >= '0' && tt <= '9') {
				tt -= '0';
			} else if (tt >= 'A' && tt <= 'F') {
				tt = (tt - 'A') + 10;
			} else {}

			_t += (tt * _b);
			_b *= 16;
			_i++;
		}
		return _t;
	}
} }

#endif