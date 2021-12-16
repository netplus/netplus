#ifndef  _NETP_HANDLER_HLEN_HPP
#define _NETP_HANDLER_HLEN_HPP

#include <queue>

#include <netp/core.hpp>
#include <netp/channel_handler.hpp>

namespace netp { namespace handler {

	class hlen final :
		public channel_handler_abstract
	{
		enum parse_state {
			HLEN_PARSE_S_READ_LEN,
			HLEN_PARSE_S_READ_CONTENT
		};

		NETP_DECLARE_NONCOPYABLE(hlen)

		using in_packet_q_t = netp::packet_queue_t;

		bool m_read_closed; //fire_read might result in read closed, we've to drop all the pending data
		u8_t m_state;
		u32_t m_size;
		u32_t m_in_q_nbytes;
		in_packet_q_t m_in_q;
		NRP<netp::packet> m_tmp_for_fire;
	public:
		hlen() :
			channel_handler_abstract(CH_INBOUND_READ| CH_OUTBOUND_WRITE|CH_ACTIVITY_CONNECTED|CH_ACTIVITY_READ_CLOSED),
			m_read_closed(true),
			m_state(HLEN_PARSE_S_READ_LEN),
			m_size(0),
			m_in_q_nbytes(0)
		{}

		virtual ~hlen() {}
		void connected(NRP<channel_handler_context> const& ctx) override;
		void read_closed(NRP<channel_handler_context> const& ctx) override;

		void read( NRP<channel_handler_context> const& ctx, NRP<packet> const& income ) override;
		void write(NRP<promise<int>> const& intp, NRP<channel_handler_context> const& ctx, NRP<packet> const& outlet) override;
	};
}}
#endif