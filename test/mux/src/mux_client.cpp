#include <netp.hpp>
#include "shared.hpp"

int main(int argc, char** argv) {

	netp::app app;
	std::string dialurl = "tcp://127.0.0.1:22314";
	NRP<netp::handler::mux> h_mux = netp::make_ref<netp::handler::mux>();

	NRP<netp::channel_future> dial_f = netp::socket::dial(dialurl, [h_mux](NRP <netp::channel> const& ch) {
		ch->pipeline()->add_last(h_mux);
	});

	int rt = dial_f->get();
	NETP_ASSERT(rt == netp::OK);
	NRP<netp::handler::mux> _mux = h_mux;
	NETP_ASSERT(_mux != NULL);
	netp::handler::mux_stream_id_t id = netp::handler::mux_make_stream_id();

	NRP<netp::channel_future> dial_mux_stream_f = _mux->dial_stream(id, [](NRP<netp::channel> const& ch) {
		NRP<stream_handler_client> h = netp::make_ref<stream_handler_client>();
		ch->pipeline()->add_last(h);
	});

	dial_mux_stream_f->add_listener([](NRP<netp::channel_future> const& f) {
		int rt = f->get();
		NETP_ASSERT(rt == netp::OK);
	});
	dial_mux_stream_f->wait();
	dial_mux_stream_f->channel()->ch_close_future()->wait();

	return 0;
}