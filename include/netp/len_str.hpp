#ifndef _NETP_LEN_STR_HPP
#define _NETP_LEN_STR_HPP

#include <netp/core.hpp>
#include <netp/string.hpp>

namespace netp {
	template <typename _char_t>
	struct len_cstr_impl {
		//supported operation
		// 1, new & new with a init length
		// 2, init assign
		// 3, copy assign
		// 4, operator + for concat
		// 5, length of string
		// 6, cstr address of string
		// 7, for any other requirement , please use std::string|std::wstring,,,
		typedef _char_t char_t;
		typedef _char_t* char_t_ptr;
		typedef len_cstr_impl<char_t> _MyT;

		char_t* cstr;
		netp::size_t len;

		len_cstr_impl() :
			cstr((char_t*)("")),
			len(0)
		{
		}

		len_cstr_impl(char_t const* const str) :
			cstr((char_t*)("")),
			len(0)
		{
			NETP_ASSERT(str != nullptr);
			len = netp::strlen(str);// str + '\0'

			if (len > 0) {
				NETP_ASSERT(cstr != str);
				cstr = netp::allocator<char_t>::malloc( (len + 1));
				NETP_ALLOC_CHECK(cstr, sizeof(char_t) * (len + 1));
				netp::strcpy(cstr, str);
			}
		}

		explicit len_cstr_impl(char_t const* const str, netp::size_t const& slen) :
			cstr((char_t*)("")),
			len(0)
		{
			NETP_ASSERT(netp::strlen(str) >= slen);
			if (slen > 0) {
				len = slen;
				cstr = netp::allocator<char_t>::malloc((slen + 1)); // str + '\0'
				NETP_ALLOC_CHECK(cstr, sizeof(char) * (slen + 1));
				netp::strncpy(cstr, str, slen);
			}
		}

		~len_cstr_impl() {
			if (len > 0) {
				netp::allocator<char_t>::free(cstr);
			}
			cstr = 0;
			len = 0;
		}

		len_cstr_impl substr(netp::size_t const& begin, netp::size_t const& _len) const {
			NETP_ASSERT((begin + _len) <= len);
			return len_cstr_impl(cstr + begin, _len);
		}

		void swap(len_cstr_impl& other) {
			std::swap(cstr, other.cstr);
			std::swap(len, other.len);
		}

		len_cstr_impl(len_cstr_impl const& other) :
			cstr((char_t*)("")),
			len(0)
		{
			if (other.len > 0) {
				cstr = netp::allocator<char_t>::malloc( (other.len + 1));
				NETP_ALLOC_CHECK(cstr, sizeof(char_t) * (other.len + 1));

				len = other.len;
				//contain '\0'
				::memcpy(cstr, other.cstr, (len + 1) * sizeof(char_t));
			}
		}

		len_cstr_impl& operator = (len_cstr_impl const& other) {
			len_cstr_impl(other).swap(*this);
			return *this;
		}

		//str must be null terminated, or behaviour undefined
		len_cstr_impl& operator = (char const* const str) {
			len_cstr_impl(str).swap(*this);
			return *this;
		}

		inline bool operator == (len_cstr_impl const& other) const {
			return (len == other.len) && (netp::strncmp(cstr, other.cstr, len) == 0);
		}
		inline bool operator != (len_cstr_impl const& other) const {
			return (len != other.len) || (netp::strncmp(cstr, other.cstr, len) != 0);
		}

		inline bool operator < (len_cstr_impl const& other) const {
			return netp::strcmp(cstr, other.cstr) < 0;
		}

		inline bool operator > (len_cstr_impl const& other) const {
			return netp::strcmp(cstr, other.cstr) > 0;
		}

		_MyT operator + (len_cstr_impl const& other) const {
			netp::size_t buffer_length = len + other.len + 1;
			char_t* _buffer = (char_t*) netp::allocator<char_t>::calloc(buffer_length);

			if (len) {
				netp::strncpy(_buffer, cstr, len);
			}

			if (other.len) {
				netp::strncat(_buffer, other.cstr, other.len);
			}

			_MyT lcstr(_buffer, len + other.len);
			netp::allocator<char_t>::free(_buffer);

			return lcstr;
		}

		_MyT operator + (char const* const str) const {
			return *this + len_cstr_impl(str);
		}

		_MyT& operator += (len_cstr_impl const& other) {
			_MyT tmp = (*this) + other;
			tmp.swap(*this);
			return *this;
		}

