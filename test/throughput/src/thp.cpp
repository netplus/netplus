#include <netp.hpp>

#define AIO

#ifndef AIO
void read_and_write(NRP<netp::socket> const& so, NRP<netp::packet> const& buf, netp::u64_t total_received_to_exit) {
	netp::u64_t total_received = 0;
	do {
		buf->reset();
		int ec = netp::OK;
		netp::u32_t len = so->recv(buf->begin(), buf->left_right_capacity(), ec);
		if (len > 0) {
			if (total_received_to_exit > 0) {
				total_received += len;
				if (total_received >= total_received_to_exit) {
					so->ch_close();
					break;
				}
			}
			buf->forward_write_index(len);
			netp::u32_t wlen = so->send(buf->begin(), buf->len(), ec);
			NETP_ASSERT(len == wlen);
		}
		else if (len == 0) {
			so->ch_close();
			NETP_ERR("remote fin received: %d", ec);
			break;
		}

		if (ec != netp::OK) {
			NETP_ERR("write failed: %d", ec);
			break;
		}
	} while (1);
}

void th_listener() {

	NRP<netp::socket_create_cfg> cfg = netp::make_ref<netp::socket_create_cfg>();
	cfg->family = netp::s_family::F_AF_INET;
	cfg->type = netp::s_type::T_STREAM;
	cfg->proto = netp::s_protocol::P_TCP;
	cfg->L = netp::io_event_loop_group::instance()->next();
	cfg->option &= ~netp::u8_t(netp::socket_option::OPTION_NON_BLOCKING);
	std::tuple<int, NRP<netp::socket>> tupc = netp::socket::create(cfg);

	int rt = std::get<0>(tupc);
	if (rt != netp::OK) {
		NETP_ERR("create listener failed: %d", rt);
		return;
	}

	NRP<netp::socket> listener = std::get<1>(tupc);

	netp::address laddr = netp::address("0.0.0.0", 32002, netp::s_family::F_AF_INET);
	rt = listener->bind(laddr);
	if (rt != netp::OK) {
		NETP_ERR("bind failed: %d", rt);
		return;
	}

	rt = listener->listen(128);
	if (rt != netp::OK) {
		NETP_ERR("listen failed: %d", rt);
		return;
	}

	netp::address raddr;
	int nfd = listener->accept(raddr);
	if (nfd < 0) {
		NETP_ERR("accept failed: %d", rt);
		return;
	}

	NRP<netp::socket_create_cfg> acfg = netp::make_ref<netp::socket_create_cfg>();
	acfg->family = netp::s_family::F_AF_INET;
	acfg->type = netp::s_type::T_STREAM;
	acfg->proto = netp::s_protocol::P_TCP;
	acfg->fd = nfd;
	acfg->L = netp::io_event_loop_group::instance()->next();
	acfg->option &= ~netp::u8_t(netp::socket_option::OPTION_NON_BLOCKING);

	std::tuple<int, NRP<netp::socket>> accepted_tupc = netp::socket::create(acfg);
	rt = std::get<0>(tupc);
	if (rt != netp::OK) {
		NETP_ERR("create accepted socket failed: %d", rt);
		return;
	}

	NRP<netp::socket> accepted_so = std::get<1>(accepted_tupc);

	NRP<netp::packet> buf = netp::make_ref<netp::packet>(64 * 1024);
	read_and_write(accepted_so, buf, 0);
}

void th_dialer() {
	NRP<netp::socket_create_cfg> cfg = netp::make_ref<netp::socket_create_cfg>();
	cfg->family = netp::s_family::F_AF_INET;
	cfg->type = netp::s_type::T_STREAM;
	cfg->proto = netp::s_protocol::P_TCP;
	cfg->L = netp::io_event_loop_group::instance()->next();
	cfg->option &= ~netp::u8_t(netp::socket_option::OPTION_NON_BLOCKING);

	std::tuple<int, NRP<netp::socket>> tupc = netp::socket::create(cfg);

	int rt = std::get<0>(tupc);
	if (rt != netp::OK) {
		NETP_ERR("create listener failed: %d", rt);
		return;
	}

	NRP<netp::socket> dialer = std::get<1>(tupc);

	netp::address raddr = netp::address("127.0.0.1", 32002, netp::s_family::F_AF_INET);
	rt = dialer->connect(raddr);
	if (rt != netp::OK) {
		NETP_ERR("bind failed: %d", rt);
		return;
	}

	NRP<netp::packet> buf = netp::make_ref<netp::packet>(64 * 1024);
	buf->forward_write_index(64 * 1024);
	int ec = netp::OK;
	netp::u32_t len = dialer->send(buf->begin(), buf->len(), ec);
	NETP_ASSERT(len == buf->len());
	read_and_write(dialer, buf, 6553500000LL);
}

