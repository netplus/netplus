#ifndef  _NETP_HANDLER_HLEN_HPP
#define _NETP_HANDLER_HLEN_HPP

#include <netp/core.hpp>
#include <netp/channel_handler.hpp>

namespace netp { namespace handler {

	class hlen final :
		public channel_handler_abstract
	{
		enum parse_state {
			S_READ_LEN,
			S_READ_CONTENT
		};

		NETP_DECLARE_NONCOPYABLE(hlen)

		parse_state m_state;
		u32_t m_size;
		NRP<packet> m_tmp;

	public:
		hlen() :
			channel_handler_abstract(CH_INBOUND_READ| CH_OUTBOUND_WRITE),
			m_state(S_READ_LEN) {}
		virtual ~hlen() {}

		void read( NRP<channel_handler_context> const& ctx, NRP<packet> const& income ) override;
		void write(NRP<channel_handler_context> const& ctx, NRP<packet> const& outlet,NRP<promise<int>> const& chp) override;
	};
}}
#endif