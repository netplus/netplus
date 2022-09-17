#ifndef _THP_HANDLER_HPP
#define _THP_HANDLER_HPP
#include <netp.hpp>

#include "thp_param.hpp"
extern thp_param g_param;

NRP<netp::channel> g_listener;
std::atomic<long> g_channels; 

class server_echo_handler :
	public netp::channel_handler_abstract
{
	mode m_session_mode;
	netp::u64_t m_acked;
	netp::u64_t m_lastacked;
	netp::u32_t m_ackdelta;
public:
	server_echo_handler() :
		channel_handler_abstract(netp::channel_handler_api::CH_INBOUND_READ),
		m_session_mode(m_notset),
		m_acked(0),
		m_lastacked(0),
		m_ackdelta(1)
	{}

	void read(NRP<netp::channel_handler_context> const& ctx, NRP<netp::packet> const& income) {

_again:
		switch (m_session_mode) {
		case m_notset:
		{
			m_session_mode = (mode)income->read<netp::u8_t>();
			m_ackdelta = income->read<netp::u32_t>();
			goto _again;
		}
		break;
		case m_tps:
		{
			if ( (++m_acked % m_ackdelta) == 0 ) {
				NRP<netp::packet> outp = netp::make_ref<netp::packet>();
				outp->write<netp::u64_t>(m_acked-m_lastacked);
				m_lastacked = m_acked;
				NRP<netp::promise<int>> wp = ctx->write(outp);
				wp->if_done([](int const& rt) {
					if (rt != netp::OK) {
						NETP_ERR("write error: %d", rt);
					}
				});
			}
		}
		break;
		case m_rps:
		{
			NRP<netp::promise<int>> wp = ctx->write(income);
			wp->if_done([](int const& rt) {
				if (rt != netp::OK) {
					NETP_ERR("write error: %d", rt);
				}
			});
		}
		break;
		default:
		{
			NETP_ERR("invalid mode: %u", m_session_mode);
		}
		break;
		}
	}
};

class client_echo_handler :
	public netp::channel_handler_abstract
{
public:
	client_echo_handler(long long total) :
		channel_handler_abstract(netp::channel_handler_api::CH_ACTIVITY_CONNECTED|netp::channel_handler_api::CH_INBOUND_READ),
		m_total_to_receive(total),
		m_total_received(0)
	{}

	void connected(NRP<netp::channel_handler_context> const& ctx) {
		NRP<netp::packet> outp = netp::make_ref<netp::packet>(g_param.packet_size);
		outp->incre_write_idx(g_param.packet_size);
		
		NETP_ASSERT(g_param.mode == mode::m_rps);
		outp->write_left<netp::u8_t>((g_param.mode&0xff));

		NRP<netp::promise<int>> wp = ctx->write(outp);
		wp->if_done([](int const& rt) {
			NETP_ASSERT(rt == netp::OK);
		});
	}

	void read(NRP<netp::channel_handler_context> const& ctx, NRP<netp::packet> const& income) {
		m_total_received += income->len();
		if (m_total_received >= m_total_to_receive) {
			ctx->close();
			long channels = netp::atomic_decre(&g_channels, std::memory_order_acq_rel);
			if (channels == 1) {
				//close listener once the test is done
				::raise(SIGTERM);
			}
			return;
		}
		NRP<netp::promise<int>> wp = ctx->write(income);
		wp->if_done([](int const& rt) {
			if (rt != netp::OK) {
				NETP_ERR("write error: %d", rt);
			}
		});
	}

	netp::u64_t m_total_received;
	netp::u64_t m_total_to_receive;
};

