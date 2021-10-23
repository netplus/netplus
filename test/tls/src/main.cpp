#include <netp.hpp>
#include <botan/auto_rng.h>

int main(int argc, char** argv) {

	netp::app::instance()->init(argc, argv);
	netp::app::instance()->start_loop();
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

	netp::app::instance()->wait();
	
	NRP<netp::http::client> c = std::get<1>(dial_f->get());
	if (c != nullptr) {
		c->close()->wait();
		c = nullptr;
	}

	dial_f = nullptr;

	//we need this line here for any tls related app unless we call the following line before netp::app::instance();
	//Botan::initialize_allocator();
	netp::app::instance()->stop_loop();

	return 0;
}