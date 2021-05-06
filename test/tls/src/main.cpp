#include <netp.hpp>
#include <botan/auto_rng.h>

int main(int argc, char** argv) {

	netp::app _app;
	std::string host_ip = "207.246.84.122";
	std::string host = "botan.randombit.net";
//	int rt =  netp::get_ip_by_host(host.c_str(), host_ip);
	NETP_INFO("%s -> %s", host.c_str(), host_ip.c_str() );
	std::string dialurl = "https://" + host_ip + ":443";

	NRP<netp::handler::tls_context> tlsctx = netp::make_ref<netp::handler::tls_context>();
	tlsctx->server_info = netp::make_shared<Botan::TLS::Server_Information>("botan.randombit.net",443);
	tlsctx->rng = netp::make_shared<Botan::AutoSeeded_RNG>();
	tlsctx->session_mgr = netp::make_shared<Botan::TLS::Session_Manager_In_Memory>( *(tlsctx->rng));
	tlsctx->credentials_mgr = netp::make_shared<netp::handler::Basic_Credentials_Manager>(false,"");
	tlsctx->policy = netp::make_shared<netp::handler::Policy>( Botan::TLS::Protocol_Version::TLS_V12 );
	tlsctx->tls_version = Botan::TLS::Protocol_Version::TLS_V12;
	tlsctx->next_protocols = {};

	netp::http::dial_cfg  dcfg = { true,true, {tlsctx}, netp::make_ref<netp::socket_cfg>() };
	NRP<netp::http::client_dial_promise> dial_f = netp::http::dial(dialurl, dcfg);

	int dial_rt = std::get<0>(dial_f->get());
	NETP_INFO("dial rt: %d", dial_rt);
	NRP<netp::http::client> http_client = std::get<1>(dial_f->get());
	if(dial_rt == netp::OK) {
		NRP<netp::http::request_promise> req_promise = http_client->get("/");
		req_promise->if_done([](std::tuple<int, NRP<netp::http::message>> const& rtup) {
			int rt = std::get<0>(rtup);
			if (rt != netp::OK) {
				NETP_INFO("request failed: %d", rt );
				return;
			}

			NRP<netp::http::message> const& resp = std::get<1>(rtup);
			NETP_INFO("request response done");
			NRP<netp::packet> body = netp::make_ref<netp::packet>(resp->body->head(), resp->body->len());
			NETP_INFO("body: %s", std::string((char*)body->head(), body->len()).c_str());
		});

		req_promise->wait();
	}
	http_client = NULL;

	_app.run();
	return 0;
}