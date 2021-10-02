#ifndef _NETP_HANDLER_DUMP_IN_TEXT_HPP
#define _NETP_HANDLER_DUMP_IN_TEXT_HPP

#include <netp/core.hpp>
#include <netp/packet.hpp>
#include <netp/channel_handler.hpp>

#include <netp/app.hpp>

namespace netp {namespace handler {

	class dump_in_text final :
	public netp::channel_handler_abstract
{
public:
	dump_in_text() :
		channel_handler_abstract(CH_INBOUND_READ|CH_INBOUND_READ_FROM)
	{}

	void read(NRP<netp::channel_handler_context> const& ctx, NRP<netp::packet> const& income)
	{
		NETP_INFO("<<< %s\n%s", ctx->ch->ch_info().c_str(), std::string( (char*)income->head(), income->len() ).c_str() );
		ctx->fire_read(income);
	}

	void readfrom(NRP<netp::channel_handler_context> const& ctx, NRP<netp::packet> const& income, NRP<address> const& from)
	{
		NETP_INFO("<<< %s, from: %s\n%s", ctx->ch->ch_info().c_str(), from->to_string().c_str(), std::string((char*)income->head(), income->len()).c_str());
		ctx->fire_readfrom(income,from);
	}

};

}}
#endif