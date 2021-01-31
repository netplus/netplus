
#include <netp.hpp>

struct http_request_handler:
	public netp::ref_base
{
	NRP<netp::http::message> m;
};

class http_server_handler :
	public netp::ref_base
{
public:
	void on_connected(NRP<netp::channel_handler_context> const& ctx) {
		//ctx->channel()->ch_set_nodelay();
	}
	void on_read_closed(NRP<netp::channel_handler_context> const& ctx) {
		ctx->close();
	}
	void on_header_end(NRP<netp::channel_handler_context> const& ctx, NRP<netp::http::message> const& m)
	{
		NRP<http_request_handler> reqh = netp::make_ref<http_request_handler>();
		reqh->m = m;
		reqh->m->body = netp::make_ref<netp::packet>(64*1024);
		ctx->ch->set_ctx(reqh);
	}

	void on_message_body(NRP<netp::channel_handler_context> const& ctx, const char* data, netp::u32_t len) {
		NRP<http_request_handler> reqh = ctx->ch->get_ctx<http_request_handler>();
		reqh->m->body->write(data, len);
	}

	void on_message_end(NRP<netp::channel_handler_context> const& ctx) {
		NRP<netp::http::message> m = ctx->ch->get_ctx<http_request_handler>()->m;

		bool close_after_write = false;
		NRP<netp::http::message> resp = netp::make_ref<netp::http::message>();
		resp->H = netp::make_ref<netp::http::header>();
		resp->type = netp::http::T_RESP;

		resp->ver = m->ver;
		resp->code = 200;
		resp->status = "OK";

		if (m->H->have("Connection") && m->H->get("Connection") == "Keep-Alive") {
			resp->H->set("Connection", "Keep-Alive");
		}
		else {
			resp->H->set("Connection", "close");
			close_after_write = true;
		}

		NRP<netp::packet> req;
		m->encode(req);

		resp->body = req;

		NRP<netp::packet> outp;
		resp->encode(outp);

		NRP<netp::promise<int>> f_write = ctx->write(outp);
		f_write->if_done([](int const& rt) {
			NETP_ASSERT(rt == netp::OK);
			});

		if (close_after_write) {
			ctx->close();
		}
	}
};

#define ACCESS_ONCE(x) ((*(volatile typeof(x) *)&(x))
#define READ_ONCE(x) ({int  ___x=ACCESS_ONCE(x);___x;})

int main(int argc, char** argv) {

	//int a = 10;
	//NETP_ASSERT(READ_ONCE(a) == 10);
	//int b = READ_ONCE(a);
	//int b = 
	//	{
	//		int __a = 3;
	//		__a;
	//	}
	//;

	//netp::test::any_container_operation();
#ifdef _NETP_WIN
	int* vldtest = new int;
#endif
	//int fd[2];
	//int rt = netp::socket_api::posix::socketpair(netp::F_AF_INET, netp::T_STREAM, netp::P_TCP, fd);

	netp::app app;

	//typedef std::unordered_map<netp::SOCKET, NRP<netp::watch_ctx>, std::hash<netp::SOCKET>, std::equal_to<netp::SOCKET>, netp::container_allocator<netp::SOCKET>> watch_ctx_map_t;
//	NRP<netp::watch_ctx> ctx_ = netp::make_ref<netp::watch_ctx>();
	//watch_ctx_map_t ctx;
	//for (size_t i = 0; i < 1000; ++i) {
	//	ctx.insert({ 123, ctx_ });
	//}

	NRP<http_server_handler> http_handler = netp::make_ref<http_server_handler>();
	NRP<netp::channel_listen_promise> ch_lpromise = netp::socket::listen_on("tcp://0.0.0.0:8083", [http_handler](NRP<netp::channel> const& ch) {
		NRP<netp::handler::http> h = netp::make_ref<netp::handler::http>();
		h->bind<netp::handler::http::fn_http_activity_t>(netp::handler::http::http_event::E_CONNECTED, &http_server_handler::on_connected, http_handler, std::placeholders::_1);
		h->bind<netp::handler::http::fn_http_activity_t>(netp::handler::http::http_event::E_READ_CLOSED, &http_server_handler::on_read_closed, http_handler, std::placeholders::_1);
		h->bind<netp::handler::http::fn_http_message_header_t >(netp::handler::http::http_event::E_MESSAGE_HEADER, &http_server_handler::on_header_end, http_handler, std::placeholders::_1, std::placeholders::_2);
		h->bind<netp::handler::http::fn_http_message_body_t >(netp::handler::http::http_event::E_MESSAGE_BODY, &http_server_handler::on_message_body, http_handler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
		h->bind<netp::handler::http::fn_http_message_end_t >(netp::handler::http::http_event::E_MESSAGE_END, &http_server_handler::on_message_end, http_handler, std::placeholders::_1 );
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
