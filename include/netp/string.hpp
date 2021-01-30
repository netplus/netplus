#ifndef _NETP_STRING_HPP_
#define _NETP_STRING_HPP_

#define NETP_ENABLE_STDC_WANT_LIB_EXT1 0

#if defined(__STDC_LIB_EXT1__)
	#if (__STDC_LIB_EXT1__ >= 201112L)
		#undef NETP_ENABLE_STDC_WANT_LIB_EXT1
		#define NETP_ENABLE_STDC_WANT_LIB_EXT1 1
		#define __STDC_WANT_LIB_EXT1__ 1 /* Want the ext1 functions */
	#endif
#endif


#include <cstdio>
#include <cstdlib>
#include <clocale>
#include <vector>
#include <cstring>
#include <cwchar>
#include <algorithm>
#include <string>

#include <netp/core/platform.hpp>
#include <netp/core/config.hpp>
#include <netp/memory.hpp>
#include <netp/constants.hpp>

#define _NETP_IS_CHAR_T(_T) (sizeof(_T)==sizeof(char))
#define _NETP_IS_WCHAR_T(_T) (sizeof(_T)==sizeof(wchar_t))


#if !defined(_NETP_WIN) && !NETP_ENABLE_STDC_WANT_LIB_EXT1
// refer to
// https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/mbstowcs-s-mbstowcs-s-l?view=vs-2019
// https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/wcstombs-s-wcstombs-s-l?view=vs-2019

typedef int errno_t;
inline errno_t wcstombs_s(
	size_t* pReturnValue,
	char* mbstr,
	size_t sizeInBytes,
	const wchar_t* wcstr,
	size_t count
) {
	NETP_ASSERT(count <= sizeInBytes);
	//note: wcstombs always return real len of string (exclude '\0')
	int rt = (int)wcstombs(mbstr, wcstr, count);
	if (rt > 0) {
		if (mbstr != nullptr) {
			*(mbstr + rt) = '\0';
		}
		*pReturnValue = (rt + 1);
		return 0;
	}
	return rt;
}

inline errno_t mbstowcs_s(
	size_t* pReturnValue,
	wchar_t* wcstr,
	size_t sizeInWords,
	const char* mbstr,
	size_t count
)
{
	NETP_ASSERT(count <= sizeInWords);
	int rt = (int)mbstowcs(wcstr, mbstr, count);
	if (rt > 0) {
		*(wcstr + rt) = L'\0';
		*pReturnValue = (rt + 1);
		return 0;
	}
	return rt;
}
#endif

//extern part
namespace netp {
	typedef std::basic_string<char, std::char_traits<char>, netp::allocator<char> >	string_t;
	typedef std::basic_string<wchar_t, std::char_traits<wchar_t>, netp::allocator<wchar_t> >	wstring_t;
	template<typename T>
	inline string_t to_string(T const& t) {
		return string_t(std::to_string(t).c_str());
	}

#if defined(_NETP_WIN) && _MSC_VER <= 1700
	/* abc-333		-> -333
	 * abc333		-> +333
	 * abc 333		-> +333
	 * abc -333		-> -333
	 * abc -3 33	-> -3
	 * abc -3abc333	-> -3
	 */

	inline i64_t to_i64(char const* const str) {
		if( str == nullptr ) return 0;
		i64_t ret = 0;
		int i = 0;
		int negated_flag = 1;

		while( *(str+i) != '\0') {
			if( (*(str+i) <= '9' && *(str+i) >= '0') || *(str+i) == '-' || *(str+i) == '+' )
				break;
			++i;
		}

		if( *(str+i) == '-' ) {
			negated_flag = -1;
			++i;
		}

		if( *(str+i) == '+' ) {
			negated_flag = 1;
			++i;
		}

		while( *(str+i) != '\0' ) {
			if( !(*(str+i) >= '0' && *(str+i)<='9') )
			{
				break;
			}
			ret *= 10;
			ret += (*(str+i)) - '0';
			++i;
		}
		return ret*negated_flag;
	}

	inline u64_t to_u64(char const* const str) {
		if( str == nullptr ) return 0;
		u64_t ret = 0;
		int i = 0;
		int negated_flag = 1;

		while( *(str+i) != '\0') {
			if( (*(str+i) <= '9' && *(str+i) >= '0') || *(str+i) == '-' || *(str+i) == '+' )
				break;
			++i;
		}

		if( *(str+i) == '-' ) {
			negated_flag = -1;
			++i;
		}

		if( *(str+i) == '+' ) {
			negated_flag = 1;
			++i;
		}

		while( *(str+i) != '\0' ) {
			if( !(*(str+i) >= '0' && *(str+i)<='9') )
			{
				break;
			}
			ret *= 10;
			ret += (*(str+i)) - '0';
			++i;
		}
		return ret*negated_flag;
	}
#else
	inline i64_t to_i64(char const* const str) {
		NETP_ASSERT(str != nullptr);
		char* pEnd;
		return std::strtoll(str, &pEnd, 10);
	}

