#ifndef _NETP_BYTES_HELPER_HPP_
#define _NETP_BYTES_HELPER_HPP_

#include <netp/core.hpp>

namespace netp {

	namespace bytes_helper {
		//big endian 0x01020304 => 01 02 03 04 (most significant first)
		template <class T> struct type{};

		template<class T>
		struct basic_type_width {
			typedef void width_type_t;
		};

		template<>
		struct basic_type_width<u8_t> {
			typedef type<u8_t> width_type_t;
		};
		template<>
		struct basic_type_width<i8_t> {
			typedef type<i8_t> width_type_t;
		};
		template<>
		struct basic_type_width<u16_t> {
			typedef type<u16_t> width_type_t;
		};
		template<>
		struct basic_type_width<i16_t> {
			typedef type<u16_t> width_type_t;
		};
		template<>
		struct basic_type_width<u32_t> {
			typedef type<u32_t> width_type_t;
		};
		template<>
		struct basic_type_width<i32_t> {
			typedef type<u32_t> width_type_t;
		};
		template<>
		struct basic_type_width<u64_t> {
			typedef type<u64_t> width_type_t;
		};
		template<>
		struct basic_type_width<i64_t> {
			typedef type<u64_t> width_type_t;
		};
		
		struct big_endian {

			template<class T, class ReadIt>
			__NETP_FORCE_INLINE static T __read_impl(ReadIt const& start, type<u8_t>) {
				return *((byte_t*)start);
			}
			template<class T, class ReadIt>
			__NETP_FORCE_INLINE static T __read_impl(ReadIt const& start, type<u16_t>) {
				T v = 0;
				*(((byte_t*)&v) + 1) = *((byte_t*)start);
				*((byte_t*)&v) = *(((byte_t*)start) + 1);
				return v;
			}
			template<class T, class ReadIt>
			__NETP_FORCE_INLINE static T __read_impl(ReadIt const& start, type<u32_t>) {
				T v = 0;
				*(((byte_t*)&v) + 3) = *((byte_t*)start);
				*(((byte_t*)&v) + 2) = *(((byte_t*)start) + 1);
				*(((byte_t*)&v) + 1) = *(((byte_t*)start) + 2);
				*((byte_t*)&v) = *(((byte_t*)start) + 3);
				return v;
			}
			template<class T, class ReadIt>
			__NETP_FORCE_INLINE static T __read_impl(ReadIt const& start, type<u64_t>) {
				T v = 0;
				*(((byte_t*)&v) + 7) = *((byte_t*)start);
				*(((byte_t*)&v) + 6) = *(((byte_t*)start) + 1);
				*(((byte_t*)&v) + 5) = *(((byte_t*)start) + 2);
				*(((byte_t*)&v) + 4) = *(((byte_t*)start) + 3);
				*(((byte_t*)&v) + 3) = *(((byte_t*)start) + 4);
				*(((byte_t*)&v) + 2) = *(((byte_t*)start) + 5);
				*(((byte_t*)&v) + 1) = *(((byte_t*)start) + 6);
				*((byte_t*)&v) = *(((byte_t*)start) + 7);
				return v;
			}

			template <class T, class ReadIt>
			__NETP_FORCE_INLINE static T read_impl(ReadIt const& start, type<T>) {
#if __NETP_IS_BIG_ENDIAN
				return *((T*)(start));
#else
				return __read_impl<T, ReadIt>(start, typename basic_type_width<T>::width_type_t() );
#endif
			}

			template<class T, class OutIt>
			__NETP_FORCE_INLINE static u32_t __write_impl(T v, OutIt const& start, type<u8_t>) {
				*((byte_t*)start) = v;
				return 1;
			}

			template<class T, class OutIt>
			__NETP_FORCE_INLINE static u32_t __write_impl(T v, OutIt const& start, type<u16_t>) {
				*((byte_t*)start)		= ((v >> 8) & 0xff);
				*((byte_t*)start + 1) = (v & 0xff);
				return 2;
			}
			template<class T, class OutIt>
			__NETP_FORCE_INLINE static u32_t __write_impl(T v, OutIt const& start, type<u32_t>) {
				*((byte_t*)start)		= ((v >> 24) & 0xff);
				*((byte_t*)start + 1) = ((v >> 16) & 0xff);
				*((byte_t*)start + 2) = ((v >> 8) & 0xff);
				*((byte_t*)start + 3) = (v & 0xff);
				return 4;
			}
			template<class T, class OutIt>
			__NETP_FORCE_INLINE static u32_t __write_impl(T v, OutIt const& start, type<u64_t>) {
				*((byte_t*)start)		= ((v >> 56) & 0xff);
				*((byte_t*)start + 1) = ((v >> 48) & 0xff);
				*((byte_t*)start + 2) = ((v >> 40) & 0xff);
				*((byte_t*)start + 3) = ((v >> 32) & 0xff);
				*((byte_t*)start + 4) = ((v >> 24) & 0xff);
				*((byte_t*)start + 5) = ((v >> 16) & 0xff);
				*((byte_t*)start + 6) = ((v >> 8) & 0xff);
				*((byte_t*)start + 7) = (v & 0xff);
				return 8;
			}

