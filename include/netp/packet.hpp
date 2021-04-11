#ifndef _NETP_PACKET_HPP_
#define _NETP_PACKET_HPP_


#include <netp/core.hpp>
#include <netp/smart_ptr.hpp>
#include <netp/bytes_helper.hpp>

#define PACK_MIN_LEFT_CAPACITY (64)
#define PACK_MIN_RIGHT_CAPACITY (128-(PACK_MIN_LEFT_CAPACITY))
#define PACK_MIN_CAPACITY ((PACK_MIN_LEFT_CAPACITY)+(PACK_MIN_RIGHT_CAPACITY))
#define PACK_MAX_CAPACITY (netp::u32::MAX)
#define PACK_DEFAULT_CAPACITY (1920)
#define PACK_INCREMENT_SIZE ((1024*4))

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

	template<class _ref_base, u32_t LEFT_RESERVE, u32_t DEFAULT_CAPACITY, u32_t AGN>
	class cap_fix_packet:
		public _ref_base
	{
		typedef cap_fix_packet<_ref_base, LEFT_RESERVE, DEFAULT_CAPACITY,AGN> fix_packet_t;

	protected:
		byte_t* m_buffer;
		u32_t	m_read_idx; //read index
		u32_t	m_write_idx; //write index
		u32_t	m_capacity; //the total buffer size

		void _init_buffer(netp::u32_t left, netp::u32_t right) {
			if (right == 0) {
				right = DEFAULT_CAPACITY - left;
			}
			NETP_ASSERT((left + right) <= PACK_MAX_CAPACITY);
			m_capacity = (left + right);
			reset(left);
			m_buffer = netp::allocator<byte_t>::malloc(sizeof(byte_t) * m_capacity, AGN);
			NETP_ALLOC_CHECK(m_buffer, sizeof(byte_t) * m_capacity);
		}

	public:
		explicit cap_fix_packet(netp::u32_t right_capacity = (DEFAULT_CAPACITY - LEFT_RESERVE), netp::u32_t left_capacity = LEFT_RESERVE) :
			m_buffer(nullptr),
			m_read_idx(0),
			m_write_idx(0)
		{
			_init_buffer(left_capacity, right_capacity);
		}

		explicit cap_fix_packet(void const* const buf, netp::u32_t len) :
			m_buffer(nullptr),
			m_read_idx(0),
			m_write_idx(0)
		{
			_init_buffer(LEFT_RESERVE, len);
			write(buf, len);
		}

		~cap_fix_packet() {
			netp::allocator<byte_t>::free(m_buffer);
		}

		__NETP_FORCE_INLINE void reset(netp::u32_t left_capacity = LEFT_RESERVE) {
#ifdef _DEBUG
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

		__NETP_FORCE_INLINE netp::u32_t len() const {
			return m_write_idx - m_read_idx;
		}

		__NETP_FORCE_INLINE void incre_write_idx(netp::u32_t bytes) {
			NETP_ASSERT((m_capacity - m_write_idx) >= bytes);
			m_write_idx += bytes;
		}

		__NETP_FORCE_INLINE void decre_write_idx(netp::u32_t bytes) {
			NETP_ASSERT((m_write_idx) >= bytes);
			m_write_idx -= bytes;
		}

		__NETP_FORCE_INLINE void incre_read_idx(netp::u32_t bytes) {
#ifdef _DEBUG
			NETP_ASSERT(bytes <= len());
#endif
			m_read_idx += bytes;
		}

		__NETP_FORCE_INLINE void decre_read_idx(netp::u32_t bytes) {
#ifdef _DEBUG
			NETP_ASSERT(m_read_idx >= bytes);
#endif
			m_read_idx -= bytes;
		}

		const inline netp::u32_t left_left_capacity() const { return (NETP_UNLIKELY(m_buffer == nullptr)) ? 0 : m_read_idx; }
		const inline netp::u32_t left_right_capacity() const { return (NETP_UNLIKELY(m_buffer == nullptr)) ? 0 : m_capacity - m_write_idx; }

		void write_left(byte_t const* buf, netp::u32_t len) {
			NETP_ASSERT(m_read_idx >= len);
			m_read_idx -= len;
			::memcpy(m_buffer + m_read_idx, buf, len);
		}

		//would result in memmove if left space is not enough
		template <class T, class endian = netp::bytes_helper::big_endian>
		inline void write_left(T t) {
			NETP_ASSERT( m_read_idx >= sizeof(T) );
			m_read_idx -= sizeof(T);
			netp::u32_t wnbytes = endian::write_impl(t, (m_buffer + m_read_idx));
			NETP_ASSERT(wnbytes == sizeof(T));
			(void)wnbytes;
		}

		template <class T, class endian = netp::bytes_helper::big_endian>
		inline void write(T t) {
			NETP_ASSERT( left_right_capacity() >= sizeof(T) ) ;
			m_write_idx += endian::write_impl(t, (m_buffer + m_write_idx));
		}

		inline void write(void const* const buf, netp::u32_t len) {
			NETP_ASSERT(left_right_capacity() >= len);
			::memcpy(m_buffer + m_write_idx, buf, len);
			m_write_idx += len;
		}

		inline void fill(u8_t b, netp::u32_t len) {
			NETP_ASSERT(left_right_capacity() >= len);
			::memset(m_buffer + m_write_idx, b, len);
			m_write_idx += len;
		}

		__NETP_FORCE_INLINE void write_zero(netp::u32_t len) {
			fill(0, len);
		}

		template <class T, class endian = netp::bytes_helper::big_endian>
		inline T read() {
#ifdef _DEBUG
			NETP_ASSERT(sizeof(T) <= len());
#endif
			const T t = endian::read_impl(m_buffer + m_read_idx, netp::bytes_helper::type<T>());
			m_read_idx += sizeof(T);
			return t;
		}

		inline netp::u32_t read(byte_t* const dst, netp::u32_t cap_) {
			if ((dst == nullptr) || cap_ == 0) { return 0; }
			netp::u32_t c = (len() > cap_ ? cap_ : len());
			::memcpy(dst, m_buffer + m_read_idx, c);
			m_read_idx += c;
			return c;
		}

		__NETP_FORCE_INLINE void skip(netp::u32_t len_) {
			incre_read_idx(len_);
		}

		template <class T, class endian = netp::bytes_helper::big_endian>
		inline T peek() const {
#ifdef _DEBUG
			NETP_ASSERT(sizeof(T) <= len());
#endif
			return endian::read_impl(m_buffer + m_read_idx, netp::bytes_helper::type<T>());
		}

		netp::u32_t peek(byte_t* const dst, netp::u32_t len_) const {
			if ((dst == nullptr) || len_ == 0) { return 0; }

			const netp::u32_t can_peek_size = len_ >= len() ? len() : len_;
			::memcpy(dst, m_buffer + m_read_idx, can_peek_size);
			return can_peek_size;
		}

		bool operator ==(fix_packet_t const& other) const {
			if (len() != other.len()) {
				return false;
			}
			return ::memcmp(head(), other.head(), len()) == 0;
		}

		inline bool operator != (fix_packet_t const& other) const {
			return !(*this == other);
		}

		inline NRP<fix_packet_t> clone() const {
			return netp::make_ref<fix_packet_t>(head(), len());
		}
	};

	template<class _ref_base, u32_t LEFT_RESERVE, u32_t DEFAULT_CAPACITY, u32_t AGN>
	class cap_expandable_packet:
		public cap_fix_packet<_ref_base, LEFT_RESERVE, DEFAULT_CAPACITY,AGN> 
	{
		typedef cap_fix_packet<_ref_base, LEFT_RESERVE, DEFAULT_CAPACITY, AGN> cap_fix_packet_t;
		typedef cap_expandable_packet<_ref_base, LEFT_RESERVE, DEFAULT_CAPACITY, AGN> expandable_packet_t;
	private:
		inline void _extend_leftbuffer_capacity__(netp::u32_t increment = PACK_INCREMENT_SIZE) {

			NETP_ASSERT(cap_fix_packet_t::m_buffer != nullptr);
			NETP_ASSERT(((cap_fix_packet_t::m_capacity + increment) <= PACK_MAX_CAPACITY));
			cap_fix_packet_t::m_capacity += increment;
			byte_t* _newbuffer = netp::allocator<byte_t>::malloc(cap_fix_packet_t::m_capacity, AGN);
			NETP_ALLOC_CHECK(_newbuffer, cap_fix_packet_t::m_capacity);

			const netp::u32_t new_left = cap_fix_packet_t::m_read_idx + increment;
			netp::u32_t _len = cap_fix_packet_t::len();
			if (_len>0) {
				std::memcpy(_newbuffer + new_left, cap_fix_packet_t::m_buffer + cap_fix_packet_t::m_read_idx, _len);
			}
			cap_fix_packet_t::m_read_idx = new_left;
			cap_fix_packet_t::m_write_idx = cap_fix_packet_t::m_read_idx+_len;
//			std::swap(_newbuffer, cap_fix_packet_t::m_buffer);
			
			netp::allocator<byte_t>::free(cap_fix_packet_t::m_buffer);
			cap_fix_packet_t::m_buffer = _newbuffer;
		}

		inline void _extend_rightbuffer_capacity__(netp::u32_t increment = PACK_INCREMENT_SIZE) {
			NETP_ASSERT(cap_fix_packet_t::m_capacity != 0);
			NETP_ASSERT(cap_fix_packet_t::m_buffer != nullptr);
			NETP_ASSERT((cap_fix_packet_t::m_capacity + increment) <= PACK_MAX_CAPACITY);
			cap_fix_packet_t::m_capacity += increment;
			byte_t* _newbuffer = netp::allocator<byte_t>::realloc(cap_fix_packet_t::m_buffer, cap_fix_packet_t::m_capacity, AGN);
			NETP_ALLOC_CHECK(_newbuffer, cap_fix_packet_t::m_capacity);
			cap_fix_packet_t::m_buffer = _newbuffer;
		}

	public:
		explicit cap_expandable_packet(netp::u32_t right_capacity = (DEFAULT_CAPACITY - LEFT_RESERVE), netp::u32_t left_capacity = LEFT_RESERVE) :
			cap_fix_packet_t(right_capacity, left_capacity)
		{
		}

		explicit cap_expandable_packet(void const* const buf, netp::u32_t len):
			cap_fix_packet_t(buf,len)
		{
		}

		void write_left( byte_t const* buf, netp::u32_t len ) {
			while ( NETP_UNLIKELY(len > (cap_fix_packet_t::left_left_capacity())) ) {
				_extend_leftbuffer_capacity__( ((len - (cap_fix_packet_t::left_left_capacity() ))<<1));
			}

#ifdef _DEBUG
			NETP_ASSERT(cap_fix_packet_t::m_read_idx >= len);
#endif
			cap_fix_packet_t::m_read_idx -= len;
			::memcpy(cap_fix_packet_t::m_buffer + cap_fix_packet_t::m_read_idx, buf, len) ;
		}

		//would result in memmove if left space is not enough
		template <class T, class endian=netp::bytes_helper::big_endian>
		inline void write_left(T t) {
			while ( NETP_UNLIKELY(sizeof(T) > (cap_fix_packet_t::left_left_capacity())) ) {
				_extend_leftbuffer_capacity__();
			}

			cap_fix_packet_t::m_read_idx -= sizeof(T);
			netp::u32_t wnbytes = endian::write_impl(t, (cap_fix_packet_t::m_buffer + cap_fix_packet_t::m_read_idx) );
#ifdef _DEBUG
			NETP_ASSERT( wnbytes == sizeof(T) );
#endif
			(void)wnbytes;
		}

		template <class T, class endian=netp::bytes_helper::big_endian>
		inline void write(T t) {
			while ( NETP_UNLIKELY(sizeof(T) > (cap_fix_packet_t::left_right_capacity())) ) {
				_extend_rightbuffer_capacity__();
			}

			cap_fix_packet_t::m_write_idx += endian::write_impl(t, (cap_fix_packet_t::m_buffer + cap_fix_packet_t::m_write_idx) );
		}

		inline void write( void const* const buf, netp::u32_t len ) {
			while ( NETP_UNLIKELY(len > (cap_fix_packet_t::left_right_capacity())) ) {
				_extend_rightbuffer_capacity__(((len - (cap_fix_packet_t::left_right_capacity()))<<1));
			}
			::memcpy(cap_fix_packet_t::m_buffer + cap_fix_packet_t::m_write_idx, buf, len) ;
			cap_fix_packet_t::m_write_idx += len;
		}

		inline void fill(u8_t b, netp::u32_t len) {
			while (NETP_UNLIKELY(len > (cap_fix_packet_t::left_right_capacity()))) {
				_extend_rightbuffer_capacity__(((len - (cap_fix_packet_t::left_right_capacity())) << 1));
			}
			::memset(cap_fix_packet_t::m_buffer + cap_fix_packet_t::m_write_idx, b, len);
			cap_fix_packet_t::m_write_idx += len;
		}

		__NETP_FORCE_INLINE void write_zero(netp::u32_t len) {
			fill(0, len);
		}

		bool operator ==(expandable_packet_t const& other) const {
			if (cap_fix_packet_t::len() != other.len()) {
				return false;
			}
			return ::memcmp(cap_fix_packet_t::head(), other.head(), cap_fix_packet_t::len()) == 0;
		}

		inline bool operator != (expandable_packet_t const& other) const {
			return !(*this == other);
		}

		inline NRP<expandable_packet_t> clone() const {
			return netp::make_ref<expandable_packet_t>(cap_fix_packet_t::head(), cap_fix_packet_t::len());
		}
	};

	using packet = cap_expandable_packet<netp::ref_base, PACK_MIN_LEFT_CAPACITY, PACK_DEFAULT_CAPACITY,NETP_DEFAULT_ALIGN>;
}
#endif