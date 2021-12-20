#ifndef  _NETP_HANDLER_HLEN_HPP
#define _NETP_HANDLER_HLEN_HPP

#include <queue>

#include <netp/core.hpp>
#include <netp/channel_handler.hpp>
#include <netp/util_hlen.hpp>
namespace netp { namespace handler {

	class hlen final :
		public channel_handler_abstract
	{
		NETP_DECLARE_NONCOPYABLE(hlen)
		bool m_read_closed; //fire_read might result in read closed, we've to drop all the pending data
		util_hlen<netp::u32_t> m_util_hlen;
		NRP<netp::packet> m_tmp_for_fire;
	public:
		hlen() :
			channel_handler_abstract(CH_INBOUND_READ| CH_OUTBOUND_WRITE|CH_ACTIVITY_CONNECTED|CH_ACTIVITY_READ_CLOSED),
			m_read_closed(true),
			m_util_hlen(),
			m_tmp_for_fire(nullptr)
		{}

		virtual ~hlen() {}
		void connected(NRP<channel_handler_context> const& ctx) override;
		void read_closed(NRP<channel_handler_context> const& ctx) override;

		void read( NRP<channel_handler_context> const& ctx, NRP<packet> const& income ) override;
		void write(NRP<promise<int>> const& intp, NRP<channel_handler_context> const& ctx, NRP<packet> const& outlet) override;
	};
}}
#endif