	inline u64_t to_u64(char const* const str) {
		NETP_ASSERT(str != nullptr);
		char* pEnd;
		return std::strtoull(str, &pEnd, 10);
	}
#endif

	inline i32_t to_i32(char const* const str) {
		char* pEnd;
		return std::strtol(str, &pEnd, 10);
	}
	inline u32_t to_u32(char const* const str) {
		char* pEnd;
		return std::strtoul(str, &pEnd, 10);
	}

	//typedef u32_t size_t;

	/* Please read these lines first if u have any question on these string apis
	 * why We have these APIs ?
	 *
	 * 1, std::strcpy is not safe
	 * 2, strcpy_s and strlcpy's parameter sequence is different
	 * 3, we don't have a wcsncpy_s on gnu
	 * 4, there would be no obvious performance difference if we imply it by memxxx APIs
	 * 5, if I'm wrong, please correct me, thx.
	 */

	inline netp::size_t strlen( char const* const str ) {
		return std::strlen(str);
	}

	inline netp::size_t strlen(wchar_t const* const wstr ) {
		return std::wcslen(wstr);
	}

	inline int strcmp(char const* const lcptr, char const* const rcptr ) {
		return std::strcmp(lcptr,rcptr);
	}
	inline int strncmp(char const* const lcptr, char const* const rcptr, netp::size_t const& length ) {
		return std::strncmp(lcptr,rcptr,length);
	}

	inline int strcmp(wchar_t const* const lwcptr, wchar_t const* const rwcptr ) {
		return std::wcscmp(lwcptr,rwcptr);
	}

	inline int strncmp(wchar_t const* const lwcptr, wchar_t const* const rwcptr, netp::size_t const& length) {
		return std::wcsncmp(lwcptr,rwcptr,length);
	}

	template<class string_t>
	inline bool iequals(const string_t& a, const string_t& b) {
		const size_t len = a.length();
		if (len != b.length()) { return false; }
		size_t i = 0;
		while (i < len) {
			if ( ::tolower(a[i]) != tolower(b[i])) {
				return false;
			}
			++i;
		}
		return true;
	}

	//src must be null-terminated
	//The behavior is undefined if the dest array is not large enough. The behavior is undefined if the strings overlap
	inline char* strcpy(char* const dst, char const* const src) {
		netp::size_t src_len = netp::strlen(src);
		::memcpy(dst,src,src_len);
		*(dst+src_len) = '\0';
		return dst;
	}
	inline wchar_t* strcpy(wchar_t* const dst, wchar_t const* const src) {
		netp::size_t src_len = netp::strlen(src);
		::memcpy(dst,src,src_len*sizeof(wchar_t) );
		*(dst+src_len) = L'\0';
		return dst;
	}

	inline char* strncpy(char* const dst, char const* const src, netp::size_t len) {
		const netp::size_t src_len = netp::strlen(src);
		const netp::size_t copy_length = (src_len> len)? len :src_len;
		::memcpy(dst,src,copy_length*sizeof(char));
		*(dst+ len) = '\0';
		return dst;
	}
	inline wchar_t* strncpy(wchar_t* const dst, wchar_t const* const src, netp::size_t len) {
		netp::size_t src_len = netp::strlen(src);
		netp::size_t copy_length = (src_len>len)?len:src_len;
		::memcpy(dst,src,copy_length*sizeof(wchar_t));
		*(dst+ len) = L'\0';
		return dst;
	}

	//src must be null-terminated, or the behaviour is undefined
	//dst must have enough space, or the behaviour is undefined if the strings overlap
	//dst will be null-terminated after strcat&strncat
	inline char* strcat(char* const dst, char const* const src) {
		netp::size_t dst_length = netp::strlen(dst);
		netp::size_t src_len = netp::strlen(src);

		netp::strncpy(dst+dst_length,src,src_len);
		return dst;
	}
	inline char* strncat(char* const dst, char const* const src, netp::size_t const& length) {
		netp::size_t dst_length = netp::strlen(dst);
		netp::size_t src_len = netp::strlen(src);
		netp::size_t copy_length = (src_len>length) ? length : src_len;

		netp::strncpy(dst+dst_length,src,copy_length);
		return dst;
	}
	inline wchar_t* strncat(wchar_t* const dst, wchar_t const* const src, netp::size_t const& length) {
		netp::size_t dst_length = netp::strlen(dst);
		netp::size_t src_len = netp::strlen(src);
		netp::size_t copy_length = (src_len>length) ? length : src_len;

		netp::strncpy(dst+dst_length,src,copy_length);
		return dst;
	}

