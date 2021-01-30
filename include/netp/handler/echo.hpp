#ifndef _NETP_HANDLER_ECHO_HPP
#define _NETP_HANDLER_ECHO_HPP

#include <netp/core.hpp>
#include <netp/packet.hpp>
#include <netp/channel_handler.hpp>

namespace netp {namespace handler {

	class echo final :
	public netp::channel_handler_abstract {
public:
	echo() :channel_handler_abstract(CH_INBOUND_READ) {}
	void read(NRP<netp::channel_handler_context> const& ctx, NRP<netp::packet> const& income)
	{	
		ctx->write(netp::make_ref<packet>(income->head(), income->len()));
	}
};

}}
#endif