#else



struct socket_info:
	public netp::ref_base
{
	netp::u64_t m_totoal_received;
	netp::u64_t m_expected;
};

void aio_read_and_write(NRP<netp::socket> const& so, NRP<netp::packet> const& buf, NRP<socket_info> const& info = nullptr) {
	so->ch_aio_read([so, buf, info](int const& rt) {
		if (rt != netp::OK) {
			so->ch_close();
			return;
		}

		buf->reset();
		int rec = netp::OK;
		netp::u32_t len = so->recv(buf->head(), buf->left_right_capacity(), rec);
		if (len > 0) {
			if (info != nullptr) {
				info->m_totoal_received += len;
				if (info->m_totoal_received >= info->m_expected) {
					::raise(SIGINT);
					so->ch_close();
					return;
				}
			}
			buf->incre_write_idx(len);
			NRP<netp::promise<int>> wp = so->ch_write(buf);
			wp->if_done([so](int const& rt) {
				NETP_ASSERT(rt != netp::E_CHANNEL_WRITE_BLOCK);
				if (rt != netp::OK) {
					NETP_ERR("aio write failed: %d", rt);
					so->ch_close();
				}
			});
		}

		if (rec == netp::OK ||
			rec == netp::E_SOCKET_READ_BLOCK
			) {
			return;
		}

		so->ch_close();
		NETP_ERR("read failed: %d", rec);
	});
}

netp::spin_mutex g_mtx;
std::vector<NRP<netp::channel>> g_channels;
long long g_client_token;
long long g_client_max;
long long g_packet_number;
long long g_packet_size;
long long g_rcvwnd;
long long g_sndwnd;
long long g_loopbufsize;

class server_echo_handler :
	public netp::channel_handler_abstract
{
public:
	server_echo_handler() :
		channel_handler_abstract(netp::channel_handler_api::CH_INBOUND_READ)
	{}

	void read(NRP<netp::channel_handler_context> const& ctx, NRP<netp::packet> const& income) {
		ctx->write(income);
	}
};


NRP<netp::channel> start_listener() {
	NRP<netp::socket_create_cfg> cfg = netp::make_ref<netp::socket_create_cfg>();
	cfg->sock_buf.rcv_size = (g_rcvwnd<<1);
	cfg->sock_buf.snd_size = (g_sndwnd);

	NRP<netp::channel_listen_promise> lp = netp::socket::listen_on("tcp://0.0.0.0:32002", [](NRP<netp::channel> const& ch) {
		ch->pipeline()->add_last( netp::make_ref<server_echo_handler>() );
	}, cfg );

	int rt = std::get<0>(lp->get());
	if (rt == netp::OK) {
		return std::get<1>(lp->get());
	}
	return nullptr;
}



class client_echo_handler :
	public netp::channel_handler_abstract
{
public:
	client_echo_handler(long long total) :
		channel_handler_abstract(netp::channel_handler_api::CH_INBOUND_READ),
		m_total_to_receive(total),
		m_total_received(0)
	{}

	void read(NRP<netp::channel_handler_context> const& ctx, NRP<netp::packet> const& income) {
		m_total_received += income->len();
		if (m_total_received >= m_total_to_receive) {
			ctx->close();
		}
		ctx->write(income);
	}

	netp::u64_t m_total_received;
	netp::u64_t m_total_to_receive;
};