class client_commit_handler :
	public netp::channel_handler_abstract
{
	netp::u64_t m_total_to_commit;
	netp::u64_t m_total_commited;
	netp::u64_t m_total_acked;

public:
	client_commit_handler(long long total) :
		channel_handler_abstract( netp::channel_handler_api::CH_ACTIVITY_CONNECTED|netp::channel_handler_api::CH_INBOUND_READ),
		m_total_to_commit(total),
		m_total_commited(0),
		m_total_acked(0)
	{}

	void do_commit_done(int rt, NRP<netp::channel_handler_context> const& ctx) {
		if (rt != netp::OK) {
			NETP_WARN("write transaction failed: %d", rt);
			::raise(SIGTERM);
		}
		if (++m_total_commited < m_total_to_commit) {
			do_commit(ctx);
		}
	}

	void do_commit(NRP<netp::channel_handler_context> const& ctx) {
		NRP<netp::packet> outp = netp::make_ref<netp::packet>(g_param.packet_size);
		outp->incre_write_idx(g_param.packet_size);
		if (m_total_commited == 0) {
			//the first one
			NETP_ASSERT(g_param.mode == mode::m_tps);
			outp->write_left<netp::u32_t>(g_param.ackdelta);
			outp->write_left<netp::u8_t>((g_param.mode&0xff));
		}
		NRP<netp::promise<int>> wp = netp::make_ref<netp::promise<int>>();
		wp->if_done([cch = NRP<client_commit_handler>(this), ctx](int const& rt) {
			cch->do_commit_done(rt, ctx);
		});
		ctx->write(wp, outp);
	}

	void connected(NRP<netp::channel_handler_context> const& ctx) {
		//start commit
		do_commit(ctx);
	}

	void read(NRP<netp::channel_handler_context> const& ctx, NRP<netp::packet> const& income) {
		netp::u64_t acked = income->read<netp::u64_t>();
		m_total_acked += acked;
		NETP_INFO("transation acked: %u, total_acked: %u, commited: %u", acked, m_total_acked, m_total_commited);
		if (m_total_acked == m_total_to_commit) {
			ctx->close();
			long channels = netp::atomic_decre(&g_channels, std::memory_order_acq_rel);
			if (channels == 1) {
				//close listener once the test is done
				::raise(SIGTERM);
			}
		}
	}
};


void handler_start_listener() {
	g_channels = 0;
	NRP<netp::socket_cfg> cfg = netp::make_ref<netp::socket_cfg>();
	cfg->sock_buf = { netp::u32_t(g_param.rcvwnd), netp::u32_t(g_param.sndwnd) };

	NRP<netp::channel_listen_promise> lp = netp::listen_on("tcp://0.0.0.0:32002", [](NRP<netp::channel> const& ch) {
		ch->pipeline()->add_last(netp::make_ref<netp::handler::hlen>());
		ch->pipeline()->add_last(netp::make_ref<server_echo_handler>());
	}, cfg);

	int rt = std::get<0>(lp->get());
	if (rt == netp::OK) {
		g_listener= std::get<1>(lp->get());
	}
}

void handler_stop_listener() {
	if (g_listener != nullptr) {
		g_listener->ch_close();
		g_listener->ch_close_promise()->wait();
	}
	g_listener = nullptr;
	NETP_ASSERT(g_channels == 0);
}

void handler_dial_one_client() {
	NRP<netp::socket_cfg> cfg = netp::make_ref<netp::socket_cfg>();
	cfg->sock_buf = { netp::u32_t(g_param.rcvwnd), netp::u32_t(g_param.sndwnd) };

	NRP<netp::channel_dial_promise> dp = netp::dial("tcp://127.0.0.1:32002", [](NRP<netp::channel> const& ch) {
		ch->pipeline()->add_last(netp::make_ref<netp::handler::hlen>());

		if (g_param.mode == mode::m_rps) {
			ch->pipeline()->add_last(netp::make_ref<client_echo_handler>(netp::u64_t(g_param.packet_size) * netp::u64_t(g_param.packet_number)));
		} else if (g_param.mode == mode::m_tps) {
			ch->pipeline()->add_last(netp::make_ref<client_commit_handler>(netp::u64_t(g_param.packet_number)));
		}
	}, cfg);

	dp->if_done([](std::tuple<int, NRP<netp::channel>> const& tupc) {
		if (std::get<0>(tupc) != netp::OK) {
			NETP_WARN("dial failed: %d", std::get<0>(tupc));
			return;
		}
		netp::atomic_incre(&g_channels, std::memory_order_acq_rel);
	});
}

void handler_dial_clients() {
	long max_token = 0;
	do {
		NETP_ASSERT(max_token <= g_param.client_max);
		if (max_token == g_param.client_max) {
			break;
		} 
		++max_token;
		handler_dial_one_client();
	} while (1);
}

#endif