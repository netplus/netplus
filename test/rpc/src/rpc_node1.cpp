
#include <netp.hpp>
#include "shared.hpp"

/*
	server: listen on tcp://127.0.0.1:21001
		bindcall(api_ping, [](){});
	client:
		dial to tcp://127.0.0.1:21001
		call(api_ping, ...)
*/

int main(int argc, char** argv) {
	netp::app::instance()->init(argc, argv);
	netp::app::instance()->start_loop();

	netp::fn_rpc_activity_notify_t fn_bind_api = [](NRP<netp::rpc> const& r) {
		r->bindcall(rpc_call_test_api::API_PING, 
			[](NRP<netp::rpc> const& r, NRP<netp::packet> const& in, NRP<netp::rpc_call_promise> const& f) {
				NRP<netp::packet> pong = netp::make_ref<netp::packet>(in->head(), in->len());
				f->set(std::make_tuple(netp::OK, pong));
			});
	};

	NRP<netp::socket_cfg> cfg = netp::make_ref<netp::socket_cfg>();
	NRP<netp::rpc_listen_promise> rpc_lf = netp::rpc::listen("tcp://0.0.0.0:21001", fn_bind_api, nullptr ,cfg );

	int rt = std::get<0>(rpc_lf->get());
	if (rt != netp::OK) {
		NETP_INFO("[rpc_server]listen rpc service failed: %d", rt);
		return -1;
	}

	NRP<netp::rpc_dial_promise> rdp = netp::rpc::dial("tcp://127.0.0.1:21001", nullptr, cfg);
	rdp->if_done([](std::tuple<int, NRP<netp::rpc>> const& tupr) {
		if (std::get<0>(tupr) != netp::OK) {
			NETP_WARN("dial failed, , failed rt: %d", std::get<0>(tupr));
			return;
		}

		NRP<netp::packet> outp = netp::make_ref<netp::packet>(128);
		const char* hello_rpc = "hello rpc";
		outp->write(hello_rpc, netp::strlen(hello_rpc));
		NRP<netp::rpc> r = std::get<1>(tupr);
		NRP<netp::rpc_call_promise> rcp = r->call(rpc_call_test_api::API_PING, outp);

		rcp->if_done([](std::tuple<int, NRP<netp::packet>> const& tupp) {
			int rt = std::get<0>(tupp);
			NRP<netp::packet> rpc_reply = std::get<1>(tupp);

			NETP_INFO("rpc reply: %s", std::string((char*)rpc_reply->head(), rpc_reply->len()).c_str());
			::raise(SIGINT);
		});
	});

	netp::app::instance()->wait();
	std::get<1>(rpc_lf->get())->ch_close();

	return 0;
}