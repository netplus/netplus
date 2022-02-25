#include <netp/socket.hpp>
#include <netp/app.hpp>

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
			case (NETP_PROTOCOL_USER):
			{
				family = NETP_AF_USER;
				stype = NETP_SOCK_USERPACKET;
			}
			break;
			default:
			{
				NETP_THROW("unknow dial url");
			}
		}

		return std::make_tuple(netp::OK, family, stype, sproto);
	}

	NRP<socket_channel> default_socket_channel_maker(NRP<netp::socket_cfg> const& cfg) {
#ifdef NETP_HAS_POLLER_IOCP
		return netp::make_ref<socket_channel_iocp>(cfg);
#endif
		return netp::make_ref<socket_channel>(cfg);
	}

	std::tuple<int, NRP<socket_channel>> create_socket_channel(NRP<netp::socket_cfg> const& cfg) {
		NETP_ASSERT(cfg->L != nullptr && cfg->L->in_event_loop());

		NRP<socket_channel> __socketch;
		if (cfg->family == NETP_AF_USER) {
			NETP_ASSERT(cfg->L->poller_type() != NETP_DEFAULT_POLLER_TYPE);
			if (cfg->ch_maker == nullptr) {
				return std::make_tuple(netp::E_CHANNEL_MISSING_MAKER, nullptr);
			}
			__socketch = cfg->ch_maker(cfg);
		} else {
			NETP_ASSERT(cfg->L->poller_type() == NETP_DEFAULT_POLLER_TYPE);
			NETP_ASSERT(cfg->ch_maker == nullptr);
			__socketch = default_socket_channel_maker(cfg);
		}

		NETP_ASSERT(__socketch != nullptr);
		int rt = __socketch->ch_init(cfg->option, cfg->kvals, cfg->sock_buf);
		if (rt != netp::OK) {
			__socketch->ch_close();
			return std::make_tuple(rt, nullptr);
		}

		NETP_ASSERT(__socketch->ch_errno() == netp::OK);
		return std::make_tuple(netp::OK, __socketch);
	}

	//we must make sure that the creation of the socket happens on its thead(L)
	void do_async_create_socket_channel(NRP<netp::promise<std::tuple<int, NRP<socket_channel>>>> const& p, NRP<netp::socket_cfg> const& cfg) {
		NETP_ASSERT(cfg->L != nullptr);
		cfg->L->execute([cfg, p]() {
			p->set(create_socket_channel(cfg));
		});
	}

	NRP<netp::promise<std::tuple<int, NRP<socket_channel>>>> async_create_socket_channel(NRP<netp::socket_cfg> const& cfg) {
		//can not be nullptr, as we don't know which group to use
		NETP_ASSERT(cfg->L != nullptr);
		NRP <netp::promise<std::tuple<int, NRP<socket_channel>>>> p = netp::make_ref<netp::promise<std::tuple<int, NRP<socket_channel>>>>();
		do_async_create_socket_channel(p, cfg);
		return p;
	}

	void do_dial(NRP<channel_dial_promise> const& ch_dialf, NRP<address> const& addr, fn_channel_initializer_t const& initializer, NRP<socket_cfg> const& cfg) {
		if (cfg->L == nullptr) {
			NETP_ASSERT(cfg->type != NETP_AF_USER);
			cfg->L = netp::app::instance()->def_loop_group()->next();
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

		NRP<promise<int>> so_dialp = netp::make_ref<promise<int>>();
		NRP<socket_channel> so = std::get<1>(tupc);
		so_dialp->if_done([ch_dialf, so](int const& rt) {
			NETP_ASSERT( so->L->in_event_loop() );
			if (rt == netp::OK) {
				ch_dialf->set(std::make_tuple(rt, so));
			} else {
				ch_dialf->set(std::make_tuple(rt, nullptr));
				NETP_ASSERT( so->ch_errno() != netp::OK );
				NETP_ASSERT( so->ch_flag() & int(channel_flag::F_CLOSED) );
			}
		});

		so->do_dial(so_dialp, addr, initializer);
	}

	void do_dial(NRP<channel_dial_promise> const& ch_dialf, netp::size_t idx, std::vector< NRP<address>, netp::allocator<NRP<address>>> const& addrs, fn_channel_initializer_t const& initializer, NRP<socket_cfg> const& cfg) {
		NETP_ASSERT( idx<addrs.size() );

		NRP<channel_dial_promise> _dp = netp::make_ref<channel_dial_promise>();
		_dp->if_done([idx, addrs, initializer, ch_dialf, cfg](std::tuple<int, NRP<channel>> const& tupc) {
			int dialrt = std::get<0>(tupc);
			if (dialrt == netp::OK) {
				ch_dialf->set(tupc);
				return;
			}

			if ((idx+1) == addrs.size()) {
				NETP_WARN("[socket]dail failed after try count: %u, last dialrt: %d", (idx+1), dialrt);
				ch_dialf->set(std::make_tuple(dialrt, nullptr));
				return;
			}

			do_dial(ch_dialf, (idx + 1), addrs, initializer, cfg);
		});

		do_dial(_dp, addrs[idx], initializer, cfg);
	}

	void do_dial(NRP<channel_dial_promise> const& ch_dialf, const char* dialurl, size_t len, fn_channel_initializer_t const& initializer, NRP<socket_cfg> const& cfg_ ) {
		socket_url_parse_info info;
		int rt = parse_socket_url(dialurl, len, info);
		if (rt != netp::OK) {
			ch_dialf->set(std::make_tuple(rt, nullptr));
			return;
		}

		NRP<socket_cfg> _dcfg = cfg_->clone();
		std::tie(rt, _dcfg->family, _dcfg->type, _dcfg->proto) = inspect_address_info_from_dial_str(info.proto.c_str());
		if (rt != netp::OK) {
			ch_dialf->set(std::make_tuple(rt, nullptr));
			return;
		}

		if (netp::is_dotipv4_decimal_notation(info.host.c_str())) {
			do_dial(ch_dialf, netp::make_ref<address>(info.host.c_str(), info.port, _dcfg->family), initializer, _dcfg);
			return;
		}

		if (cfg_->L == nullptr) {
			cfg_->L = netp::app::instance()->def_loop_group()->next();
		}

		NRP<dns_query_promise> dnsp = cfg_->L->resolve(info.host);
		dnsp->if_done([host=info.host,port = info.port, initializer, ch_dialf, _dcfg](std::tuple<int, std::vector<ipv4_t, netp::allocator<ipv4_t>>> const& tupdns) {
			if (std::get<0>(tupdns) != netp::OK) {
				NETP_WARN("[socket]dns resolve failed, host: %s, rt: %d", host.c_str(), std::get<0>(tupdns));
				ch_dialf->set(std::make_tuple(std::get<0>(tupdns), nullptr));
				return;
			}

			std::vector<ipv4_t, netp::allocator<ipv4_t>> const& ipv4s = std::get<1>(tupdns);
			if (ipv4s.size() == 0) {
				NETP_WARN("[socket]no ipv4 record for :%s", host.c_str() );
				ch_dialf->set(std::make_tuple(netp::E_SOCKET_NO_AVAILABLE_ADDR, nullptr));
				return;
			}

			std::vector<NRP<address>, netp::allocator<NRP<address>>> dialaddrs;
			for (netp::size_t i = 0; i < ipv4s.size(); ++i) {
				NRP<address > __a = netp::make_ref<address>();
				__a->setipv4(ipv4s[i]);
				__a->setport(port);
				__a->setfamily(_dcfg->family);
				dialaddrs.push_back(__a);
			}

			do_dial(ch_dialf, 0, dialaddrs, initializer, _dcfg);
		});
	}

	void do_listen_on(NRP<channel_listen_promise> const& listenp, NRP<address> const& laddr, fn_channel_initializer_t const& initializer, NRP<socket_cfg> const& cfg, int backlog) {
		NETP_ASSERT(cfg->L != nullptr);
		NETP_ASSERT(cfg->L->in_event_loop());

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
				NETP_ASSERT(so->ch_errno() != netp::OK);
				NETP_ASSERT(so->ch_flag() & int(channel_flag::F_CLOSED));
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
			cfg->L = app::instance()->def_loop_group()->next();
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
