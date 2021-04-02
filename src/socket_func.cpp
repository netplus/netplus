#include <netp/socket.hpp>

namespace netp {

	int parse_socket_url(const char* url, size_t len, socket_url_parse_info& info) {
		std::vector<string_t> _arr;
		netp::split<string_t>(string_t(url, len), ":", _arr);
		if (_arr.size() != 3) {
			return netp::E_SOCKET_INVALID_ADDRESS;
		}

		info.proto = _arr[0];
		if (_arr[1].substr(0, 2) != "//") {
			return netp::E_SOCKET_INVALID_ADDRESS;
		}

		info.host = _arr[1].substr(2);
		info.port = netp::to_u32(_arr[2].c_str()) & 0xFFFF;

		return netp::OK;
	}

	std::tuple<int, u8_t, u8_t, u16_t> inspect_address_info_from_dial_str(const char* dialstr) {
		u16_t sproto = DEF_protocol_str_to_proto(dialstr);
		u8_t family;
		u8_t stype;
		switch (sproto) {
		case (NETP_PROTOCOL_TCP):
		{
			family = NETP_AF_INET;
			stype = NETP_SOCK_STREAM;
		}
		break;
		case (NETP_PROTOCOL_UDP):
		{
			family = NETP_AF_INET;
			stype = NETP_SOCK_DGRAM;
		}
		break;
		default:
		{
			family = (NETP_AF_USER);
			stype = NETP_SOCK_USERPACKET;
		}
		}

		return std::make_tuple(netp::OK, family, stype, sproto);
	}







	//we must make sure that the creation of the socket happens on its thead(L)
	void do_async_create_socket_channel(NRP<netp::promise<std::tuple<int, NRP<socket_channel>>>> const& p,NRP<netp::socket_cfg> const& cfg) {
		NETP_ASSERT(cfg->L != nullptr);
		cfg->L->execute([cfg, p]() {
			p->set(create_socket_channel(cfg));
		});
	}

	NRP<netp::promise<std::tuple<int, NRP<socket_channel>>>> async_create_socket_channel(NRP<netp::socket_cfg> const& cfg) {
		NRP <netp::promise<std::tuple<int, NRP<socket_channel>>>> p = netp::make_ref<netp::promise<std::tuple<int, NRP<socket_channel>>>>();
		if (cfg->L == nullptr) {
			NETP_ASSERT(cfg->proto != NETP_PROTOCOL_USER);
			cfg->L = netp::io_event_loop_group::instance()->next(NETP_DEFAULT_POLLER_TYPE);
		}
		do_async_create_socket_channel(p, cfg);
		return p;
	}

	void do_dial(NRP<channel_dial_promise> const& ch_dialf, NRP<address> const& addr, fn_channel_initializer_t const& initializer, NRP<socket_cfg> const& cfg) {
		if (cfg->L == nullptr) {
			NETP_ASSERT(cfg->type != NETP_AF_USER);
			cfg->L = io_event_loop_group::instance()->next();
		}
		if (!cfg->L->in_event_loop()) {
			cfg->L->schedule([addr, initializer, ch_dialf, cfg]() {
				do_dial(ch_dialf, addr, initializer, cfg);
			});
			return;
		}

		std::tuple<int, NRP<socket_channel>> tupc = create_socket_channel(cfg);
		int rt = std::get<0>(tupc);
		if (rt != netp::OK) {
			ch_dialf->set(std::make_tuple(rt, nullptr));
			return;
		}

		NRP<promise<int>> so_dialf = netp::make_ref<promise<int>>();
		NRP<socket_channel> so = std::get<1>(tupc);
		so_dialf->if_done([ch_dialf, so](int const& rt) {
			if (rt == netp::OK) {
				ch_dialf->set(std::make_tuple(rt, so));
			} else {
				ch_dialf->set(std::make_tuple(rt, nullptr));
				so->ch_errno() = rt;
				so->ch_flag() |= int(channel_flag::F_WRITE_ERROR);
				so->ch_close_impl(nullptr);
			}
		});

		so->do_dial(so_dialf, addr, initializer);
	}

	void do_dial(NRP<channel_dial_promise> const& ch_dialf, netp::size_t idx, std::vector< NRP<address>> const& addrs, fn_channel_initializer_t const& initializer, NRP<socket_cfg> const& cfg) {
		if (idx >= addrs.size()) {
			NETP_WARN("[socket]dail failed after try count: %u", idx);
			ch_dialf->set(std::make_tuple(netp::E_SOCKET_NO_AVAILABLE_ADDR, nullptr));
			return;
		}

		NRP<channel_dial_promise> _dp = netp::make_ref<channel_dial_promise>();
		_dp->if_done([nidx = (idx + 1), addrs, initializer, ch_dialf, cfg](std::tuple<int, NRP<channel>> const& tupc) {
			int dialrt = std::get<0>(tupc);
			if (dialrt == netp::OK) {
				ch_dialf->set(tupc);
				return;
			}

			do_dial( ch_dialf, nidx, addrs, initializer,cfg);
		});

		do_dial(_dp, addrs[idx], initializer, cfg);
	}

