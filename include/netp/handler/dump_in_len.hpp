#ifndef _NETP_HANDLER_DUMP_IN_LEN_HPP
#define _NETP_HANDLER_DUMP_IN_LEN_HPP

#include <netp/core.hpp>
#include <netp/packet.hpp>
#include <netp/channel_handler.hpp>

#include <netp/logger_broker.hpp>

namespace netp {namespace handler {

	class dump_in_len final:
	public netp::channel_handler_abstract
{
public:
	dump_in_len() :channel_handler_abstract(CH_INBOUND_READ) {}
	void read(NRP<netp::channel_handler_context> const& ctx, NRP<netp::packet> const& income)
	{
		NETP_INFO("<<< %s, len: %u", ctx->ch->ch_info().c_str(), income->len() );
		ctx->fire_read(income);
	}
};

}}
#endif