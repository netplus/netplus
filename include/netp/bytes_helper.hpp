#ifndef _NETP_BYTES_HELPER_HPP_
#define _NETP_BYTES_HELPER_HPP_

#include <netp/core.hpp>

namespace netp {

	namespace bytes_helper {
		//big endian 0x01020304 => 01 02 03 04 (most significant first)
		template <class T> struct type{};

		struct big_endian {
			template <class T, class ReadIt>
			static inline T read_impl(ReadIt const& start, type<T>) {
				T ret = 0;
				for (::size_t i = 0; i < sizeof(T); ++i) {
					ret <<= 8;
					ret |= static_cast<u8_t>(*(start + i));
				}
				return ret;
			}
			
			template <class T, class OutIt>
			static inline netp::u32_t write_impl(T val, OutIt const& start_addr ) {
				netp::u32_t write_idx = 0;
				for (::size_t i = (sizeof(T) - 1); i != ~0; --i) {
					*(start_addr + write_idx++) = ((val >> (i * 8)) & 0xff);
				}
				return write_idx;
			}
		};

		struct little_endian {
			template <class T, class ReadIt>
			static inline T read_impl(ReadIt const& start, type<T>) {
				T ret = 0;
				for (::size_t i = (sizeof(T) - 1); i != ~0; --i) {
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