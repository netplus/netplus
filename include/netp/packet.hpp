#ifndef _NETP_PACKET_HPP_
#define _NETP_PACKET_HPP_

#include <queue>

#include <netp/core.hpp>
#include <netp/smart_ptr.hpp>
#include <netp/bytes_helper.hpp>

//Caches consist of lines, each holding multiple adjacent words.
//On Core i7, cache lines hold 64 bytes.
//64-byte lines common for Intel/AMD processors.
//64 bytes = 16 32-bit values, 8 64-bit values, etc.
//E.g., 16 32-bit array elements.

#define PACK_DEF_LEFT_CAPACITY (64)
#define PACK_DEF_RIGHT_CAPACITY (2048-(PACK_DEF_LEFT_CAPACITY)-64)
#define PACK_MAX_CAPACITY (netp::u32::MAX)
#define PACK_INCREMENT_SIZE_RIGHT ((1024*4)-64)
#define PACK_INCREMENT_SIZE_LEFT (64)

namespace netp {

	//[ head,tail )
	// memory bytes layout
	// [--writeable----bytes---bytes-----begin ------bytes--------bytes----bytes--end---writeable--]

	/*
	 * 1, head, tail writeable
	 * 2, read forward only
	 * 3, write_left from head, write from tail, 
	 * 4, always read from head, 
	 */

