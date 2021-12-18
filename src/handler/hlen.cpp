#include <netp/handler/hlen.hpp>
#include <netp/channel_handler_context.hpp>

namespace netp { namespace handler {

	void hlen::connected(NRP<channel_handler_context> const& ctx) {
		m_read_closed = false;
		ctx->fire_connected();
	}
	void hlen::read_closed(NRP<channel_handler_context> const& ctx) {
		m_read_closed = true;
		ctx->fire_read_closed();
	}

	void hlen::read(NRP<channel_handler_context> const& ctx, NRP<packet> const& income) {
		NETP_ASSERT(income != nullptr && income->len());
		//@NOTE: for a stream based connection, we must handle the following edge case
		// 1) len across two packets

		m_in_q.push(income);
		m_in_q_nbytes += income->len();

		bool bExit = false;
__label_m_in_q:
		while (!bExit && !m_read_closed && m_in_q_nbytes !=0 ) {
#ifdef _NETP_DEBUG
			NETP_ASSERT(m_in_q.size());
#endif
			NRP<packet>& in = m_in_q.front();
			switch (m_state) {
			case HLEN_PARSE_S_READ_LEN:
			{
				if (m_in_q_nbytes < sizeof(u32_t)) {
					bExit = true;
					break;
				}

				if (in->len() < sizeof(u32_t)) {
					NRP<netp::packet> _in = m_in_q.front();
					m_in_q.pop();
#ifdef _NETP_DEBUG
					NETP_ASSERT(m_in_q.size());
#endif
					m_in_q.front()->write_left(_in->head(), _in->len());
					goto __label_m_in_q;
				}

				m_size = in->read<u32_t>();
#ifdef _NETP_DEBUG
				NETP_ASSERT(m_size > 0, "no zero size hlen packet allowed");
#endif
				m_in_q_nbytes -= sizeof(u32_t);
				m_state = HLEN_PARSE_S_READ_CONTENT;

#ifdef _NETP_DEBUG
				NETP_ASSERT(m_tmp_for_fire == nullptr);
#endif
				m_tmp_for_fire = netp::make_ref<netp::packet>(m_size);
				if (in->len() == 0) {
					m_in_q.pop();
					goto __label_m_in_q;
				}
				goto __label_read_content;
			}
			break;
			case HLEN_PARSE_S_READ_CONTENT:
			{
			__label_read_content:
#ifdef _NETP_DEBUG
				NETP_ASSERT(m_tmp_for_fire != nullptr);
#endif
				if (m_in_q_nbytes < m_size) {
					bExit = true;
					break;
				}
				const u32_t to_write = in->len() > m_size ? m_size : in->len();
				m_tmp_for_fire->write(in->head(), to_write);
				in->skip(to_write);

				if (in->len() == 0) {
					m_in_q.pop();
				}
				m_in_q_nbytes -= to_write;
				m_size -= to_write;

				if (m_size == 0) {
					ctx->fire_read(m_tmp_for_fire);
					m_tmp_for_fire = nullptr;
					m_state = HLEN_PARSE_S_READ_LEN;
				}
			}
			break;
			}
		}
	}

	void hlen::write(NRP<promise<int>> const& intp, NRP<channel_handler_context> const& ctx, NRP<packet> const& outlet) {
#ifdef _NETP_DEBUG
		NETP_ASSERT(outlet->len() > 0);
#endif
		NRP<netp::packet> lenoutlet = netp::make_ref<netp::packet>(outlet->head(), outlet->len());
		lenoutlet->write_left<u32_t>(outlet->len() & 0xFFFFFFFF);
		ctx->write(intp, lenoutlet);
	}
}}