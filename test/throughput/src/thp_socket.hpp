#ifndef _THP_SOCKET_HPP
#define _THP_SOCKET_HPP

void read_and_write(NRP<netp::socket_channel> const& so, NRP<netp::packet> const& buf, netp::u64_t total_received_to_exit) {
	netp::u64_t total_received = 0;
	do {
		buf->reset();
		int received = netp::recv(so->ch_id(), buf->head(), buf->left_right_capacity());
		if (received > 0) {
			if (total_received_to_exit > 0) {
				total_received += received;
				if (total_received >= total_received_to_exit) {
					so->ch_close();
					break;
				}
			}
			buf->incre_write_idx(received);
			int written = netp::send(so->ch_id(), buf->head(), buf->len());
			if (written < 0) {
				so->ch_close();
				NETP_ERR("reply failed: %d", written);
				break;
			}
			NETP_ASSERT(received == written);
		}
		else if (received == 0) {
			so->ch_close();
			NETP_ERR("remote fin received: %d");
			break;
		} else if(received<0) {
			NETP_ERR("receive failed: %d", received);
			break;
		}
	} while (1);
}

void th_listener() {

	NRP<netp::socket_cfg> cfg = netp::make_ref<netp::socket_cfg>();
	cfg->family = NETP_AF_INET;
	cfg->type = NETP_SOCK_STREAM;
	cfg->proto = NETP_PROTOCOL_TCP;
	cfg->L = netp::app::instance()->def_loop_group()->next();
	cfg->option &= ~netp::u8_t(netp::socket_option::OPTION_NON_BLOCKING);
	std::tuple<int, NRP<netp::socket_channel>> tupc = netp::create_socket_channel(cfg);

	int rt = std::get<0>(tupc);
	if (rt != netp::OK) {
		NETP_ERR("create listener failed: %d", rt);
		return;
	}

	NRP<netp::socket_channel> listener = std::get<1>(tupc);

	NRP<netp::address> laddr = netp::make_ref<netp::address>("0.0.0.0", 32002, NETP_AF_INET);
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

	NRP<netp::address> raddr;
	NRP<netp::address> laddr_;
	netp::SOCKET nfd = listener->accept( raddr, laddr_);
	if (nfd == NETP_INVALID_SOCKET ) {
		NETP_ERR("accept failed: %d", rt);
		return;
	}

	NRP<netp::socket_cfg> acfg = netp::make_ref<netp::socket_cfg>();
	acfg->family = NETP_AF_INET;
	acfg->type = NETP_SOCK_STREAM;
	acfg->proto = NETP_PROTOCOL_TCP;
	acfg->fd = nfd;
	acfg->L = netp::app::instance()->def_loop_group()->next();
	acfg->option &= ~netp::u8_t(netp::socket_option::OPTION_NON_BLOCKING);

	std::tuple<int, NRP<netp::socket_channel>> accepted_tupc = netp::create_socket_channel(acfg);
	rt = std::get<0>(tupc);
	if (rt != netp::OK) {
		NETP_ERR("create accepted socket failed: %d", rt);
		return;
	}

	NRP<netp::socket_channel> accepted_so = std::get<1>(accepted_tupc);

	NRP<netp::packet> buf = netp::make_ref<netp::packet>(64 * 1024);
	read_and_write(accepted_so, buf, 0);
}

void th_dialer() {
	NRP<netp::socket_cfg> cfg = netp::make_ref<netp::socket_cfg>();
	cfg->family = NETP_AF_INET;
	cfg->type = NETP_SOCK_STREAM;
	cfg->proto = NETP_PROTOCOL_TCP;
	cfg->L = netp::app::instance()->def_loop_group()->next();
	cfg->option &= ~netp::u8_t(netp::socket_option::OPTION_NON_BLOCKING);

	std::tuple<int, NRP<netp::socket_channel>> tupc = netp::create_socket_channel(cfg);

	int rt = std::get<0>(tupc);
	if (rt != netp::OK) {
		NETP_ERR("create listener failed: %d", rt);
		return;
	}

	NRP<netp::socket_channel> dialer = std::get<1>(tupc);

	NRP<netp::address> raddr = netp::make_ref<netp::address>("127.0.0.1", 32002, NETP_AF_INET);
	rt = dialer->connect(raddr);
	if (rt != netp::OK) {
		NETP_ERR("bind failed: %d", rt);
		return;
	}

	NRP<netp::packet> buf = netp::make_ref<netp::packet>(64 * 1024);
	buf->incre_write_idx(64 * 1024);
	int written = netp::send(dialer->ch_id(), buf->head(), buf->len());
	NETP_ASSERT(written == buf->len());
	read_and_write(dialer, buf, 6553500000LL);
}

#ifdef NBLOCK_READ_WRITE
struct socket_info :
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
#endif

#endif