#ifndef  _NETP_HANDLER_HLEN_HPP
#define _NETP_HANDLER_HLEN_HPP

#include <queue>

#include <netp/core.hpp>
#include <netp/channel_handler.hpp>
#include <netp/util_hlen.hpp>
namespace netp { namespace handler {

	template <typename size_width_t>
	class hlen_basic final :
		public channel_handler_abstract
	{
		NETP_DECLARE_NONCOPYABLE(hlen_basic)
		bool m_read_closed; //fire_read might result in read closed, we've to drop all the pending data
		util_hlen<size_width_t> m_util_hlen;
		NRP<netp::packet> m_tmp_for_fire;
	public:
		hlen_basic() :
			channel_handler_abstract(CH_INBOUND_READ| CH_OUTBOUND_WRITE|CH_ACTIVITY_CONNECTED|CH_ACTIVITY_READ_CLOSED),
			m_read_closed(true),
			m_util_hlen(),
			m_tmp_for_fire(nullptr)
		{}

		virtual ~hlen_basic() {}
		void connected(NRP<channel_handler_context> const& ctx) override
		{
			m_read_closed = false;
			ctx->fire_connected();
		}
		void read_closed(NRP<channel_handler_context> const& ctx) override
		{
			m_read_closed = true;
			ctx->fire_read_closed();
		}

		void read( NRP<channel_handler_context> const& ctx, NRP<packet> const& income ) override {
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

		void write(NRP<promise<int>> const& intp, NRP<channel_handler_context> const& ctx, NRP<packet> const& outlet) override 
		{
	#ifdef _NETP_DEBUG
			NETP_ASSERT(outlet->len() > 0 );
	#endif
			NRP<netp::packet> lenoutlet = netp::make_ref<netp::packet>(outlet->head(), outlet->len());
			m_util_hlen.encode(lenoutlet);
			ctx->write(intp, lenoutlet);
		}
	};

	using hlen = hlen_basic<netp::u32_t>;
}}
#endif