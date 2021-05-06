#include <netp.hpp>

//this is a ping-pong server for websocket

class myecho :
	public netp::channel_handler_abstract
{
public:
	myecho() :
		channel_handler_abstract(netp::CH_ACTIVITY_CONNECTED)
	{
	}

	void connected(NRP<netp::channel_handler_context> const & ctx) override {
		const char* hello = "hello";
		NRP<netp::packet> outp = netp::make_ref<netp::packet>();
		outp->write(hello, netp::strlen(hello));
		NRP<netp::promise<int>> wp = ctx->write(outp);
		wp->if_done([]( const int& rt ) {
			NETP_ASSERT(rt== netp::OK);
		});
	}
};

int main(int argc, char** argv) {

	NETP_INFO("main begin");

	netp::app app;
	std::string listenurl = "tcp://0.0.0.0:8010";
	NRP<netp::channel_listen_promise> lch = netp::listen_on(listenurl, [](NRP<netp::channel> const& ch) {
		ch->ch_set_nodelay();

		NRP<netp::channel_handler_abstract> ws = netp::make_ref<netp::handler::websocket>(netp::handler::websocket_type::T_SERVER);
		ch->pipeline()->add_last(ws);

		NRP<netp::channel_handler_abstract> dump_ilen = netp::make_ref<netp::handler::dump_in_len>();
		ch->pipeline()->add_last( dump_ilen);

		NRP<netp::channel_handler_abstract> dump_olen = netp::make_ref<netp::handler::dump_out_len>();
		ch->pipeline()->add_last(dump_olen);
		
		NRP<netp::channel_handler_abstract> echo = netp::make_ref<netp::handler::echo>();
		ch->pipeline()->add_last( echo);

		ch->pipeline()->add_last( netp::make_ref<myecho>() );

	});

	if (std::get<0>( lch->get()) != netp::OK) {
		NETP_INFO("[roger]websocket server listen failed: %d", lch->get());
		return std::get<0>(lch->get());
	}
	NETP_ASSERT(std::get<1>(lch->get()) != NULL);
	NETP_INFO("[roger]websocket server listening: %s", std::get<1>(lch->get())->ch_info().c_str() );

	app.run();
	std::get<1>(lch->get())->ch_close();
	NETP_INFO("main end");
	return netp::OK;
}