	template<class _ref_base, class buf_size_width_t, u32_t DEF_LEFT_RESERVE, u32_t DEF_RIGHT_CAPACITY, u32_t AGN>
	class cap_fix_packet:
		public _ref_base
	{
		typedef cap_fix_packet<_ref_base, buf_size_width_t, DEF_LEFT_RESERVE, DEF_RIGHT_CAPACITY,AGN> fix_packet_t;

		static_assert(u32_t(buf_size_width_t(-1)) >= DEF_RIGHT_CAPACITY, "DEF_RIGHT_CAPACITY check failed ");
		static_assert(u32_t(buf_size_width_t(-1)) >= DEF_LEFT_RESERVE, "DEF_LEFT_RESERVE check failed ");

	protected:
		byte_t* m_buffer;
		buf_size_width_t	m_read_idx; //read index
		buf_size_width_t	m_write_idx; //write index
		buf_size_width_t	m_capacity; //the total buffer size

		void _init_buffer(buf_size_width_t left, buf_size_width_t right) {
			if (right == 0) {
				right = (DEF_RIGHT_CAPACITY);
			}
			NETP_ASSERT((left + right) <= PACK_MAX_CAPACITY);
			m_capacity = (left + right);
			reset(left);
			m_buffer = netp::allocator<byte_t>::malloc(sizeof(byte_t) * m_capacity, AGN);
			NETP_ALLOC_CHECK(m_buffer, sizeof(byte_t) * m_capacity);
		}

	public:
		explicit cap_fix_packet(buf_size_width_t right_capacity = DEF_RIGHT_CAPACITY, buf_size_width_t left_capacity = DEF_LEFT_RESERVE) :
			m_buffer(nullptr),
			m_read_idx(0),
			m_write_idx(0)
		{
			_init_buffer(left_capacity, right_capacity);
		}

		explicit cap_fix_packet(void const* const buf, buf_size_width_t len, buf_size_width_t left_cap = DEF_LEFT_RESERVE) :
			m_buffer(nullptr),
			m_read_idx(0),
			m_write_idx(0)
		{
			_init_buffer(left_cap, len);
			write(buf, len);
		}

		~cap_fix_packet() {
			netp::allocator<byte_t>::free(m_buffer);
		}

		__NETP_FORCE_INLINE void reset(buf_size_width_t left_capacity = DEF_LEFT_RESERVE) {
#ifdef _NETP_DEBUG
			NETP_ASSERT(left_capacity < m_capacity);
#endif
			m_read_idx = m_write_idx = left_capacity;
		}

		__NETP_FORCE_INLINE byte_t* head() const {
			return m_buffer + m_read_idx;
		}

		__NETP_FORCE_INLINE byte_t* tail() const {
			return m_buffer + m_write_idx;
		}

		__NETP_FORCE_INLINE buf_size_width_t len() const {
			return m_write_idx - m_read_idx;
		}

		__NETP_FORCE_INLINE void incre_write_idx(buf_size_width_t bytes) {
			NETP_ASSERT((m_capacity - m_write_idx) >= bytes, "m_capacity: %u, m_write_idx: %u, bytes: %u", m_capacity, m_write_idx, bytes);
			m_write_idx += bytes;
		}

		__NETP_FORCE_INLINE void decre_write_idx(buf_size_width_t bytes) {
			NETP_ASSERT((m_write_idx) >= bytes);
			m_write_idx -= bytes;
		}

		__NETP_FORCE_INLINE void incre_read_idx(buf_size_width_t bytes) {
#ifdef _NETP_DEBUG
			NETP_ASSERT(bytes <= len());
#endif
			m_read_idx += bytes;
		}

		__NETP_FORCE_INLINE void decre_read_idx(buf_size_width_t bytes) {
#ifdef _NETP_DEBUG
			NETP_ASSERT(m_read_idx >= bytes);
#endif
			m_read_idx -= bytes;
		}

		const __NETP_FORCE_INLINE
		buf_size_width_t left_left_capacity() const { return (NETP_UNLIKELY(m_buffer == nullptr)) ? 0 : m_read_idx; }
		
		const __NETP_FORCE_INLINE
		buf_size_width_t left_right_capacity() const { return (NETP_UNLIKELY(m_buffer == nullptr)) ? 0 : m_capacity - m_write_idx; }

		__NETP_FORCE_INLINE
		void write_left(byte_t const* buf, buf_size_width_t len) {
			NETP_ASSERT(m_read_idx >= len);
			m_read_idx -= len;
			std::memcpy(m_buffer + m_read_idx, buf, len);
		}

		//would result in memmove if left space is not enough
		template <class T, class endian = netp::bytes_helper::big_endian>
		inline void write_left(T t) {
			NETP_ASSERT( m_read_idx >= sizeof(T) );
			m_read_idx -= sizeof(T);
			buf_size_width_t wnbytes = buf_size_width_t(endian::write_impl(t, (m_buffer + m_read_idx)));
			NETP_ASSERT(wnbytes == sizeof(T));
			(void)wnbytes;
		}

		template <class T, class endian = netp::bytes_helper::big_endian>
		inline void write(T t) {
			NETP_ASSERT( left_right_capacity() >= sizeof(T) ) ;
			m_write_idx += buf_size_width_t(endian::write_impl(t, (m_buffer + m_write_idx)));
		}

		inline void write(void const* const buf, buf_size_width_t len) {
			NETP_ASSERT(left_right_capacity() >= len, "left_right_capacity: %u, len: %u", left_right_capacity(), len);
			std::memcpy(m_buffer + m_write_idx, buf, len);
			m_write_idx += len;
		}

		inline void fill(u8_t b, buf_size_width_t len) {
			NETP_ASSERT(left_right_capacity() >= len, "left_right_capacity: %u, len: %u", left_right_capacity(), len );
			std::memset(m_buffer + m_write_idx, b, len);
			m_write_idx += len;
		}

		__NETP_FORCE_INLINE void write_zero(buf_size_width_t len) {
			fill(0, len);
		}

		template <class T, class endian = netp::bytes_helper::big_endian>
		inline T read() {
#ifdef _NETP_DEBUG
			NETP_ASSERT(sizeof(T) <= len());
#endif
			const T t = endian::read_impl(m_buffer + m_read_idx, netp::bytes_helper::type<T>());
			m_read_idx += sizeof(T);
			return t;
		}

		inline buf_size_width_t read(byte_t* const dst, buf_size_width_t cap_) {
			if ((dst == nullptr) || cap_ == 0) { return 0; }
			buf_size_width_t c = (len() > cap_ ? cap_ : len());
			std::memcpy(dst, m_buffer + m_read_idx, c);
			m_read_idx += c;
			return c;
		}

		__NETP_FORCE_INLINE void skip(buf_size_width_t len_) {
			incre_read_idx(len_);
		}

		template <class T, class endian = netp::bytes_helper::big_endian>
		inline T peek() const {
#ifdef _NETP_DEBUG
			NETP_ASSERT(sizeof(T) <= len());
#endif
			return endian::read_impl(m_buffer + m_read_idx, netp::bytes_helper::type<T>());
		}

		buf_size_width_t peek(byte_t* const dst, buf_size_width_t len_) const {
			if ((dst == nullptr) || len_ == 0) { return 0; }

			const buf_size_width_t can_peek_size = len_ >= len() ? len() : len_;
			std::memcpy(dst, m_buffer + m_read_idx, can_peek_size);
			return can_peek_size;
		}

		bool operator ==(fix_packet_t const& other) const {
			if (len() != other.len()) {
				return false;
			}
			return std::memcmp(head(), other.head(), len()) == 0;
		}

		inline bool operator != (fix_packet_t const& other) const {
			return !(*this == other);
		}

		inline NRP<fix_packet_t> clone() const {
			return netp::make_ref<fix_packet_t>(head(), len());
		}
	};

