#include <netp.hpp>

int main(int argc, char** argv) {

{
	netp::app _app;
	{
		std::string host_ip = "207.246.84.122";
		std::string host = "botan.randombit.net";
	//	int rt =  netp::get_ip_by_host(host.c_str(), host_ip);
		NETP_INFO("%s -> %s", host.c_str(), host_ip.c_str() );
		std::string dialurl = "https://" + host_ip + ":443";

		NRP<netp::handler::tls_context> tlsctx = netp::make_ref<netp::handler::tls_context>();
		tlsctx->server_info = netp::make_shared<Botan::TLS::Server_Information>("botan.randombit.net",443);
		tlsctx->rng = netp::make_shared<Botan::AutoSeeded_RNG>();
		tlsctx->session_mgr = netp::make_shared<Botan::TLS::Session_Manager_In_Memory>( *(tlsctx->rng));
		tlsctx->credentials_mgr = netp::make_shared<netp::handler::Basic_Credentials_Manager>("","");
		tlsctx->policy = netp::make_shared<netp::handler::Policy>( Botan::TLS::Protocol_Version::TLS_V12 );
		tlsctx->tls_version = Botan::TLS::Protocol_Version::TLS_V12;
		tlsctx->next_protocols = {};

		NRP<netp::http::client> http_client = netp::make_ref<netp::http::client>();
		NRP<netp::future<int>> dial_f = http_client->dial(dialurl, tlsctx);

		int dial_rt = dial_f->get();
		NETP_INFO("dial rt: %d", dial_rt);

		if(dial_rt == netp::OK) {
			NRP<netp::http::request> r = http_client->make_request();
			NRP<netp::http::request_future> rf = r->do_get("/");
			rf->add_listener([]( NRP<netp::http::request_future> const& rf ) {
				if(rf->get() == netp::OK) {
					NETP_INFO("request response done");
					NRP<netp::packet> body;
					rf->response()->do_read_body(body);

					NETP_INFO("body: %s", std::string((char*)body->begin(), body->len()).c_str());
				} else {
					NETP_INFO("request failed: %d", rf->get() );
				}
			});

			rf->wait();
		}
		http_client = NULL;
	}

	_app.run();
	}
	return 0;
}