#ifndef _NETP_UTIL_HLEN_HPP
#define _NETP_UTIL_HLEN_HPP

#include <netp/packet.hpp>

namespace netp {

	template <typename size_width_t>
	struct util_hlen {
		enum class hlen_util_parse_state {
			HLEN_UTIL_PARSE_S_READ_LEN,
			HLEN_UTIL_PARSE_S_READ_CONTENT
		};
		typedef size_width_t hlen_util_size_t;
		hlen_util_parse_state m_state;
		hlen_util_size_t m_size;
		NRP<netp::packet> m_pkt_tmp;
		netp::u32_t m_in_q_nbytes;
		netp::packet_deque_t m_in_q;

		util_hlen() :
			m_state(hlen_util_parse_state::HLEN_UTIL_PARSE_S_READ_LEN),
			m_size(0),
			m_pkt_tmp(nullptr),
			m_in_q_nbytes(0)
		{}

		//@note: no zero-len m_size is supported
		//@return true for re-try
		inline bool decode(NRP<netp::packet>&& in_, NRP<netp::packet>& out) {
			//@NOTE: for a stream based connection, we must handle the following edge case
			// 1) len across two packets
			if (in_ && in_->len()) {
				m_in_q_nbytes += in_->len();
				//NETP_VERBOSE("[util_hlen] in_->len(): %d, m_in_qu_nbytes: %d, m_in_q.size(): %d", in_->len(), m_in_q_nbytes , (m_in_q.size()+1) );
				m_in_q.emplace_back(std::move(in_));
			}

		__label_m_in_q:
			while (m_in_q_nbytes != 0) {
#ifdef _NETP_DEBUG
				NETP_ASSERT(m_in_q.size(),"m_in_q_nbytes: %u", m_in_q_nbytes);
#endif
				NRP<netp::packet>& in = m_in_q.front();
				switch (m_state) {
				case hlen_util_parse_state::HLEN_UTIL_PARSE_S_READ_LEN:
				{
					if (m_in_q_nbytes < sizeof(hlen_util_size_t)) {
						return false;
					}

					if (in->len() < sizeof(hlen_util_size_t)) {
						NRP<netp::packet> _in = m_in_q.front();
						m_in_q.pop_front();
#ifdef _NETP_DEBUG
						NETP_ASSERT(m_in_q.size());
#endif
						m_in_q.front()->write_left(_in->head(), _in->len());
						goto __label_m_in_q;
					}

					m_size = in->read<hlen_util_size_t>();
#ifdef _NETP_DEBUG
					NETP_ASSERT(m_size > 0, "no zero size hlen packet allowed");
#endif
					m_in_q_nbytes -= sizeof(hlen_util_size_t);
					m_state = hlen_util_parse_state::HLEN_UTIL_PARSE_S_READ_CONTENT;

					//m_pkt_tmp = netp::make_ref<netp::packet>(m_size);
					if (in->len() == 0) {
						m_in_q.pop_front();
						goto __label_m_in_q;
					}
					goto __label_read_content;
				}
				break;
				case hlen_util_parse_state::HLEN_UTIL_PARSE_S_READ_CONTENT:
				{
				__label_read_content:
#ifdef _NETP_DEBUG
					NETP_ASSERT(m_pkt_tmp == nullptr ? true : m_pkt_tmp->len() );
#endif
					if (m_in_q_nbytes < m_size) {
						return false;
					}

					const netp::u32_t inlen = in->len();
					const netp::u32_t to_write = (inlen > m_size ? m_size : inlen);

					if (m_pkt_tmp == nullptr) {
						if ( (inlen<=m_size) && (inlen+in->left_right_capacity()) >= m_size) {
							//short path to save a memcpy
							m_pkt_tmp = std::move(in);
							m_in_q.pop_front();
							goto __label_skip_nbytes;
						}
						m_pkt_tmp = netp::make_ref<netp::packet>(m_size);
					}
					m_pkt_tmp->write(in->head(), to_write);
					in->skip(to_write);
					if (in->len() == 0) {
						m_in_q.pop_front();
					}

__label_skip_nbytes:
					m_in_q_nbytes -= to_write;
					m_size -= to_write;

					if (m_size == 0) {
						m_pkt_tmp.swap(out);
						m_state = hlen_util_parse_state::HLEN_UTIL_PARSE_S_READ_LEN;
						return (m_in_q_nbytes > sizeof(hlen_util_size_t));
					}
#ifdef _NETP_DEBUG
					NETP_ASSERT(m_in_q_nbytes > 0);
#endif
				}
				break;
				}
			}
			//if in->len == 0, we get here
			return false;
		}

		__NETP_FORCE_INLINE
		void encode(NRP<netp::packet> const& pkt) {
#ifdef _NETP_DEBUG
			NETP_ASSERT(pkt->len() <= hlen_util_size_t(-1));
#endif
			pkt->write_left<hlen_util_size_t>(hlen_util_size_t(pkt->len()));
		}
	};
}

#endif