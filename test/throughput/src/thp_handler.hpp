#ifndef _THP_HANDLER_HPP
#define _THP_HANDLER_HPP
#include <netp.hpp>

#include "thp_param.hpp"
NRP<netp::channel> g_listener;
std::atomic<long> g_channels; 

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
			long channels = netp::atomic_decre(&g_channels, std::memory_order_acq_rel);
			if (channels == 1) {
				//close listener once the test is done
				::raise(SIGTERM);
			}
		}
		ctx->write(income);
	}

	netp::u64_t m_total_received;
	netp::u64_t m_total_to_receive;
};

void handler_start_listener(thp_param const& param_) {
	g_channels = 0;
	NRP<netp::socket_cfg> cfg = netp::make_ref<netp::socket_cfg>();
	cfg->sock_buf = { netp::u32_t(param_.rcvwnd), netp::u32_t(param_.sndwnd) };

	NRP<netp::channel_listen_promise> lp = netp::socket::listen_on("tcp://0.0.0.0:32002", [](NRP<netp::channel> const& ch) {
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

void handler_dial_one_client(thp_param const& param_) {
	NRP<netp::socket_cfg> cfg = netp::make_ref<netp::socket_cfg>();
	cfg->sock_buf = { netp::u32_t(param_.rcvwnd), netp::u32_t(param_.sndwnd) };

	NRP<netp::channel_dial_promise> dp = netp::socket::dial("tcp://127.0.0.1:32002", [&param_](NRP<netp::channel> const& ch) {
		ch->pipeline()->add_last(netp::make_ref<netp::handler::hlen>());
		ch->pipeline()->add_last(netp::make_ref<client_echo_handler>( netp::u64_t (param_.packet_size) * netp::u64_t(param_.packet_number)) );
	}, cfg);

	dp->if_done([&param_](std::tuple<int, NRP<netp::channel>> const& tupc) {
		if (std::get<0>(tupc) != netp::OK) {
			NETP_WARN("dial failed: %d", std::get<0>(tupc));
			return;
		}

		netp::atomic_incre(&g_channels, std::memory_order_acq_rel);

		NRP<netp::packet> outp = netp::make_ref<netp::packet>(param_.packet_size);
		outp->incre_write_idx(param_.packet_size);

		NRP<netp::channel> const& ch = std::get<1>(tupc);
		NRP<netp::promise<int>> wp = ch->ch_write(outp);
		wp->if_done([](int const& rt) {
			NETP_ASSERT(rt == netp::OK);
		});
	});
}

void handler_dial_clients(thp_param const& param_) {
	long max_token = 0;
	do {
		NETP_ASSERT(max_token <= param_.client_max);
		if (max_token == param_.client_max) {
			break;
		} 
		++max_token;
		handler_dial_one_client(param_);
	} while (1);
}

#endif