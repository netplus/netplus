
#include <netp.hpp>

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
};

int main(int argc, char** argv) {

	netp::app app;


	NRP<http_server> http_handler = netp::make_ref<http_server>();
	NRP<netp::channel_listen_promise> ch_lpromise = netp::listen_on("tcp://0.0.0.0:8083", [http_handler](NRP<netp::channel> const& ch) {

		NRP<netp::handler::tls_context> tlsctx = netp::handler::default_tls_server_context(std::string("./fullchain1.pem"), std::string("./privkey1.pem"));
		NRP<netp::handler::tls_server> _tls_server = netp::make_ref<netp::handler::tls_server>(tlsctx);
		ch->pipeline()->add_last(_tls_server);

		NRP<netp::handler::http> h = netp::make_ref<netp::handler::http>();
		h->bind<netp::handler::http::fn_http_message_header_t >(netp::handler::http::http_event::E_MESSAGE_HEADER, &http_server::on_message_header, http_handler, std::placeholders::_1, std::placeholders::_2);
		h->bind<netp::handler::http::fn_http_message_body_t >(netp::handler::http::http_event::E_MESSAGE_BODY, &http_server::on_message_body, http_handler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
		h->bind<netp::handler::http::fn_http_message_end_t >(netp::handler::http::http_event::E_MESSAGE_END, &http_server::on_message_end, http_handler, std::placeholders::_1 );
		ch->pipeline()->add_last(h);
	});

	int listen_rt = std::get<0>(ch_lpromise->get());
	if (listen_rt != netp::OK) {
		return listen_rt;
	}

	app.run();
//	NRP<channel_future> ch_close_f = ch_future->channel()->ch_close();
//	NETP_ASSERT(ch_close_f->get() == netp::OK);
	std::get<1>(ch_lpromise->get())->ch_close();
	std::get<1>(ch_lpromise->get())->ch_close_promise()->wait();
	NETP_ASSERT(std::get<1>(ch_lpromise->get())->ch_close_promise()->is_done());
	NETP_INFO("lsocket closed close: %d", std::get<1>(ch_lpromise->get())->ch_close_promise()->get());

	return netp::OK;
}