	inline char* strstr(char* const dst, char const* const check) {
		return std::strstr( dst, check);
	}
	inline char const* strstr( char const* const dst, char const* const check ) {
		return std::strstr(dst,check);
	}

	inline wchar_t* strstr(wchar_t const* const dst, wchar_t const* const check) {
		return std::wcsstr( const_cast<wchar_t*>(dst), check);
	}
	inline wchar_t const* strstr(wchar_t* const dst, wchar_t const* const check) {
		return std::wcsstr( dst, check);
	}

	//return the first occurence of the string's appear postion
	//not the bytes position , but the char|wchar_t 's sequence order number
	inline int strpos( char const* const dst, char const* const check ) {
		char const* sub = netp::strstr(dst,check);

		if(sub==nullptr) {
			return -1;
		}

		return netp::long_t(sub-dst);
	}

	inline int strpos( wchar_t const* const dst, wchar_t const* const check ) {
		wchar_t const* sub = netp::strstr(dst,check);

		if(sub==nullptr) {
			return -1;
		}

		return netp::long_t(sub-dst);
	}

	inline void strtolower( char* const buffer, u32_t size, char const* const src ) {
		NETP_ASSERT(size>=netp::strlen(src) );
		const netp::size_t c = netp::strlen(src);
		netp::size_t i = 0;
		while ( (i<c) && (i<size) )  {
			*(buffer + i) = static_cast<char>(::tolower(*(src + i) ));
			++i;
		}
	}

	inline wchar_t* strdup(const wchar_t* nwptr_src ) {
		wchar_t* nwptr_dst = (wchar_t*) malloc(sizeof(wchar_t)*(netp::strlen(nwptr_src)+1));
		return netp::strcpy(nwptr_dst, nwptr_src);
	}

	inline char* strdup(const char* nptr_src) {
		char* nptr_dst = (char*)malloc(sizeof(char)*(netp::strlen(nptr_src) + 1));
		return netp::strcpy(nptr_dst, nptr_src);
	}

	template<typename T>
	inline void strdupfree(T* ptr) {
		free(ptr);
	}

	inline void strtoupper(char* const buffer, netp::size_t size, char const* const src) {
		NETP_ASSERT(size >= netp::strlen(src));
		const netp::size_t c = netp::strlen(src);
		netp::size_t i = 0;
		while ( (i<c) && (i<size)) {
			*(buffer + i) = static_cast<char>(::toupper(*(src + i)));
			++i;
		}
	}

	template <class _string_t>
	inline int wchartochar(const wchar_t* wcstr, size_t len, _string_t& cstring, const char* ctype = "") {

		char* old_local = netp::strdup(std::setlocale(LC_CTYPE, NULL));
		if (strcmp(ctype, "") == 0) {
			std::setlocale(LC_CTYPE, std::setlocale(LC_CTYPE, ""));
		} else {
			std::setlocale(LC_CTYPE, ctype);
		}

		wchar_t *nwcsptr = new wchar_t[len + 1];
		netp::strncpy(nwcsptr, wcstr, len); //null terminated
		size_t converted;
		int rt = wcstombs_s(&converted, NULL, 0, nwcsptr, 0);
		if (rt == 0) {
			char* ncptr = new char[converted];
			rt = wcstombs_s(&converted, ncptr, converted, nwcsptr, converted);
			if (rt == 0) {
				cstring = _string_t(ncptr, converted - 1);
			}
			delete[] ncptr;
		}
		delete[] nwcsptr;

		std::setlocale(LC_CTYPE,old_local);
		netp::strdupfree(old_local);
		return rt;
	}

	template <class _wstring_t>
	inline int chartowchar(const char* csptr, size_t len, _wstring_t& wstring, const char* ctype = "" ) {
		char* old_local = netp::strdup(std::setlocale(LC_CTYPE, NULL));
		if (strcmp(ctype, "") == 0) {
			std::setlocale(LC_CTYPE, std::setlocale(LC_CTYPE, ""));
		} else {
			std::setlocale(LC_CTYPE, ctype);
		}

		wchar_t *nwcsptr = new wchar_t[len + 1]; //maximum
		std::size_t converted;
		int rt = mbstowcs_s(&converted, nwcsptr, (len+1), csptr, len);
		if (rt == 0) {
			wstring = _wstring_t(nwcsptr, converted - 1);
		}
		delete[] nwcsptr;

		std::setlocale(LC_CTYPE, old_local);
		netp::strdupfree(old_local);
		return rt;
	}