		_MyT& operator += (char const* const str) {
			_MyT tmp = (*this) + str;
			tmp.swap(*this);
			return *this;
		}
	};

	typedef len_cstr_impl<char>		len_cstr;
	typedef len_cstr_impl<wchar_t>	len_wcstr;

	inline netp::len_cstr	to_string(i64_t const& int64) {
		char tmp[32] = { 0 };
		int rt = snprintf(tmp, sizeof(tmp) / sizeof(tmp[0]), "%lld", int64);
		(void)rt;
		NETP_ASSERT(rt > 0 && rt < 32);
		return netp::len_cstr(tmp, std::strlen(tmp));
	}
	inline netp::len_cstr	to_string(u64_t const& uint64) {
		char tmp[32] = { 0 };
		int rt = snprintf(tmp, sizeof(tmp) / sizeof(tmp[0]), "%llu", uint64);
		(void)rt;
		NETP_ASSERT(rt > 0 && rt < 32);
		return len_cstr(tmp, std::strlen(tmp));
	}
	inline len_cstr	to_string(i32_t const& int32) {
		char tmp[32] = { 0 };
		int rt = snprintf(tmp, sizeof(tmp) / sizeof(tmp[0]), "%d", int32);
		(void)rt;
		NETP_ASSERT(rt > 0 && rt < 32);
		return len_cstr(tmp, std::strlen(tmp));
	}
	inline len_cstr	to_string(u32_t const& uint32) {
		char tmp[32] = { 0 };
		int rt = snprintf(tmp, sizeof(tmp) / sizeof(tmp[0]), "%u", uint32);
		(void)rt;
		NETP_ASSERT(rt > 0 && rt < 32);
		return len_cstr(tmp, std::strlen(tmp));
	}

	inline netp::len_cstr rtrim(netp::len_cstr const& source_) {
		if (source_.len == 0) return netp::len_cstr();

		netp::size_t end = source_.len - 1;
		while (end > 0 && (*(source_.cstr + end)) == ' ')
			--end;

		return netp::len_cstr(source_.cstr, end + 1);
	}

	inline netp::len_cstr ltrim(netp::len_cstr const& source_) {
		if (source_.len == 0) return netp::len_cstr();

		netp::size_t start = 0;
		while (start < source_.len && (*(source_.cstr + start)) == ' ')
			++start;

		return netp::len_cstr(source_.cstr + start, source_.len - start);
	}

	inline netp::len_cstr trim(netp::len_cstr const& source_) {
		return rtrim(ltrim(source_));
	}


	//return n if replace success, otherwise return -1
	inline int replace(netp::len_cstr const& source_, netp::len_cstr const& search_, netp::len_cstr const& replace_, netp::len_cstr& result) {
		int replace_count = 0;
		netp::len_cstr source = source_;
	begin:
		int pos = netp::strpos(source.cstr, search_.cstr);
		if (pos == -1) {
			return replace_count;
		}
		netp::len_cstr new_str = source.substr(0, pos) + replace_ + source.substr(pos + search_.len, source.len - (pos + search_.len));
		++replace_count;
		source = new_str;

		result = new_str;
		goto begin;
	}

	inline void split(netp::len_cstr const& lcstr, netp::len_cstr const& delimiter_string, std::vector<netp::len_cstr>& result) {
		std::vector<std::string> length_result;
		split(std::string(lcstr.cstr, lcstr.len), std::string(delimiter_string.cstr, delimiter_string.len), length_result);

		std::for_each(length_result.begin(), length_result.end(), [&result](std::string const& lengthcstr) {
			result.push_back(netp::len_cstr(lengthcstr.c_str(), lengthcstr.length()));
			});
	}

	inline void join(std::vector<netp::len_cstr> const& strings, netp::len_cstr const& delimiter, netp::len_cstr& result) {
		if (strings.size() == 0)
		{
			return;
		}

		u32_t curr_idx = 0;
		result += strings[curr_idx];
		curr_idx++;

		while (curr_idx < strings.size()) {
			result += delimiter;
			result += strings[curr_idx++];
		}
	}

}

namespace std {
	template <>
	struct hash<netp::len_cstr>
	{
		size_t operator() (netp::len_cstr const& lenstr) const {
			static std::hash<std::string> std_string_hash_func;
			return std_string_hash_func(std::string(lenstr.cstr, lenstr.len));
		}
	};
}
#endif