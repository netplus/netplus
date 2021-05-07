#include <netp.hpp>
#include <botan/auto_rng.h>

int main(int argc, char** argv) {

	netp::app _app;
	std::string dialurl = "https://botan.randombit.net";

	NRP<netp::http::client_dial_promise> dial_f = netp::http::dial(dialurl);

	dial_f->if_done([](std::tuple<int, NRP<netp::http::client>> const& tupc) {
		int dial_rt = std::get<0>(tupc);
		NETP_INFO("dial rt: %d", dial_rt);
		NRP<netp::http::client> http_client = std::get<1>(tupc);

		if (dial_rt == netp::OK) {
			NRP<netp::http::request_promise> req_promise = http_client->get("/");
			req_promise->if_done([](std::tuple<int, NRP<netp::http::message>> const& rtup) {
				int rt = std::get<0>(rtup);
				if (rt != netp::OK) {
					NETP_INFO("request failed: %d", rt);
					return;
				}

				NRP<netp::http::message> const& resp = std::get<1>(rtup);
				NETP_INFO("request response done");
				NRP<netp::packet> body = netp::make_ref<netp::packet>(resp->body->head(), resp->body->len());
				NETP_INFO("body: %s", std::string((char*)body->head(), body->len()).c_str());
			});
		}
	});

	_app.run();
	return 0;
}