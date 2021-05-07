#include <netp.hpp>

int main(int argc, char** argv) {

	netp::app _app;

//	std::string url = "http://www.neea.edu.cn/";
	std::string url = "https://www.163.com/";

	NRP<netp::http::request_promise> rp = netp::http::get(url);
	const std::tuple<int, NRP<netp::http::message>>& resp = rp->get();

	if (std::get<0>(resp) != netp::OK) {
		return std::get<0>(resp);
	}

	int httpcode = std::get<1>(resp)->code;
	if (httpcode == 200) {
		NETP_INFO("url: %s\n%s", url.c_str(), std::string((char*)std::get<1>(resp)->body->head(), std::get<1>(resp)->body->len()).c_str());
	} else {
		NETP_INFO("url: %s\nhttp response: %d %s", url.c_str(), (char*)std::get<1>(resp)->code, std::get<1>(resp)->status.c_str() );
	}

	std::string host = "http://127.0.0.1:80";
	NRP<netp::http::client_dial_promise> dial_f = netp::http::dial(host);
	
	dial_f->if_done([](std::tuple<int, NRP<netp::http::client>> const& ddup) {
		int rt = std::get<0>(ddup);
	});

	int dial_rt = std::get<0>(dial_f->get());
	NETP_INFO("dial rt: %d", dial_rt);

	if (dial_rt == netp::OK) {
		NRP<netp::http::client> http_client = std::get<1>(dial_f->get());
		NRP<netp::http::request_promise> rf = http_client->get("/phpinfo.php", std::chrono::seconds(55));

		rf->if_done([]( std::tuple<int, NRP<netp::http::message>> const& resptup ) {
			int oprt = std::get<0>(resptup);
			NRP<netp::http::message> const& httpresp = std::get<1>(resptup);

			NETP_INFO("write request rt: %d", oprt );
			if (oprt == netp::OK) {
				NETP_INFO("request response done");
			}
		});
		rf->wait();

		//
		//NRP<netp::future<int>> close_f = http_client->close();
		//int close_rt = close_f->get();
		//NETP_INFO("close rt: %d", close_rt);
		//

		nlohmann::json data;
		data["device"] = "judge_client";
		data["name"] = "robot";
		data["password"] = "robot123";

		std::string jdata = data.dump();
		NRP<netp::packet> outp = netp::make_ref<netp::packet>( jdata.length() );
		outp->write( (netp::byte_t*)jdata.c_str(), jdata.length() );

		NRP<netp::http::request_promise> pf = http_client->post("/?controller=admin&action=account_login", nullptr, outp);
		pf->if_done([](std::tuple<int, NRP<netp::http::message>> const& resptup) {
			NETP_INFO("write request rt: %d", std::get<0>(resptup));
			if (std::get<0>(resptup) == netp::OK) {
				NETP_INFO("request response done");
				NRP<netp::packet> body = std::get<1>(resptup)->body;
				NETP_INFO("body: %s", std::string((char*)body->head(), body->len()).c_str());
			}
		});
		pf->wait();

		NRP<netp::http::request_promise> rf2 = http_client->get("test.php");
		rf2->if_done([](std::tuple<int, NRP<netp::http::message>> const& resptup) {
			NETP_INFO("write request rt: %d", std::get<0>( resptup ));
			if ( std::get<0>(resptup) == netp::OK) {
				NETP_INFO("request response done");
				NRP<netp::packet> body = std::get<1>(resptup)->body;
				NETP_INFO("body: %s", std::string((char*)body->head(), body->len()).c_str());
			}
		});

		rf2->wait();
	}

	_app.run();

	return 0;
}