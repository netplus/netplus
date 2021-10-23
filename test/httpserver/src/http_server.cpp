
#include <netp.hpp>


void write_file_content(int rt, NRP<netp::channel_handler_context> const& ctx, netp::byte_t* filechar, netp::u32_t filelen, netp::u32_t wrote) {
	if (rt < 0) {
		NETP_ERR("[download]write failed: %d", rt);
		ctx->close();
		return;
	}

	if (filelen == wrote) {
		delete filechar;
		ctx->close();
		return;
	}

	netp::u32_t bytes_per_write = 128 * 1024;
	NRP<netp::packet> outp = netp::make_ref<netp::packet>(bytes_per_write);
	netp::u32_t left = filelen - wrote;
	netp::u32_t to_write = left > bytes_per_write ? bytes_per_write : left;
	outp->write(filechar + wrote, bytes_per_write);
	wrote += to_write;

	NRP<netp::promise<int>> wp = netp::make_ref<netp::promise<int>>();
	wp->if_done([ctx, filechar, filelen, wrote](int rt) {
		write_file_content(rt, ctx, filechar, filelen, wrote);
	});
	ctx->write(wp, outp);
};

void file_download_service(NRP<netp::http::message> const& m, NRP<netp::channel_handler_context> const& ctx) {
	std::vector<netp::string_t> url_slice;
	netp::split(m->url, netp::string_t("/"), url_slice);
	netp::string_t filename = netp::string_t("./") + url_slice[url_slice.size() - 1];

	FILE* fp = fopen( filename.c_str(), "rb");

	if (fp == NULL) {
		NETP_ERR("file not found: %s", filename.c_str());
		ctx->close();
		return;
	}

	//long begin = ftell(fp);
	int seekrt = fseek(fp, 0L, SEEK_END);
	long end = ftell(fp);
	int seekbeg = fseek(fp, 0L, SEEK_SET);
	(void)seekrt;
	(void)seekbeg;

	netp::u32_t filelen = end;
	netp::byte_t* filechar = new netp::byte_t[filelen];
	::size_t rbytes = fread((char*)filechar, 1, filelen, fp);
	int fclosert = fclose(fp);

	NRP<netp::http::message> resp = netp::make_ref<netp::http::message>();
	resp->H = netp::make_ref<netp::http::header>();

	resp->H->add_header_line("content-length", netp::to_string(filelen) );
	resp->type = netp::http::T_RESP;
	resp->ver = { 1,1 };

	resp->code = 200;
	resp->status = "OK";

	NRP<netp::packet> outp;
	resp->encode(outp);


	NRP<netp::promise<int>> wp = netp::make_ref<netp::promise<int>>();
	wp->if_done([ ctx, m, filechar, filelen](int rt) {
		write_file_content(rt, ctx, filechar, filelen, 0);
	});
	ctx->write(wp, outp);
}

void echo_service(NRP<netp::http::message> const& m, NRP<netp::channel_handler_context> const& ctx) {
	bool close_after_write = false;
	NRP<netp::http::message> resp = netp::make_ref<netp::http::message>();
	resp->H = netp::make_ref<netp::http::header>();
	resp->type = netp::http::T_RESP;

	resp->ver = m->ver;
	resp->code = 200;
	resp->status = "OK";

	if (m->H->have("Connection") && m->H->get("Connection") == "Keep-Alive") {
		resp->H->add_header_line("Connection", "Keep-Alive");
	} else {
		resp->H->add_header_line("Connection", "close");
		close_after_write = true;
	}

	NRP<netp::packet> req;
	m->encode(req);

	resp->body = req;

	netp::string_t request_url = netp::string_t("req: ") + m->url ;
	resp->body->write( request_url.c_str() , request_url.length() );

	NRP<netp::packet> outp;
	resp->encode(outp);

	NRP<netp::promise<int>> f_write = ctx->write(outp);
	f_write->if_done([close_after_write, ctx](int const& rt) {
		//NETP_DEBUG("http reply done: %d", rt );
		if (close_after_write) {
			ctx->close();
		}
	});
}

class http_server :
	public netp::ref_base
{
public:
	~http_server() {}

	void on_message_header(NRP<netp::channel_handler_context> const& ctx, NRP<netp::http::message> const& m)
	{
		m->body = netp::make_ref<netp::packet>(64*1024);
		ctx->ch->set_ctx(m);
	}

	void on_message_body(NRP<netp::channel_handler_context> const& ctx, const char* data, netp::u32_t len) {
		NRP<netp::http::message> m = ctx->ch->get_ctx<netp::http::message>();
		m->body->write(data, len);
	}

	void on_message_end(NRP<netp::channel_handler_context> const& ctx) {
		NRP<netp::http::message> m = ctx->ch->get_ctx<netp::http::message>();
		if (m->url.find("echo") != std::string::npos) {
			echo_service(m,ctx);
			return;
		}

		if (m->url.find("download") != std::string::npos) {
			file_download_service(m,ctx);
			return;
		}

		ctx->close();
	}
};

