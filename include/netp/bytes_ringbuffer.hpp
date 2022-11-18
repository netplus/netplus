#ifndef _NETP_BYTES_RING_BUFFER_HPP_
#define _NETP_BYTES_RING_BUFFER_HPP_

#include <netp/smart_ptr.hpp>
#include <netp/bytes_helper.hpp>

namespace netp {

	class bytes_ringbuffer :
		public netp::ref_base {
		NETP_DECLARE_NONCOPYABLE(bytes_ringbuffer)

		enum size_range {
			MAX_RING_BUFFER_SIZE = 1024*1024*32,
			MIN_RING_BUFFER_SIZE = 1024*2
		};
	public:
		bytes_ringbuffer( netp::u32_t const& capacity ) :
			m_capacity(capacity+1),
			m_begin(0),
			m_end(0),
			m_buffer(nullptr)
		{
			reset();
		}
		~bytes_ringbuffer() {
			NETP_ASSERT(m_buffer != nullptr);
			netp::allocator<byte_t>::free( m_buffer );
			m_buffer = 0;
		}

		inline void reset() {
			m_begin = m_end = 0;

			if (m_buffer == nullptr) {
				NETP_ASSERT((m_capacity - 1) >= MIN_RING_BUFFER_SIZE && (m_capacity - 1) <= MAX_RING_BUFFER_SIZE);
				m_buffer = netp::allocator<byte_t>::malloc(m_capacity* sizeof(byte_t));
				NETP_ALLOC_CHECK(m_buffer, (m_capacity) * sizeof(byte_t));
			}
		}
		__NETP_FORCE_INLINE bool is_empty() const {
			return m_begin == m_end;
		}

		__NETP_FORCE_INLINE bool is_full() const {
			return (m_end+1) % m_capacity == m_begin;
		}

		__NETP_FORCE_INLINE netp::u32_t capacity() const {
			return m_capacity - 1;
		}
		__NETP_FORCE_INLINE netp::u32_t left_capacity() const {
			return ( capacity() - count() ) ;
		}

		__NETP_FORCE_INLINE netp::u32_t count() const {
			return ((m_end - m_begin) + m_capacity ) % m_capacity;
		}

		inline netp::u32_t peek( netp::byte_t* const buffer, netp::u32_t const& size ) {
			return read(buffer, size, false );
		}

		inline bool try_read( netp::byte_t* const buffer, netp::u32_t const& size) {
			if( count() >= size	) {
				netp::u32_t read_count = read(buffer, size);
				NETP_ASSERT( read_count == size);
				(void)read_count;
				return true;
			}
			return false ;
		}

		template <class T, class endian= NETP_DEF_ENDIAN>
		inline T read() {
			NETP_ASSERT( m_buffer != nullptr );
#ifdef _NETP_DEBUG
			byte_t tmp[sizeof(T)] = {'i'} ;
#else
			byte_t tmp[sizeof(T)] ;
#endif
			NETP_ASSERT( count() >= sizeof(T) );

			u32_t rnbytes = read( tmp, sizeof(T) );
			NETP_ASSERT( rnbytes == sizeof(T) );
			(void)rnbytes;

			return endian::read_impl( tmp, netp::bytes_helper::type<T>() );
		}

		template <class T,class endian= NETP_DEF_ENDIAN>
		inline bool try_read(T& t) {
			if( count() < sizeof(T) ) {
				return false;
			}
			return (t = read<T, endian>()), true ;
		}

		__NETP_FORCE_INLINE void skip( netp::u32_t const& s ) {
			NETP_ASSERT( s <= count() );
			m_begin = ((m_begin + s) % m_capacity) ;
			if( is_empty() ) {
				reset() ;
			}
		}

		//return read count
		netp::u32_t read( netp::byte_t* const target, netp::u32_t const& s, bool const& move_forward_r_idx = true ) {
			const netp::u32_t avail_c = count();

			if( NETP_UNLIKELY(avail_c == 0) ) {
				return 0;
			}

			//try to read as much as possible, then return the read count
			const netp::u32_t copy_c = ( avail_c > s ) ? s : avail_c ;

			if( m_end > m_begin ) {
				//[------b|-----copy-bytes|----e-------]

				::memcpy( target, (m_buffer+m_begin), copy_c );
		//		m_begin += should_copy_bytes_count;
			} else {

				const netp::u32_t tail_c = m_capacity - m_begin ;

				if( tail_c >= copy_c ) {
					//tail buffer is enough
					//[------e-----b|---copy-bytes--|----]

					std::memcpy( target, (m_buffer+m_begin ), copy_c );
//					m_begin = (m_begin + should_copy_bytes_count) % m_capacity ;

				} else {

					//[|--copy-bytes--|---e-----b|---copy-bytes----|]
					//copy tail first , then header

					std::memcpy( target, m_buffer+m_begin, tail_c );
//					m_begin = 0; //reset m_begin position
					//netp::u32_t head_c = copy_c - tail_c;
					std::memcpy( target + tail_c, m_buffer, copy_c - tail_c);
				}
			}

			if( NETP_LIKELY(move_forward_r_idx) ) {
				skip(copy_c) ;
			}

			return copy_c ;
		}

		netp::u32_t write(const byte_t* const bytes, netp::u32_t const& s ) {

			NETP_ASSERT( s != 0 );
			NETP_ASSERT( m_buffer != nullptr );

			const netp::u32_t avail_s = left_capacity();

			if( NETP_UNLIKELY(avail_s == 0) ) {
				return 0;
			}

			if( m_end == m_begin ) {
				m_end = 0;
				m_begin = 0;
			}

			//try to writes as much as possible, then return the write bytes count
			const netp::u32_t to_write_c = ( avail_s > s ) ? s : avail_s ;

			if( m_end <= m_begin ) {

				//[----------e|---write-bytes----|--b------]
				std::memcpy( m_buffer+m_end, bytes, to_write_c );
				m_end = ( m_end+to_write_c ) % m_capacity ;

			} else if( m_end > m_begin ) {
				//[------s--------e-----]

				const netp::u32_t tail_s = m_capacity - m_end;
				if( tail_s >= to_write_c ) {
					//[----------b---------e|--write-bytes--|--]
					::memcpy( m_buffer+m_end, bytes, to_write_c ) ;
					m_end = (m_end+to_write_c) % m_capacity ;
				} else {

					//[|---writes-bytes---|----b---------e|--write-bytes----|]
					std::memcpy( m_buffer+m_end, bytes, tail_s );
					m_end = to_write_c - tail_s;
					std::memcpy( m_buffer, bytes+tail_s, m_end);
				}
			}
			return to_write_c ;
		}
	private:
		u32_t m_capacity; //total capacity
		u32_t m_begin; //read
		u32_t m_end; //write

		byte_t* m_buffer;
	};
}
#endif //_BYTE_RING_BUFFER_