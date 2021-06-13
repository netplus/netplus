#ifndef  _NETP_HANDLER_HLEN_HPP
#define _NETP_HANDLER_HLEN_HPP

#include <netp/core.hpp>
#include <netp/channel_handler.hpp>

namespace netp { namespace handler {

	class hlen final :
		public channel_handler_abstract
	{
		enum class parse_state {
			S_READ_LEN,
			S_READ_CONTENT
		};

		NETP_DECLARE_NONCOPYABLE(hlen)

		parse_state m_state;
		u32_t m_size;
		NRP<packet> m_tmp;
		bool m_read_closed;
	public:
		hlen() :
			channel_handler_abstract(CH_INBOUND_READ| CH_OUTBOUND_WRITE|CH_ACTIVITY_CONNECTED|CH_ACTIVITY_READ_CLOSED),
			m_state(parse_state::S_READ_LEN),
			m_size(0),
			m_tmp(nullptr),
			m_read_closed(true)
		{}

		virtual ~hlen() {}
		void connected(NRP<channel_handler_context> const& ctx) override;
		void read_closed(NRP<channel_handler_context> const& ctx) override;

		void read( NRP<channel_handler_context> const& ctx, NRP<packet> const& income ) override;
		void write(NRP<promise<int>> const& intp, NRP<channel_handler_context> const& ctx, NRP<packet> const& outlet) override;
	};
}}
#endif