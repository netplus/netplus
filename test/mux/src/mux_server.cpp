#include <netp.hpp>
//#include <vld.h>

#include "shared.hpp"

void stream_accepted(NRP<netp::channel> const& ch) {
	NRP<stream_handler_server> h_stream = netp::make_ref<stream_handler_server>();
	ch->pipeline()->add_last(h_stream);
}

int main(int argc, char** argv) {

	netp::app app;
	std::string listenurl = "tcp://0.0.0.0:22314";

	NRP<netp::channel_listen_promise> listen_f = netp::socket::listen_on(listenurl, [](NRP<netp::channel> const& ch) {
		NRP<netp::handler::mux> h_mux = netp::make_ref<netp::handler::mux>(ch->L);
		h_mux->bind<netp::handler::fn_mux_stream_accepted_t>(netp::handler::E_MUX_CH_STREAM_ACCEPTED, &stream_accepted, std::placeholders::_1);
		ch->pipeline()->add_last(h_mux);
	});

	app.run();
	std::get<1>(listen_f->get())->ch_close();
	std::get<1>(listen_f->get())->ch_close_promise()->wait();

	return netp::OK;
}