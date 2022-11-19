#ifndef _NETP_BYTES_HELPER_HPP_
#define _NETP_BYTES_HELPER_HPP_

#include <netp/core.hpp>

namespace netp {

	namespace bytes_helper {
		//big endian: 0x01020304 -> 01 02 03 04 (most significant first)
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

		namespace bytes_internal {
			//@NOTE
			//ITS CORRECTNESS RELY ON A PRE-JUDGE OF THE CURRENT ARCH'S ENDIANNESS
			//DO NOT COPY THESE LINES DIRECTLY

			template<class T, class ReadIt>
			__NETP_FORCE_INLINE static T __read_impl(ReadIt const& start, type<u8_t> const&) {
				return *((byte_t*)start);
			}
			template<class T, class ReadIt>
			__NETP_FORCE_INLINE static T __read_impl(ReadIt const& start, type<u16_t> const&) {
				T v = 0;
				*(((byte_t*)&v) + 1) = *((byte_t*)start);
				*((byte_t*)&v) = *(((byte_t*)start) + 1);
				return v;
			}
			template<class T, class ReadIt>
			__NETP_FORCE_INLINE static T __read_impl(ReadIt const& start, type<u32_t> const&) {
				T v = 0;
				*(((byte_t*)&v) + 3) = *((byte_t*)start);
				*(((byte_t*)&v) + 2) = *(((byte_t*)start) + 1);
				*(((byte_t*)&v) + 1) = *(((byte_t*)start) + 2);
				*((byte_t*)&v) = *(((byte_t*)start) + 3);
				return v;
			}
			template<class T, class ReadIt>
			__NETP_FORCE_INLINE static T __read_impl(ReadIt const& start, type<u64_t> const&) {
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

			template<class T, class WriteIt>
			_NETP_CONSTEXPR static __NETP_FORCE_INLINE u32_t __write_impl(WriteIt const& start, T v, type<u8_t> const&) {
				*((byte_t*)start) = v;
				return 1;
			}

			template<class T, class WriteIt>
			_NETP_CONSTEXPR static __NETP_FORCE_INLINE u32_t __write_impl(WriteIt const& start, T v, type<u16_t> const&) {
				*((byte_t*)start) = ((v >> 8) & 0xff);
				*((byte_t*)start + 1) = (v & 0xff);
				return 2;
			}
			template<class T, class WriteIt>
			_NETP_CONSTEXPR static __NETP_FORCE_INLINE u32_t __write_impl(WriteIt const& start, T v, type<u32_t> const&) {
				*((byte_t*)start) = ((v >> 24) & 0xff);
				*((byte_t*)start + 1) = ((v >> 16) & 0xff);
				*((byte_t*)start + 2) = ((v >> 8) & 0xff);
				*((byte_t*)start + 3) = (v & 0xff);
				return 4;
			}
			template<class T, class WriteIt>
			_NETP_CONSTEXPR static __NETP_FORCE_INLINE u32_t __write_impl(WriteIt const& start, T v, type<u64_t> const&) {
				*((byte_t*)start) = ((v >> 56) & 0xff);
				*((byte_t*)start + 1) = ((v >> 48) & 0xff);
				*((byte_t*)start + 2) = ((v >> 40) & 0xff);
				*((byte_t*)start + 3) = ((v >> 32) & 0xff);
				*((byte_t*)start + 4) = ((v >> 24) & 0xff);
				*((byte_t*)start + 5) = ((v >> 16) & 0xff);
				*((byte_t*)start + 6) = ((v >> 8) & 0xff);
				*((byte_t*)start + 7) = (v & 0xff);
				return 8;
			}

			//template <class WriteIt>
			//struct bytes_writer {
			//	__NETP_FORCE_INLINE netp::u32_t write_bytes(WriteIt const buffer, netp::u32_t len, WriteIt const& it) {
			//		netp::u32_t write_idx = 0;
			//		for (; write_idx < len; ++write_idx) {
			//			*(it + write_idx) = *(buffer + write_idx);
			//		}
			//		return write_idx;
			//	}
			//};
			//template<>
			//struct bytes_writer<netp::byte_t*> {
			//};
		}
		
		struct big_endian {
			template <class T, class ReadIt>
			__NETP_FORCE_INLINE static T read_impl(ReadIt const& start, type<T>) {
#if __NETP_IS_BIG_ENDIAN
				return *((T*)(start));
#else
				return bytes_internal::__read_impl<T, ReadIt>(start, typename basic_type_width<T>::width_type_t() );
#endif
			}