	/*
		wchar_t* cnwstr = L"sz中文";
		std::string cnstr;
		netp::wchartochar(cnwstr, netp::strlen(cnwstr), cnstr);

		std::wstring cnwstr_;
		netp::chartowchar(cnstr.c_str(), cnstr.length(), cnwstr_);
		NETP_ASSERT(cnwstr_.compare(std::wstring(cnwstr)) ==0);
	*/

	//enum CharacterSet
	//{
	//	CS_UTF8,
	//	CS_GBK,
	//	CS_GB2312,
	//	CS_BIG5
	//};

	//wctomb,wcstombs,mbstowcs,setlocal
}

namespace netp {

	template <class _string_t>
	inline _string_t rtrim(_string_t const& source_) {
		std::size_t len = source_.length();
		if (len == 0) return _string_t();

		const char* begin = source_.c_str();
		std::size_t end = len - 1;
		while (end > 0 && (*(begin + end)) == ' ')
			--end;

		return _string_t(begin, end + 1);
	}

	template<class _string_t>
	inline _string_t ltrim(_string_t const& source_) {
		std::size_t size = source_.length();
		if (size == 0) return std::string();

		const char* begin = source_.c_str();
		std::size_t start_idx = 0;
		while (start_idx < size && (*(begin + start_idx)) == ' ')
			++start_idx;

		return _string_t(begin + start_idx, size - start_idx);
	}

	template<class _string_t>
	inline _string_t trim(_string_t const& source_) {
		return rtrim(ltrim(source_));
	}

	template<class _string_t>
	inline int replace(_string_t const& source_, _string_t const& search_, _string_t const& replace_, _string_t& result) {
		int replace_count = 0;
		_string_t source (source_.c_str(), source_.length());
	begin:
		std::size_t pos = source.find(search_);
		if (pos == _string_t::npos) {
			return replace_count;
		}
		_string_t new_str = source.replace(pos, search_.length(), replace_);
		++replace_count;
		source = new_str;

		result = new_str;
		goto begin;
	}

	template <class _string_t>
	inline void split(_string_t const& string, _string_t const& delimiter, std::vector<_string_t>& result ) {

		NETP_ASSERT( result.size() == 0 );
		char const* check_cstr = string.c_str();
		netp::size_t check_length = string.length();
		netp::size_t next_check_idx = 0;

		char const* delimiter_cstr = delimiter.c_str();
		netp::size_t delimiter_length = delimiter.length();

		int hit_pos ;

		while( (next_check_idx < check_length) && ( hit_pos = netp::strpos( check_cstr + next_check_idx, delimiter_cstr )) != -1 )
		{
			_string_t tmp( (check_cstr + next_check_idx), hit_pos );
			next_check_idx += (hit_pos + delimiter_length);

			if( tmp.length() > 0 ) {
				result.push_back( tmp );
			}
		}

		if( (check_length - next_check_idx) > 0 ) {
			_string_t left_string( (check_cstr + next_check_idx), check_length-next_check_idx );
			result.push_back( left_string );
		}
	}

	template <class _string_t>
	inline void join(std::vector<_string_t> const& strings, _string_t const& delimiter, _string_t& result) {
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

	//this code borrow from msvc stl
	inline size_t _Hash_seq(const unsigned char* _First, size_t _Count)
	{	// FNV-1a hash function for bytes in [_First, _First + _Count)
#ifdef _NETP_ARCH_X64
		static_assert(sizeof(size_t) == 8, "This code is for 64-bit size_t.");
		const size_t _FNV_offset_basis = 14695981039346656037ULL;
		const size_t _FNV_prime = 1099511628211ULL;

#else /* defined(_WIN64) */
		static_assert(sizeof(size_t) == 4, "This code is for 32-bit size_t.");
		const size_t _FNV_offset_basis = 2166136261U;
		const size_t _FNV_prime = 16777619U;
#endif /* defined(_WIN64) */

		size_t _Val = _FNV_offset_basis;
		for (size_t _Next = 0; _Next < _Count; ++_Next)
		{	// fold in another byte
			_Val ^= (size_t)_First[_Next];
			_Val *= _FNV_prime;
		}
		return (_Val);
	}
}

namespace std {
	template<>
	struct hash<netp::string_t>
	{
		typedef size_t result_type;
		inline result_type operator () (const netp::string_t& x) const {
			return netp::_Hash_seq((const unsigned char*)x.c_str(), x.size()*sizeof(char) );
		}
	};

	template<>
	struct hash<netp::wstring_t>
	{
		typedef size_t result_type;
		inline result_type operator () (const netp::wstring_t& x) const {
			return netp::_Hash_seq((const unsigned char*)x.c_str(), x.size() * sizeof(wchar_t));
		}
	};
}

#endif
