#ifndef _NETP_HANDLER_DUMP_OUT_TEXT_HPP
#define _NETP_HANDLER_DUMP_OUT_TEXT_HPP

#include <netp/core.hpp>
#include <netp/packet.hpp>
#include <netp/channel_handler.hpp>

#include <netp/logger_broker.hpp>

namespace netp {namespace handler {

	class dump_out_text final :
	public netp::channel_handler_abstract
{
public:
	dump_out_text() :
		channel_handler_abstract(CH_OUTBOUND_WRITE)
	{}

	void write(NRP<promise<int>> const& intp, NRP<netp::channel_handler_context> const& ctx, NRP<netp::packet> const& outlet)
	{
		NETP_INFO(">>> %s\n%s", ctx->ch->ch_info().c_str(), std::string( (char*)outlet->head(), outlet->len() ).c_str() );
		ctx->write(intp, outlet);
	}
};

}}
#endif