			template <class T, class WriteIt >
			_NETP_CONSTEXPR static __NETP_FORCE_INLINE netp::u32_t write_impl(WriteIt const& start, T val ) {
#if __NETP_IS_BIG_ENDIAN
				*((T*)start) = val;
				return sizeof(T);
#else
				//netp::u32_t write_idx = 0;
				//for (::size_t i = (sizeof(T) - 1); i != ::size_t(~0); --i) {
				//	*(start_addr + write_idx++) = ((val >> (i * 8)) & 0xff);
				//}
				return bytes_internal::__write_impl<T,WriteIt>(start,val,typename basic_type_width<T>::width_type_t());
#endif
			}

		};

		struct little_endian {

			template <class T, class ReadIt>
			static inline T read_impl(ReadIt const& start, type<T> const&) {
#if __NETP_IS_LITTLE_ENDIAN
				return *((T*)(start));
#else
				return bytes_internal::__read_impl<T, ReadIt>(start, typename basic_type_width<T>::width_type_t());
#endif
				//T ret = 0;
				//for (::size_t i = (sizeof(T) - 1); i != ::size_t(~0); --i) {
				//	ret <<= 8;
				//	ret |= static_cast<u8_t>(*(start + i));
				//}
				//return ret;
			}

			template <class T, class WriteIt >
			static inline netp::u32_t write_impl(WriteIt const& start, T val ) {
#if __NETP_IS_LITTLE_ENDIAN
				*((T*)(start)) = val;
				return sizeof(T); 
#else
				return bytes_internal::__write_impl<WriteIt,T>(start, val, typename basic_type_width<T>::width_type_t());
#endif
				//netp::u32_t write_idx = 0;
				//for (::size_t i = 0 ; i <sizeof(T); ++i) {
				//	*(start_addr + write_idx++) = ((val >> (i * 8)) & 0xff);
				//}
				//return write_idx;
			}
		};

		#define NETP_DEF_ENDIAN netp::bytes_helper::little_endian

		//@note: return type could not used as a c++ override factor, so we have to forge a type<T> function param
		//ex: netp::bytes_helper::read<u32_t>(ReadIt); //ReadIt could be char*, netp::byte_t*
		template <class T, class ReadIt, class endian=NETP_DEF_ENDIAN>
		__NETP_FORCE_INLINE T read(ReadIt const& start, type<T> tt = type<T>()) {
			return endian::template read_impl<T,ReadIt>(start, tt);
		}
		template <class T, class WriteIt, class endian=NETP_DEF_ENDIAN>
		__NETP_FORCE_INLINE _NETP_CONSTEXPR netp::u32_t write(WriteIt const& it, T val) {
			return endian::template write_impl<T,WriteIt>(it,val);
		}