	void do_dial(NRP<channel_dial_promise> const& ch_dialf, const char* dialurl, size_t len, fn_channel_initializer_t const& initializer, NRP<socket_cfg> const& cfg) {
		socket_url_parse_info info;
		int rt = parse_socket_url(dialurl, len, info);
		if (rt != netp::OK) {
			ch_dialf->set(std::make_tuple(rt, nullptr));
			return;
		}

		std::tie(rt, cfg->family, cfg->type, cfg->proto) = inspect_address_info_from_dial_str(info.proto.c_str());
		if (rt != netp::OK) {
			ch_dialf->set(std::make_tuple(rt, nullptr));
			return;
		}

		if (netp::is_dotipv4_decimal_notation(info.host.c_str())) {
			do_dial(ch_dialf, netp::make_ref<address>(info.host.c_str(), info.port, cfg->family), initializer, cfg);
			return;
		}

		NRP<dns_query_promise> dnsp = netp::dns_resolver::instance()->resolve(info.host);
		dnsp->if_done([port = info.port, initializer, ch_dialf, cfg](std::tuple<int, std::vector<ipv4_t, netp::allocator<ipv4_t>>> const& tupdns) {
			if (std::get<0>(tupdns) != netp::OK) {
				ch_dialf->set(std::make_tuple(std::get<0>(tupdns), nullptr));
				return;
			}

			std::vector<ipv4_t, netp::allocator<ipv4_t>> const& ipv4s = std::get<1>(tupdns);
			NETP_ASSERT(ipv4s.size());
			std::vector<NRP<address>> dialaddrs;
			for (netp::size_t i = 0; i < ipv4s.size(); ++i) {
				NRP<address > __a = netp::make_ref<address>();
				__a->setipv4(ipv4s[i]);
				__a->setport(port);
				__a->setfamily(cfg->family);
				dialaddrs.push_back(__a);
			}

			do_dial(ch_dialf, 0, dialaddrs, initializer, cfg);
		});
	}

	void do_listen_on(NRP<channel_listen_promise> const& listenp, NRP<address> const& laddr, fn_channel_initializer_t const& initializer, NRP<socket_cfg> const& cfg, int backlog) {
		NETP_ASSERT(cfg->L != nullptr);
		NETP_ASSERT(cfg->L->in_event_loop());

		cfg->option &= ~int(socket_option::OPTION_KEEP_ALIVE);
		cfg->option &= ~int(socket_option::OPTION_NODELAY);

		std::tuple<int, NRP<socket_channel>> tupc = create_socket_channel(cfg);
		int rt = std::get<0>(tupc);
		if (rt != netp::OK) {
			NETP_WARN("[socket]do_listen_on failed: %d, listen addr: %s", rt, laddr->to_string().c_str());
			listenp->set(std::make_tuple(rt, nullptr));
			return;
		}

		NRP<socket_channel> so = std::get<1>(tupc);
		NRP<promise<int>> listen_f = netp::make_ref<promise<int>>();
		listen_f->if_done([listenp, so](int const& rt) {
			if (rt == netp::OK) {
				listenp->set(std::make_tuple(netp::OK, so));
			} else {
				listenp->set(std::make_tuple(rt, nullptr));
				so->ch_errno() = rt;
				so->ch_close_impl(nullptr);
			}
		});
		so->do_listen_on(listen_f, laddr, initializer, cfg, backlog);
	}

	NRP<channel_listen_promise> listen_on(const char* listenurl, size_t len, fn_channel_initializer_t const& initializer, NRP<socket_cfg> const& cfg, int backlog ) {
		NRP<channel_listen_promise> listenp = netp::make_ref<channel_listen_promise>();

		socket_url_parse_info info;
		int rt = parse_socket_url(listenurl, len, info);
		if (rt != netp::OK) {
			listenp->set(std::make_tuple(rt, nullptr));
			return listenp;
		}

		std::tie(rt, cfg->family, cfg->type, cfg->proto) = inspect_address_info_from_dial_str(info.proto.c_str());
		if (rt != netp::OK) {
			listenp->set(std::make_tuple(rt, nullptr));
			return listenp;
		}

		if (!netp::is_dotipv4_decimal_notation(info.host.c_str())) {
			listenp->set(std::make_tuple(netp::E_SOCKET_INVALID_ADDRESS, nullptr));
			return listenp;
		}

		NRP<address> laddr=netp::make_ref<address>(info.host.c_str(), info.port, cfg->family);
		if (cfg->L == nullptr) {
			cfg->L = io_event_loop_group::instance()->next(NETP_DEFAULT_POLLER_TYPE);
		}

		if (!cfg->L->in_event_loop()) {
			cfg->L->schedule([listenp, laddr, initializer, cfg, backlog]() {
				do_listen_on(listenp, laddr, initializer, cfg, backlog);
			});
			return listenp;
		}

		do_listen_on(listenp, laddr, initializer, cfg, backlog);
		return listenp;
	}

} //end of ns
