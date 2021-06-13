#include <netp/handler/hlen.hpp>
#include <netp/channel_handler_context.hpp>

namespace netp { namespace handler {

	void hlen::connected(NRP<channel_handler_context> const& ctx) {
		m_read_closed = false;
	}
	void hlen::read_closed(NRP<channel_handler_context> const& ctx) {
		m_read_closed = true;
	}

	void hlen::read(NRP<channel_handler_context> const& ctx, NRP<packet> const& income) {
		NETP_ASSERT(income != nullptr);

		NRP<packet> _income = income;
		if (NETP_UNLIKELY(m_tmp != nullptr)) {
			if (m_tmp->len() < _income->left_left_capacity()) {
				_income->write_left(m_tmp->head(), m_tmp->len());
			} else {
				NRP<netp::packet> __income = netp::make_ref<netp::packet>(m_tmp->len() + income->len() );
				__income->write(m_tmp->head(), m_tmp->len());
				__income->write(income->head(), income->len());
				_income = __income;
			}
			m_tmp = nullptr;
		}

		bool bExit = false;
		while (!bExit && !m_read_closed) {
			switch (m_state) {
			case parse_state::S_READ_LEN:
			{
				if (_income->len() < sizeof(u32_t)) {
					NETP_ASSERT(m_tmp == nullptr);
					m_tmp = netp::make_ref<netp::packet>(_income->head(), _income->len());
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
					NETP_ASSERT(m_tmp == nullptr);
					m_tmp = netp::make_ref<netp::packet>(_income->head(), _income->len());
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