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
		bool rt = false;
		NRP<netp::packet> in = income;
		do {
			rt = m_util_hlen.decode(std::move(in), m_tmp_for_fire);
			if (m_tmp_for_fire) {
#ifdef _NETP_DEBUG
				NETP_ASSERT(m_tmp_for_fire->len());
#endif
				ctx->fire_read(m_tmp_for_fire);
				m_tmp_for_fire = nullptr;
			}
		} while ((rt == true) && !m_read_closed);
	}

	void hlen::write(NRP<promise<int>> const& intp, NRP<channel_handler_context> const& ctx, NRP<packet> const& outlet) {
#ifdef _NETP_DEBUG
		NETP_ASSERT(outlet->len() > 0 );
#endif
		NRP<netp::packet> lenoutlet = netp::make_ref<netp::packet>(outlet->head(), outlet->len());
		m_util_hlen.encode(lenoutlet);
		ctx->write(intp, lenoutlet);
	}
}}