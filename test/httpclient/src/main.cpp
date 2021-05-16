#include <netp.hpp>



int main(int argc, char** argv) {

	netp::app _app;



	std::string eastmoney = "http://quote.eastmoney.com/";
	NRP<netp::http::client_dial_promise> dial_f_estmoney = netp::http::dial(eastmoney);
	int diart = std::get<0>(dial_f_estmoney->get());

	if (diart != netp::OK) {
		return diart;
	}

	std::vector< std::chrono::steady_clock::duration> cost_dur;
	int faile_count=0;
	NRP<netp::http::client> http_client = std::get<1>(dial_f_estmoney->get());
	for (int i = 0; i < 10; ++i) {
		netp::benchmark bk("get", true);
		NRP<netp::http::request_promise> rp = http_client->get("/center/api/portfolio");
		int getrt = std::get<0>(rp->get());

		if (getrt != netp::OK) {
			++faile_count;
		}
		NETP_INFO("i: %d, cost: %.2f ms", i, bk.mark("done") / 1000000.0 );
	}




	netp::logger_broker::instance()->init();
	std::string url = "https://www.163.com/";

	NRP<netp::http::request_promise> rp = netp::http::get(url);

	int request_result;
	NRP<netp::http::message> http_resp;
	std::tie(request_result, http_resp) = rp->get();

	if (request_result != netp::OK) {
		return request_result;
	}

	int httpcode = http_resp->code;
	if (httpcode == 200) {
		NETP_INFO("url: %s\n%s", url.c_str(), std::string((char*)(http_resp)->body->head(), http_resp->body->len()).c_str());
	} else {
		NETP_INFO("url: %s\nhttp response: %d %s", url.c_str(), (char*)http_resp->code, http_resp->status.c_str() );
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