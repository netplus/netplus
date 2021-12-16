#include <netp/handler/hlen.hpp>
#include <netp/channel_handler_context.hpp>

namespace netp { namespace handler {

	void hlen::connected(NRP<channel_handler_context> const& ctx) {
		m_tmp = netp::make_ref<netp::packet>( ctx->L->channel_rcv_buf()->left_right_capacity(),0 );
		m_read_closed = false;
		ctx->fire_connected();
	}
	void hlen::read_closed(NRP<channel_handler_context> const& ctx) {
		m_read_closed = true;
		ctx->fire_read_closed();
	}

	void hlen::read(NRP<channel_handler_context> const& ctx, NRP<packet> const& income_) {
		NETP_ASSERT(income_ != nullptr);

		NRP<packet> _income = income_;
		if (NETP_UNLIKELY(m_tmp->len())) {
			m_tmp->write(_income->head(), _income->len());
			m_tmp.swap(_income);
			m_tmp->reset(0);
		}

		bool bExit = false;
		while (!bExit && !m_read_closed) {
			switch (m_state) {
			case parse_state::S_READ_LEN:
			{
				if (_income->len() < sizeof(u32_t)) {
#ifdef _NETP_DEBUG
					NETP_ASSERT(m_tmp->len() == 0);
#endif
					m_tmp.swap(_income);
					bExit = true;
					break;
				}
				m_size = _income->read<u32_t>();
				m_state = parse_state::S_READ_CONTENT;
			}
			break;
			case parse_state::S_READ_CONTENT:
			{
				if (_income->len() >= m_size) {
					NRP<netp::packet> __income_for_fire = netp::make_ref<netp::packet>(_income->head(), m_size);
					_income->skip(m_size);
					ctx->fire_read(__income_for_fire);
					m_state = parse_state::S_READ_LEN;
				} else {
#ifdef _NETP_DEBUG
					NETP_ASSERT(m_tmp->len() == 0);
#endif
					m_tmp.swap(_income);
					bExit = true;
				}
			}
			break;
			}
		}
	}

	void hlen::write(NRP<promise<int>> const& intp, NRP<channel_handler_context> const& ctx, NRP<packet> const& outlet) {
		NRP<netp::packet> lenoutlet = netp::make_ref<netp::packet>(outlet->head(), outlet->len());
		lenoutlet->write_left<u32_t>(outlet->len() & 0xFFFFFFFF);
		ctx->write(intp, lenoutlet);
	}
}}