	template<class _ref_base, class _buf_width_t, u32_t DEF_LEFT_CAPACITY, u32_t DEF_RIGHT_CAPACITY, u32_t AGN>
	class cap_expandable_packet:
		public cap_fix_packet<_ref_base, _buf_width_t, DEF_LEFT_CAPACITY, DEF_RIGHT_CAPACITY,AGN>
	{
		static_assert(sizeof(_buf_width_t) >= sizeof(u32_t), "_buf_width_t check");

		typedef cap_fix_packet<_ref_base, _buf_width_t, DEF_LEFT_CAPACITY, DEF_RIGHT_CAPACITY, AGN> cap_fix_packet_t;
		typedef cap_expandable_packet<_ref_base, _buf_width_t, DEF_LEFT_CAPACITY, DEF_RIGHT_CAPACITY, AGN> expandable_packet_t;
	private:
		inline void _extend_leftbuffer_capacity__(_buf_width_t increment) {

			NETP_ASSERT(cap_fix_packet_t::m_buffer != nullptr);
			NETP_ASSERT(((cap_fix_packet_t::m_capacity + increment) <= PACK_MAX_CAPACITY));
			cap_fix_packet_t::m_capacity += increment;
			byte_t* _newbuffer = netp::allocator<byte_t>::malloc(cap_fix_packet_t::m_capacity, AGN);
			NETP_ALLOC_CHECK(_newbuffer, cap_fix_packet_t::m_capacity);

			const _buf_width_t new_left = cap_fix_packet_t::m_read_idx + increment;
			_buf_width_t _len = cap_fix_packet_t::len();
			if (_len>0) {
				std::memcpy(_newbuffer + new_left, cap_fix_packet_t::m_buffer + cap_fix_packet_t::m_read_idx, _len);
			}
			cap_fix_packet_t::m_read_idx = new_left;
			cap_fix_packet_t::m_write_idx = cap_fix_packet_t::m_read_idx+_len;
			
			netp::allocator<byte_t>::free(cap_fix_packet_t::m_buffer);
			cap_fix_packet_t::m_buffer = _newbuffer;
		}

		inline void _extend_rightbuffer_capacity__(_buf_width_t increment) {
			NETP_ASSERT(cap_fix_packet_t::m_capacity != 0);
			NETP_ASSERT(cap_fix_packet_t::m_buffer != nullptr);
			NETP_ASSERT((cap_fix_packet_t::m_capacity + increment) <= PACK_MAX_CAPACITY);
			cap_fix_packet_t::m_capacity += increment;
			byte_t* _newbuffer = netp::allocator<byte_t>::realloc(cap_fix_packet_t::m_buffer, cap_fix_packet_t::m_capacity, AGN);
			NETP_ALLOC_CHECK(_newbuffer, cap_fix_packet_t::m_capacity);
			cap_fix_packet_t::m_buffer = _newbuffer;
		}

	public:
		explicit cap_expandable_packet(_buf_width_t right_capacity = DEF_RIGHT_CAPACITY, _buf_width_t left_capacity = DEF_LEFT_CAPACITY) :
			cap_fix_packet_t(right_capacity, left_capacity)
		{
		}

		explicit cap_expandable_packet(void const* const buf, _buf_width_t len, _buf_width_t left_cap = DEF_LEFT_CAPACITY):
			cap_fix_packet_t(buf,len,left_cap)
		{
		}

		void write_left( byte_t const* buf, _buf_width_t len ) {
			while ( NETP_UNLIKELY(len > (cap_fix_packet_t::left_left_capacity())) ) {
				_extend_leftbuffer_capacity__( ((len - (cap_fix_packet_t::left_left_capacity() ))<<1));
			}

#ifdef _NETP_DEBUG
			NETP_ASSERT(cap_fix_packet_t::m_read_idx >= len);
#endif
			cap_fix_packet_t::m_read_idx -= len;
			std::memcpy(cap_fix_packet_t::m_buffer + cap_fix_packet_t::m_read_idx, buf, len) ;
		}

		//would result in memmove if left space is not enough
		template <class T, class endian=netp::bytes_helper::big_endian>
		inline void write_left(T t) {
			while ( NETP_UNLIKELY(sizeof(T) > (cap_fix_packet_t::left_left_capacity())) ) {
				_extend_leftbuffer_capacity__(PACK_INCREMENT_SIZE_LEFT);
			}

			cap_fix_packet_t::m_read_idx -= sizeof(T);
			_buf_width_t wnbytes = endian::write_impl(t, (cap_fix_packet_t::m_buffer + cap_fix_packet_t::m_read_idx) );
#ifdef _NETP_DEBUG
			NETP_ASSERT( wnbytes == sizeof(T) );
#endif
			(void)wnbytes;
		}

		template <class T, class endian=netp::bytes_helper::big_endian>
		inline void write(T t) {
			while ( NETP_UNLIKELY(sizeof(T) > (cap_fix_packet_t::left_right_capacity())) ) {
				_extend_rightbuffer_capacity__(PACK_INCREMENT_SIZE_RIGHT);
			}

			cap_fix_packet_t::m_write_idx += endian::write_impl(t, (cap_fix_packet_t::m_buffer + cap_fix_packet_t::m_write_idx) );
		}

		inline void write( void const* const buf, _buf_width_t len ) {
			while ( NETP_UNLIKELY(len > (cap_fix_packet_t::left_right_capacity())) ) {
				_extend_rightbuffer_capacity__(((len - (cap_fix_packet_t::left_right_capacity()))<<1));
			}
			std::memcpy(cap_fix_packet_t::m_buffer + cap_fix_packet_t::m_write_idx, buf, len) ;
			cap_fix_packet_t::m_write_idx += len;
		}

		inline void fill(u8_t b, _buf_width_t len) {
			while (NETP_UNLIKELY(len > (cap_fix_packet_t::left_right_capacity()))) {
				_extend_rightbuffer_capacity__(((len - (cap_fix_packet_t::left_right_capacity())) << 1));
			}
			std::memset(cap_fix_packet_t::m_buffer + cap_fix_packet_t::m_write_idx, b, len);
			cap_fix_packet_t::m_write_idx += len;
		}

		__NETP_FORCE_INLINE void write_zero(_buf_width_t len) {
			expandable_packet_t::fill(0, len);
		}

		bool operator ==(expandable_packet_t const& other) const {
			if (cap_fix_packet_t::len() != other.len()) {
				return false;
			}
			return std::memcmp(cap_fix_packet_t::head(), other.head(), cap_fix_packet_t::len()) == 0;
		}

		inline bool operator != (expandable_packet_t const& other) const {
			return !(*this == other);
		}

		inline NRP<expandable_packet_t> clone() const {
			return netp::make_ref<expandable_packet_t>(cap_fix_packet_t::head(), cap_fix_packet_t::len());
		}
	};

	using packet = cap_expandable_packet<netp::ref_base, u32_t, PACK_DEF_LEFT_CAPACITY, PACK_DEF_RIGHT_CAPACITY,NETP_DEFAULT_ALIGN>;
	using non_atomic_ref_packet = cap_expandable_packet<netp::non_atomic_ref_base, u32_t, PACK_DEF_LEFT_CAPACITY, PACK_DEF_RIGHT_CAPACITY, NETP_DEFAULT_ALIGN>;
	using non_atomic_ref_u16packet = cap_fix_packet<netp::non_atomic_ref_base, u16_t, PACK_DEF_LEFT_CAPACITY, PACK_DEF_RIGHT_CAPACITY, NETP_DEFAULT_ALIGN>;

	typedef std::deque<NRP<netp::packet>, netp::allocator<NRP<netp::packet>>> packet_deque_t;
	typedef std::queue<NRP<netp::packet>, std::deque<NRP<netp::packet>, netp::allocator<NRP<netp::packet>>>> packet_queue_t;
}
#endif