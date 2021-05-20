
#include <netp.hpp>
#include "shared.hpp"

long long g_total_rpc;
std::atomic<long long> g_rpc_count_now;
long long g_total;
long long g_message_size;
std::atomic<long long> g_req_count;
std::atomic<long long> g_resp_count;
std::atomic<int> g_signal_switch;

class foo :
	public netp::ref_base
{
public:
	void ping(NRP<netp::rpc> const& r, NRP<netp::packet> const& in, NRP<netp::rpc_call_promise> const& f) {
		NRP<netp::packet> pong = netp::make_ref<netp::packet>(in->head(), in->len());
		f->set(std::make_tuple(netp::OK, pong));
	}

	void on_push(NRP<netp::rpc> const& r, NRP<netp::packet> const& in) {
		NRP<netp::promise<int>> replyp =r->push(netp::make_ref<netp::packet>(in->head(), in->len()));
		replyp->if_done([]( int const& rt ) {
			NETP_ASSERT(rt == netp::OK);
		});
	}
};

struct benchmark {
	std::chrono::time_point<std::chrono::steady_clock, std::chrono::steady_clock::duration> begin;
	std::string m_tag;
	benchmark(std::string const& tag) : m_tag(tag),
		begin(std::chrono::steady_clock::now())
	{
	}

	~benchmark() {
		std::chrono::time_point<std::chrono::steady_clock, std::chrono::steady_clock::duration> end = std::chrono::steady_clock::now();
		NETP_INFO("[%s][end]cost: %lld micro second", m_tag.c_str(), ((end - begin).count() / 1000));
	}

	std::chrono::steady_clock::duration mark(std::string const& tag) {
		std::chrono::time_point<std::chrono::steady_clock, std::chrono::steady_clock::duration> end = std::chrono::steady_clock::now();
		NETP_INFO("[%s][%s]cost: %lld micro second", m_tag.c_str(), tag.c_str(), ((end - begin).count() / 1000));
		return end-begin;
	}
};

void _client_on_push(NRP<netp::rpc> const& r, NRP<netp::packet> const& inp) {
	//NETP_ASSERT( inp->len() == 256*1024 );
	if (netp::atomic_incre(&g_resp_count) == g_total-1) {
		::raise(SIGINT);
	}
}

void push(NRP<netp::rpc> const& r , NRP<netp::packet> const& outp ) {
	if ( netp::atomic_incre(&g_req_count) != g_total) {
		NRP<netp::promise<int>> pushp = netp::make_ref<netp::promise<int>>();
		pushp->if_done([r, outp](int const& rt) {
			if (rt == netp::OK) {
				push(r, outp);
			}
		});
		r->do_push(pushp,outp);
	}
}

void call(NRP<netp::rpc> const& r, NRP<netp::packet> const& outp, long long rc) {

	NRP<netp::rpc_call_promise> cp = netp::make_ref<netp::rpc_call_promise>();
	cp->if_done([outp, r,nrc=rc+1](std::tuple<int, NRP<netp::packet>> const& tupp) {
		int rt = std::get<0>(tupp);
		if (rt != netp::OK) {
			r->close();
			return;
		}
		long long lastc = netp::atomic_incre(&g_resp_count);
		if (lastc >= g_total) {
			NETP_DEBUG("test done, rc: %lld", nrc );
			r->close();

			if (g_total_rpc == g_rpc_count_now) {
				if (netp::atomic_incre(&g_signal_switch) == 0) {
					::raise(SIGINT);
				}
			}
			return;
		}

		NETP_ASSERT(std::get<1>(tupp)->len() == outp->len());
		call(r, outp,nrc);
	});
	r->call(cp, rpc_call_test_api::API_PING, outp );
}

int main(int argc, char** argv) {
	std::shared_ptr<netp::app> _app = std::make_shared<netp::app>();

	g_total = 10000;
	g_message_size = 64;
	g_total_rpc = 1;
	g_req_count = 0;
	g_resp_count = 0;
	g_rpc_count_now = 0;
	g_signal_switch = 0;

	if (argc == 2) {
		g_total = netp::to_i64(argv[1]);
	}
	else if (argc == 3) {
		g_total = netp::to_i64(argv[1]);
		g_message_size = netp::to_i64(argv[2]);
	}
	else if (argc == 4) {
		g_total = netp::to_i64(argv[1]);
		g_message_size = netp::to_i64(argv[2]);
		g_total_rpc = netp::to_i32(argv[3]);
	}

	netp::fn_rpc_activity_notify_t fn_bind_api = [](NRP<netp::rpc> const& r) {
		r->bindcall(rpc_call_test_api::API_PING, 
			[](NRP<netp::rpc> const& r, NRP<netp::packet> const& in, NRP<netp::rpc_call_promise> const& f) {
				NRP<netp::packet> pong = netp::make_ref<netp::packet>(in->head(), in->len());
				f->set(std::make_tuple(netp::OK, pong));
			});
	};

	NRP<netp::socket_cfg> cfg = netp::make_ref<netp::socket_cfg>();
	NRP<netp::rpc_listen_promise> rpc_lf = netp::rpc::listen("tcp://0.0.0.0:21001", fn_bind_api, nullptr ,cfg );

	int rt = std::get<0>(rpc_lf->get());
	if (rt != netp::OK) {
		NETP_INFO("[rpc_server]listen rpc service failed: %d", rt);
		return -1;
	}

	benchmark bhm("test rpc call");

	NRP<netp::rpc>* rpcs = new NRP<netp::rpc>[g_total_rpc];
	for (int i = 0; i < g_total_rpc; ++i) {
		NRP<netp::packet> outp = netp::make_ref<netp::packet>(g_message_size);
		outp->incre_write_idx(g_message_size);
		NRP<netp::rpc_dial_promise> rdp = netp::rpc::dial("tcp://127.0.0.1:21001", nullptr, cfg);
		rdp->if_done([outp, rpcs,i](std::tuple<int, NRP<netp::rpc>> const& tupr) {
			if (std::get<0>(tupr) != netp::OK) {
				NETP_WARN("dial failed, , failed rt: %d", std::get<0>(tupr));
				return;
			}

			NRP<netp::rpc> r = std::get<1>(tupr);
			netp::atomic_incre(&g_rpc_count_now);
			*(rpcs + i) = r;
			call(r, outp,0);
		});
		if ((i % 200) == 0) {
			rdp->wait();
		}
	}

	_app->run();
	bhm.mark("wait rpc");
	for (int i = 0; i < g_total_rpc; ++i) {
		(*(rpcs + i))->close_promise()->wait();
		(*(rpcs + i)) = nullptr;
	}

	std::chrono::steady_clock::duration total_dur = bhm.mark("all done");
	std::chrono::seconds sec = std::chrono::duration_cast<std::chrono::seconds>(total_dur);
	NETP_INFO("cost seconds: %lld", sec.count() );
	if (sec.count() == 0) {
		sec = std::chrono::seconds(1);
	}

	double rate = (g_total*1.0) / sec.count();
	double thp = (g_total*g_message_size*1.0) / (sec.count()*1000);

	NETP_INFO("total: %u, cost seconds: %lld, rate: %0.f/s, thp: %0.f kb/s", g_total, sec.count(), rate, thp);

	std::get<1>(rpc_lf->get())->ch_close();

	return 0;
}