		//template <class ReadIt, class endian = NETP_DEF_ENDIAN>
		//__NETP_FORCE_INLINE u8_t read_u8( ReadIt const& start ) {
		//	return static_cast<u8_t>( *start );
		//}
		//template <class ReadIt, class endian = NETP_DEF_ENDIAN>
		//__NETP_FORCE_INLINE i8_t read_i8( ReadIt const & start ) {
		//	return static_cast<i8_t>( *start );
		//}
		//template <class ReadIt, class endian= NETP_DEF_ENDIAN>
		//__NETP_FORCE_INLINE u16_t read_u16( ReadIt const& start ) {
		//	return endian::read_impl( start, type<u16_t>() );
		//}
		//template <class ReadIt, class endian = NETP_DEF_ENDIAN>
		//__NETP_FORCE_INLINE i16_t read_i16( ReadIt const& start ) {
		//	return endian::read_impl( start, type<i16_t>() );
		//}
		//template <class ReadIt, class endian = NETP_DEF_ENDIAN>
		//__NETP_FORCE_INLINE u32_t read_u32( ReadIt const& start) {
		//	return endian::read_impl( start, type<u32_t>() );
		//}
		//template <class ReadIt, class endian = NETP_DEF_ENDIAN>
		//__NETP_FORCE_INLINE i32_t read_i32( ReadIt const& start ) {
		//	return endian::read_impl( start, type<i32_t>() ) ;
		//}
		//template <class ReadIt, class endian = NETP_DEF_ENDIAN>
		//__NETP_FORCE_INLINE u64_t read_u64( ReadIt const& start) {
		//	return endian::read_impl( start, type<u64_t>() );
		//}
		//template <class ReadIt, class endian = NETP_DEF_ENDIAN>
		//__NETP_FORCE_INLINE i64_t read_i64( ReadIt const& start ) {
		//	return endian::read_impl( start, type<i64_t>() );
		//}
		//template <class ReadIt>
		//__NETP_FORCE_INLINE netp::u32_t read_bytes( byte_t* const target, netp::u32_t len,ReadIt const& start ) {
		//	netp::u32_t read_idx = 0;
		//	for( ; read_idx < len; ++read_idx ) {
		//		*(target+read_idx) = *(start+read_idx) ;
		//	}
		//	return read_idx ;
		//}
		//@deprecated, use netp::bytes_helper::write<WriteIt, T, big_endian|little_endian>(Writeit,T) instead;
		//template <class WriteIt, class endian = NETP_DEF_ENDIAN>
		//__NETP_FORCE_INLINE netp::u32_t write_u8( WriteIt const& it, u8_t val) {
		//	return endian::template write_impl<u8_t>(val, it);
		//}
		//@deprecated, use netp::bytes_helper::write<WriteIt, T, big_endian|little_endian>(Writeit,T) instead;
		//template <class WriteIt, class endian = NETP_DEF_ENDIAN>
		//__NETP_FORCE_INLINE netp::u32_t write_i8( i8_t val, WriteIt const& it) {
		//	return endian::template write_impl<i8_t>(val, it);
		//}
		//@deprecated, use netp::bytes_helper::write<WriteIt, T, big_endian|little_endian>(Writeit,T) instead;
		//template <class WriteIt, class endian = NETP_DEF_ENDIAN>
		//__NETP_FORCE_INLINE netp::u32_t write_u16(u16_t val , WriteIt const& it) {
		//	return endian::template write_impl<u16_t>(val, it);
		//}
		//@deprecated, use netp::bytes_helper::write<WriteIt, T, big_endian|little_endian>(Writeit,T) instead;
		//template <class WriteIt, class endian = NETP_DEF_ENDIAN>
		//__NETP_FORCE_INLINE netp::u32_t write_i16(i16_t val , WriteIt const& it) {
		//	return endian::template write_impl<i16_t>(val, it);
		//}
		//@deprecated, use netp::bytes_helper::write<WriteIt, T, big_endian|little_endian>(Writeit,T) instead;
		//template <class WriteIt, class endian = NETP_DEF_ENDIAN>
		//__NETP_FORCE_INLINE netp::u32_t write_u32(WriteIt const& it, u32_t val ) {
		//	return endian::template write_impl<WriteIt, u32_t>(val, it);
		//}
		//@deprecated, use netp::bytes_helper::write<WriteIt, T, big_endian|little_endian>(Writeit,T) instead;
		//template <class WriteIt, class endian = NETP_DEF_ENDIAN>
		//__NETP_FORCE_INLINE netp::u32_t write_i32(i32_t val, WriteIt const& it) {
		//	return endian::template write_impl<i32_t>(val, it);
		//}
		//@deprecated, use netp::bytes_helper::write<WriteIt, T, big_endian|little_endian>(Writeit,T) instead;
		//template <class WriteIt, class endian = NETP_DEF_ENDIAN>
		//__NETP_FORCE_INLINE netp::u32_t write_u64(u64_t val, WriteIt const& it) {
		//	return endian::template write_impl<u64_t>(val, it);
		//}
		//@deprecated, use netp::bytes_helper::write<WriteIt, T, big_endian|little_endian>(Writeit,T) instead;
		//template <class WriteIt, class endian = NETP_DEF_ENDIAN>
		//__NETP_FORCE_INLINE netp::u32_t write_i64(i64_t val, WriteIt const& it) {
		//	return endian::template write_impl<i64_t>(val, it);
		//}
		//template <class WriteIt>
		//__NETP_FORCE_INLINE netp::u32_t write_bytes( const byte_t* const buffer, netp::u32_t len, WriteIt const& it) {
		//	netp::u32_t write_idx = 0;
		//	for( ; write_idx < len;++write_idx ) {
		//		*(it +write_idx) = *(buffer+write_idx) ;
		//	}
		//	return write_idx;
		//}
	}
}


#endif