int main(int argc, char** argv) {
	Botan::initialize_allocator();
	netp::app::instance()->init(argc, argv);
	netp::app::instance()->start_loop();

	NRP<http_server> http_server_ = netp::make_ref<http_server>();
	NRP<netp::channel_listen_promise> tls_listen = netp::listen_on("tcp://0.0.0.0:50443", [http_server_](NRP<netp::channel> const& ch) {
		NRP<netp::handler::tls_config> tlsconfig = netp::make_ref<netp::handler::tls_config>();
		tlsconfig->client_cert_auth_required = false;
		tlsconfig->cert_verify_required = false;
		
		tlsconfig->cert = std::string("./tls/cert/netplus/server_cert.pem");
		tlsconfig->cert_privkey = std::string("./tls/cert/netplus/server_key.pem");
		tlsconfig->ca_path = std::string("./tls/ca/netplus/");

		//tlsconfig->cert = std::string("./tls/cert_for_server/dd.proxy/server_node_cert.pem");
		//tlsconfig->privkey = std::string("./tls/cert_for_server/dd.proxy/server_node_key.pem");
		//tlsconfig->ca_path = std::string("./tls/ca/dd.proxy/");

		NRP<netp::handler::tls_context> tlsctx = netp::handler::tls_context_with_tlsconfig(tlsconfig);
		NRP<netp::handler::tls_server> _tls_server = netp::make_ref<netp::handler::tls_server>(tlsctx);

		ch->pipeline()->add_last(_tls_server);

		NRP<netp::handler::http> h = netp::make_ref<netp::handler::http>();
		h->bind<netp::handler::http::fn_http_message_header_t >(netp::handler::http::http_event::E_MESSAGE_HEADER, &http_server::on_message_header, http_server_, std::placeholders::_1, std::placeholders::_2);
		h->bind<netp::handler::http::fn_http_message_body_t >(netp::handler::http::http_event::E_MESSAGE_BODY, &http_server::on_message_body, http_server_, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
		h->bind<netp::handler::http::fn_http_message_end_t >(netp::handler::http::http_event::E_MESSAGE_END, &http_server::on_message_end, http_server_, std::placeholders::_1 );
		ch->pipeline()->add_last(h);
	});

	int listen_rt = std::get<0>(tls_listen->get());
	if (listen_rt != netp::OK) {
		return listen_rt;
	}

	NRP<netp::channel_listen_promise> nontls_listen = netp::listen_on("tcp://0.0.0.0:50080", [http_server_](NRP<netp::channel> const& ch) {
		NRP<netp::handler::http> h = netp::make_ref<netp::handler::http>();
		h->bind<netp::handler::http::fn_http_message_header_t >(netp::handler::http::http_event::E_MESSAGE_HEADER, &http_server::on_message_header, http_server_, std::placeholders::_1, std::placeholders::_2);
		h->bind<netp::handler::http::fn_http_message_body_t >(netp::handler::http::http_event::E_MESSAGE_BODY, &http_server::on_message_body, http_server_, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
		h->bind<netp::handler::http::fn_http_message_end_t >(netp::handler::http::http_event::E_MESSAGE_END, &http_server::on_message_end, http_server_, std::placeholders::_1);
		ch->pipeline()->add_last(h);
	});

	listen_rt = std::get<0>(nontls_listen->get());
	if (listen_rt != netp::OK) {
		return listen_rt;
	}

	netp::app::instance()->wait();
//	NRP<channel_future> ch_close_f = ch_future->channel()->ch_close();
//	NETP_ASSERT(ch_close_f->get() == netp::OK);

	std::get<1>(tls_listen->get())->ch_close();
	std::get<1>(tls_listen->get())->ch_close_promise()->wait();
	NETP_ASSERT(std::get<1>(tls_listen->get())->ch_close_promise()->is_done());
	NETP_INFO("lsocket closed close: %d", std::get<1>(tls_listen->get())->ch_close_promise()->get());

	std::get<1>(nontls_listen->get())->ch_close();
	std::get<1>(nontls_listen->get())->ch_close_promise()->wait();
	NETP_ASSERT(std::get<1>(nontls_listen->get())->ch_close_promise()->is_done());
	NETP_INFO("lsocket closed close: %d", std::get<1>(nontls_listen->get())->ch_close_promise()->get());

	return netp::OK;
}
