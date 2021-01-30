#ifndef _SHARED_HPP
#define _SHARED_HPP
#include <netp.hpp>

class stream_handler_server :
	public netp::channel_handler_abstract
{

public:
	stream_handler_server():
	channel_handler_abstract(netp::CH_ACTIVITY_CONNECTED| netp::CH_ACTIVITY_READ_CLOSED | netp::CH_ACTIVITY_CLOSED | netp::CH_INBOUND_READ)
	{}
	void connected(NRP<netp::channel_handler_context> const& ctx) {
		ctx->close();
	}
	void read_closed(NRP<netp::channel_handler_context> const& ctx) {
	}
	void closed(NRP<netp::channel_handler_context> const& ctx) {
	}
	void read(NRP<netp::channel_handler_context> const& ctx, NRP<netp::packet> const& income) {
	}
};

class stream_handler_client :
	public netp::channel_handler_abstract

{
public:
	stream_handler_client() :
		channel_handler_abstract(netp::CH_ACTIVITY_CONNECTED | netp::CH_ACTIVITY_READ_CLOSED | netp::CH_ACTIVITY_CLOSED | netp::CH_INBOUND_READ)
	{}

	void connected(NRP<netp::channel_handler_context> const& ctx) override {
	}
	void read_closed(NRP<netp::channel_handler_context> const& ctx) {
		ctx->close();
	}
	void closed(NRP<netp::channel_handler_context> const& ctx) {
	}
	void read(NRP<netp::channel_handler_context> const& ctx, NRP<netp::packet> const& income) {
	}
};

#endif