void dialone() {
	NRP<netp::socket_create_cfg> cfg = netp::make_ref<netp::socket_create_cfg>();
	cfg->sock_buf.rcv_size = (g_rcvwnd);
	cfg->sock_buf.snd_size = (g_sndwnd);

	NRP<netp::channel_dial_promise> dp = netp::socket::dial("tcp://127.0.0.1:32002", [](NRP<netp::channel> const& ch) {
		ch->pipeline()->add_last(netp::make_ref<client_echo_handler>(g_packet_size*g_packet_number));
	}, cfg);

	dp->if_done([]( std::tuple<int, NRP<netp::channel>> const& tupc ) {

		if (std::get<0>(tupc) != netp::OK) {
			NETP_INFO("dial failed: %d", std::get<0>(tupc));
			netp::lock_guard<netp::spin_mutex> lg(g_mtx);
			++g_client_token;
			return;
		}

		NRP<netp::channel> ch = std::get<1>(tupc);
		{
			netp::lock_guard<netp::spin_mutex> lg(g_mtx);
			g_channels.push_back(ch);
			//if (g_channels.size() < g_client_max) {
			//	++g_client_token;
			//}
		}

		NRP<netp::packet> outp = netp::make_ref<netp::packet>(g_packet_size);
		outp->incre_write_idx(g_packet_size);
		NRP<netp::promise<int>> wp = ch->ch_write(outp);
		wp->if_done([](int const& rt) {
			NETP_ASSERT(rt == netp::OK);
		});
	});
}
#endif

void parse_param(int argc, char** argv) {
	static struct option long_options[] = {
		{"len", optional_argument, 0, 'l'}, //packet len
		{"number",optional_argument, 0, 'n'}, //packet number
		{"rcvwnd",optional_argument,0,'r'},
		{"sndwnd", optional_argument,0, 's'},
		{"clients", optional_argument, 0, 'c'},
		{"buf-for-loop", optional_argument, 0, 'b'},
		{0,0,0,0}
	};

	const char *optstring = "l:n:c:r:s:b:";

	int opt;
	int opt_idx;
	while ((opt = getopt_long(argc, argv, optstring, long_options, &opt_idx)) != -1) {
		switch (opt) {
			case 'l':
			{
				g_packet_size = std::atoll(optarg);
			}
			break;
			case 'n':
			{
				g_packet_number = std::atoll(optarg);
			}
			break;
			case 'c':
			{
				g_client_max = std::atoll(optarg);
			}
			break;
			case 'r':
			{
				g_rcvwnd = std::atoll(optarg);
			}
			break;
			case 's':
			{
				g_sndwnd = std::atoll(optarg);
			}
			break;
			case 'b':
			{
				g_loopbufsize = std::atoll(optarg);
			}
			break;
			default:
			{
				NETP_ASSERT(!"AAA");
			}
		}
	}
}

int main(int argc, char** argv) {

	g_client_token = 1;
	g_client_max = 1;
	g_packet_number = 10000;
	g_packet_size = 128;
	g_rcvwnd = 64*1024;
	g_sndwnd = 64*1024;
	g_loopbufsize = 128*1024;

	parse_param(argc, argv);

	g_client_token = g_client_max;

	netp::app_startup_cfg appcfg;

	appcfg.poller_cfgs[netp::u8_t(DEFAULT_POLLER_TYPE)].ch_buf_size = g_loopbufsize;
	netp::app _app(appcfg);

	netp::benchmark bmarker("start");
	NRP<netp::channel> listener = start_listener();
	NETP_ASSERT(listener != nullptr);
	bmarker.mark("listen done");

	do {
		{
			netp::lock_guard<netp::spin_mutex> lg(g_mtx);
			if (g_channels.size() >= g_client_max) {
				break;
			}
			if ( g_client_token>0 ) {
				--g_client_token;
				dialone();
			} else {
				netp::this_thread::sleep(1);
			}
		}
	} while (1);

	NETP_INFO("total client: %u", g_channels.size());
	bmarker.mark("dial all done");

	while (g_channels.size()) {
		NRP<netp::channel> const& ch = g_channels.back();
		ch->ch_close_promise()->wait();
		g_channels.pop_back();
	}
	bmarker.mark("wait client done");

	listener->ch_close();
	listener->ch_close_promise()->wait();
	bmarker.mark("wait listener");

	std::chrono::steady_clock::duration cost = bmarker.mark("job done");
	std::chrono::seconds sec = std::chrono::duration_cast<std::chrono::seconds>(cost);
	if (sec.count() == 0) {
		sec = std::chrono::seconds(1);
	}
	double avgbytes = g_packet_number* g_packet_size*1.0 / (sec.count()*1000*1000);
	double avgrate = g_packet_number * 1.0 / (sec.count());
	NETP_INFO("\n---\npacketsize: %lld\nnumber: %lld\ncost: %lld\navgrate: %0.2f/s\navgbytes: %0.2fMB/s\n---", 
		g_packet_size,
		g_packet_number, 
		sec.count(),
		avgrate, avgbytes);
	//_app.run();
	NETP_INFO("main exit");
	return 0;
}