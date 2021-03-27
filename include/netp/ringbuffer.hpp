#ifndef _NETP_RING_BUFFER_HPP_
#define _NETP_RING_BUFFER_HPP_

#include <netp/core.hpp>

namespace netp {

	template <class _ItemT>
	class ringbuffer {

		NETP_DECLARE_NONCOPYABLE(ringbuffer<_ItemT> ) ;

		typedef _ItemT _MyItemT;

		_MyItemT* m_buffer;

		netp::u32_t m_space;
		netp::u32_t m_begin;//write
		netp::u32_t m_end; //read
	public:
		ringbuffer( netp::u32_t const& space ) :
			m_space(space+1),
			m_begin(0),
			m_end(0),
			m_buffer(nullptr)
		{
			NETP_ASSERT( space>0 );

			m_buffer = netp::allocator<_MyItemT>::make_array(m_space);
			NETP_ALLOC_CHECK( m_buffer, (sizeof(_MyItemT)* m_space) );
		}

		~ringbuffer() {
			netp::allocator<_MyItemT>::trash_array(m_buffer);
			m_buffer = nullptr;
		}

		inline void reset() {
			m_begin = m_end = 0;
		}

		inline netp::u32_t capacity() const {
			return m_space - 1;
		}

		inline netp::u32_t left_capacity() const {
			return capacity() - count();
		}
		inline netp::u32_t count() const {
			return ((m_end-m_begin) + m_space ) % m_space;
		}
		//can not read
		inline bool is_empty() const {
			return m_begin == m_end;
		}

		//can not write
		inline bool is_full() const {
			return (m_end+1)%m_space == m_begin;
		}

		bool push( _MyItemT const& item ) {
			if( is_full() ) {
				return false;
			}

			*(m_buffer[m_end]) = item;
			m_end = (m_end+1)%m_space;
			return true;
		}

		bool pop( _MyItemT& item ) {
			if( is_empty() ) {
				return false;
			}

			item = *(m_buffer[m_begin]);
			m_begin = (m_begin+1)%m_space;
			return true;
		}
	};
}

#endif
