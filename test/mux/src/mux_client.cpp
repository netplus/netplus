#include <netp.hpp>
#include "shared.hpp"

int main(int argc, char** argv) {

	netp::app app;
	std::string dialurl = "tcp://127.0.0.1:22314";
	NRP<netp::io_event_loop> L = netp::io_event_loop_group::instance()->next();
	NRP<netp::handler::mux> h_mux = netp::make_ref<netp::handler::mux>(L);
	NRP<netp::socket_cfg> sockcfg = netp::make_ref<netp::socket_cfg>();
	sockcfg->L = L;
	NRP<netp::channel_dial_promise> dial_f = netp::socket::dial(dialurl, [h_mux](NRP <netp::channel> const& ch) {
		ch->pipeline()->add_last(h_mux);
	}, sockcfg);

	int rt = std::get<0>(dial_f->get());
	NETP_ASSERT(rt == netp::OK);

	NETP_ASSERT( sockcfg->L == std::get<1>(dial_f->get())->L );
	NRP<netp::handler::mux> _mux = h_mux;
	NETP_ASSERT(_mux != NULL);
	netp::handler::mux_stream_id_t id = netp::handler::mux_make_stream_id();

	NRP<netp::channel_dial_promise> dial_mux_stream_f = _mux->dial(id, [](NRP<netp::channel> const& ch) {
		NRP<stream_handler_client> h = netp::make_ref<stream_handler_client>();
		ch->pipeline()->add_last(h);
	} );

	dial_mux_stream_f->if_done([](std::tuple<int, NRP<netp::channel>> const& dialdup ) {
		int rt = std::get<0>(dialdup);
		NETP_ASSERT(rt == netp::OK);
	});

	dial_mux_stream_f->wait();
	std::get<1>(dial_mux_stream_f->get())->ch_close();
	std::get<1>(dial_mux_stream_f->get())->ch_close_promise()->wait();

	return 0;
}