			template <class T, class OutIt>
			__NETP_FORCE_INLINE static netp::u32_t write_impl(T v, OutIt const& start) {
#if __NETP_IS_BIG_ENDIAN
				*((T*)start) = v;
				return sizeof(T);
#else
				//netp::u32_t write_idx = 0;
				//for (::size_t i = (sizeof(T) - 1); i != ::size_t(~0); --i) {
				//	*(start_addr + write_idx++) = ((val >> (i * 8)) & 0xff);
				//}
				return __write_impl<T, OutIt>(v,start, typename basic_type_width<T>::width_type_t());
#endif
			}

		};

		struct little_endian {
			template <class T, class ReadIt>
			static inline T read_impl(ReadIt const& start, type<T>) {
				T ret = 0;
				for (::size_t i = (sizeof(T) - 1); i != ::size_t(~0); --i) {
					ret <<= 8;
					ret |= static_cast<u8_t>(*(start + i));
				}
				return ret;
			}

			template <class T, class OutIt>
			static inline netp::u32_t write_impl(T val, OutIt const& start_addr) {
				netp::u32_t write_idx = 0;
				for (::size_t i = 0 ; i <sizeof(T); ++i) {
					*(start_addr + write_idx++) = ((val >> (i * 8)) & 0xff);
				}
				return write_idx;
			}
		};

		template <class ReadIt>
		__NETP_FORCE_INLINE u8_t read_u8( ReadIt const& start ) {
			return static_cast<u8_t>( *start );
		}
		template <class ReadIt>
		__NETP_FORCE_INLINE i8_t read_i8( ReadIt const & start ) {
			return static_cast<i8_t>( *start );
		}
		template <class ReadIt, class endian=big_endian>
		__NETP_FORCE_INLINE u16_t read_u16( ReadIt const& start ) {
			return endian::read_impl( start, type<u16_t>() );
		}
		template <class ReadIt, class endian = big_endian>
		__NETP_FORCE_INLINE i16_t read_i16( ReadIt const& start ) {
			return endian::read_impl( start, type<i16_t>() );
		}

		template <class ReadIt, class endian = big_endian>
		__NETP_FORCE_INLINE u32_t read_u32( ReadIt const& start) {
			return endian::read_impl( start, type<u32_t>() );
		}
		template <class ReadIt, class endian = big_endian>
		__NETP_FORCE_INLINE i32_t read_i32( ReadIt const& start ) {
			return endian::read_impl( start, type<i32_t>() ) ;
		}

		template <class ReadIt, class endian = big_endian>
		__NETP_FORCE_INLINE u64_t read_u64( ReadIt const& start) {
			return endian::read_impl( start, type<u64_t>() );
		}
		template <class ReadIt, class endian = big_endian>
		__NETP_FORCE_INLINE i64_t read_i64( ReadIt const& start ) {
			return endian::read_impl( start, type<i64_t>() );
		}

		template <class ReadIt>
		__NETP_FORCE_INLINE netp::u32_t read_bytes( byte_t* const target, netp::u32_t len,ReadIt const& start ) {
			netp::u32_t read_idx = 0;
			for( ; read_idx < len; ++read_idx ) {
				*(target+read_idx) = *(start+read_idx) ;
			}
			return read_idx ;
		}

		template <class WriteIt>
		__NETP_FORCE_INLINE netp::u32_t write_u8( u8_t val , WriteIt const& it ) {
			return big_endian::template write_impl<u8_t>(val, it);
		}

		template <class WriteIt>
		__NETP_FORCE_INLINE netp::u32_t write_i8( i8_t val, WriteIt const& it) {
			return big_endian::template write_impl<i8_t>(val, it);
		}

		template <class WriteIt, class endian = big_endian>
		__NETP_FORCE_INLINE netp::u32_t write_u16(u16_t val , WriteIt const& it) {
			return endian::template write_impl<u16_t>(val, it);
		}
		template <class WriteIt, class endian = big_endian>
		__NETP_FORCE_INLINE netp::u32_t write_i16(i16_t val , WriteIt const& it) {
			return endian::template write_impl<i16_t>(val, it);
		}

		template <class WriteIt, class endian = big_endian>
		__NETP_FORCE_INLINE netp::u32_t write_u32(u32_t val, WriteIt const& it) {
			return endian::template write_impl<u32_t>(val, it);
		}
		template <class WriteIt, class endian = big_endian>
		__NETP_FORCE_INLINE netp::u32_t write_i32(i32_t val, WriteIt const& it) {
			return endian::template write_impl<i32_t>(val, it);
		}

		template <class WriteIt, class endian = big_endian>
		__NETP_FORCE_INLINE netp::u32_t write_u64(u64_t val, WriteIt const& it) {
			return endian::template write_impl<u64_t>(val, it);
		}
		template <class WriteIt, class endian = big_endian>
		__NETP_FORCE_INLINE netp::u32_t write_i64(i64_t val, WriteIt const& it) {
			return endian::template write_impl<i64_t>(val, it);
		}

		template <class WriteIt>
		__NETP_FORCE_INLINE netp::u32_t write_bytes( const byte_t* const buffer, netp::u32_t len, WriteIt const& it) {
			netp::u32_t write_idx = 0;
			for( ; write_idx < len;++write_idx ) {
				*(it +write_idx) = *(buffer+write_idx) ;
			}
			return write_idx;
		}
	}
}
#endif