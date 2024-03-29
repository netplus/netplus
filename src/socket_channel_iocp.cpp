#include <netp/core.hpp>
#include <netp/app.hpp>
#include <netp/socket_channel_iocp.hpp>

#ifdef NETP_HAS_POLLER_IOCP

namespace netp {

	void socket_channel_iocp::__ch_io_cancel_connect(int status, io_ctx* ctx_) {
		iocp_ctx* ctx = (iocp_ctx*)ctx_;
		NETP_ASSERT(L->in_event_loop());
		NETP_ASSERT(status == netp::E_IO_EVENT_LOOP_NOTIFY_TERMINATING);

		NETP_ASSERT(m_chflag & int(channel_flag::F_CONNECTING) ); 
		NETP_ASSERT(ctx->ol_w->fn_ol_done != nullptr );
		ctx->ol_w->fn_ol_done(status, ctx_);

		ch_close_impl(nullptr);
	}

	int socket_channel_iocp::__iocp_do_AcceptEx(ol_ctx* olctx) {
		NETP_ASSERT(olctx != nullptr);
		olctx->accept_fd = netp::open(m_family, m_type, m_protocol);
		int ec = netp::OK;
		if (olctx->accept_fd == NETP_INVALID_SOCKET) {
			ec = netp_socket_get_last_errno();
			NETP_TRACE_IOE("[iocp][#%u]_do_accept_ex create fd failed: %d", olctx->fd, ec);
			return ec;
		}

		NETP_TRACE_IOE("[iocp][#%u]_do_accept_ex begin,new fd: %u", olctx->fd, olctx->accept_fd);

		const static LPFN_ACCEPTEX lpfnAcceptEx = (LPFN_ACCEPTEX)netp::os::load_api_ex_address(netp::os::API_ACCEPT_EX);
		NETP_ASSERT(lpfnAcceptEx != 0);
		BOOL acceptrt = lpfnAcceptEx(olctx->fd, olctx->accept_fd, olctx->wsabuf.buf, 0,
			sizeof(struct sockaddr_in) + 16, sizeof(struct sockaddr_in) + 16,
			nullptr, &(olctx->ol));

		if (!acceptrt)
		{
			ec = netp_socket_get_last_errno();
			if (ec == netp::E_WSA_IO_PENDING) {
				ec = netp::OK;
			} else {
				NETP_TRACE_SOCKET_OC("[iocp][#%u][_do_accept_ex][netp::close] acceptex failed: %d", olctx->fd, netp_socket_get_last_errno());
				netp::close(olctx->accept_fd);
			}
		}
		return ec;
	}

	void socket_channel_iocp::__iocp_do_AcceptEx_done(fn_channel_initializer_t const& fn_initializer, NRP<socket_cfg> const& cfg,  int status, io_ctx* ctx_) {
		NETP_ASSERT(L->in_event_loop());
		iocp_ctx* ctx = (iocp_ctx*)ctx_;
		NETP_ASSERT( (ctx->ol_r->action_status & AS_DONE) == 0);

		//NETP_ASSERT(m_fn_accept_initializer != nullptr);
		if (status == netp::OK) {
			const SOCKET& nfd = ctx->ol_r->accept_fd;
			NETP_ASSERT(nfd != NETP_INVALID_SOCKET);
			struct sockaddr_in* raddr_in = 0;
			struct sockaddr_in* laddr_in = 0;
			int raddr_in_len = sizeof(struct sockaddr_in);
			int laddr_in_len = sizeof(struct sockaddr_in);

			const static LPFN_GETACCEPTEXSOCKADDRS fn_getacceptexsockaddrs = (LPFN_GETACCEPTEXSOCKADDRS)netp::os::load_api_ex_address(netp::os::API_GET_ACCEPT_EX_SOCKADDRS);
			NETP_ASSERT(fn_getacceptexsockaddrs != 0);
			fn_getacceptexsockaddrs(ctx->ol_r->wsabuf.buf, 0,
				sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16,
				(LPSOCKADDR*)&laddr_in, &laddr_in_len,
				(LPSOCKADDR*)&raddr_in, &raddr_in_len);

			NRP<netp::address> raddr = netp::make_ref<netp::address>(raddr_in, sizeof(sockaddr_in));
			NRP<netp::address> laddr = netp::make_ref<netp::address>(laddr_in, sizeof(sockaddr_in));

			if ( *raddr == *laddr) {
				NETP_WARN("[socket][%s][do_accept_ex][netp::close]raddr == laddr, force close: %u", ch_info().c_str(), nfd);
				netp::close(nfd);
				return;
			}
			NETP_ASSERT(laddr->port() == m_laddr->port());
			NETP_ASSERT(raddr_in->sin_family == m_family);

			NRP<event_loop> LL = event_loop_group::instance()->next(L->poller_type());
			LL->execute([LL, fn_initializer, nfd, laddr, raddr, cfg]() {
				NRP<socket_cfg> cfg_ = netp::make_ref<socket_cfg>();
				cfg_->fd = nfd;
				cfg_->family = cfg->family;
				cfg_->type = cfg->type;
				cfg_->proto = cfg->proto;
				cfg_->laddr = laddr;
				cfg_->raddr = raddr;

				cfg_->L = LL;
				cfg_->option = cfg->option;
				cfg_->kvals = cfg->kvals;
				cfg_->sock_buf = cfg->sock_buf;
				cfg_->bdlimit = cfg->bdlimit;
				int rt;
				NRP<socket_channel> ch;
				std::tie(rt, ch) = create_socket_channel(cfg_);
				if (rt != netp::OK) {
					NETP_TRACE_SOCKET_OC("[socket][%s][do_accept_ex][netp::close] error: %d", ch_info().c_str(), status);
					netp::close(nfd);
					return;
				}
				ch->__do_accept_fire(fn_initializer);
			});
		}

		if (netp::E_EWOULDBLOCK==(status) || status == netp::OK) {
			status = __iocp_do_AcceptEx(ctx->ol_r);
			if (status == netp::OK) {
				ctx->ol_r->action_status |= AS_WAIT_IOCP;
				return;
			}
		}

		ch_errno() = status;
		m_chflag |= int(channel_flag::F_READ_ERROR);
		ch_close();
	}

	int socket_channel_iocp::__iocp_do_ConnectEx(void* ol_) {
		NETP_ASSERT(L->in_event_loop());

		//iocp need bind first, we've done it in socket_connect_impl

		NETP_ASSERT(m_laddr && !m_laddr->is_af_unspec());
		WSAOVERLAPPED* ol = (WSAOVERLAPPED*)ol_;
		NETP_ASSERT(!m_raddr->is_af_unspec());
		socklen_t socklen = sizeof(sockaddr_in);
		const static LPFN_CONNECTEX fn_connectEx = (LPFN_CONNECTEX)netp::os::load_api_ex_address(netp::os::API_CONNECT_EX);
		NETP_ASSERT(fn_connectEx != 0);
		int ec = netp::OK;
		const BOOL connrt = fn_connectEx(m_fd, (SOCKADDR*)(m_raddr->sockaddr_v4()), socklen, 0, 0, 0, ol);
		if (connrt == FALSE) {
			ec = netp_socket_get_last_errno();
			if (ec == netp::E_WSA_IO_PENDING) {
				NETP_VERBOSE("[socket][iocp][#%u]__connectEx E_WSA_IO_PENDING", m_fd, connrt);
				ec = netp::OK;
			}
		}
		NETP_VERBOSE("[socket][iocp][#%u]connectex ok", m_fd);
		return ec;
	}

	void socket_channel_iocp::__iocp_do_WSARecvfrom_done(int status, io_ctx* ctx_) {
		iocp_ctx* ctx = (iocp_ctx*)ctx_;
		if (status < 0) {
			NETP_TRACE_SOCKET("[socket][%s]WSARecvfrom error: %d", ch_info().c_str(), status);
			___do_io_read_done(status);
			return;
		}

		NETP_ASSERT(ULONG(status) <= ctx->ol_r->wsabuf.len);
		channel::ch_fire_readfrom(netp::make_ref<netp::packet>(ctx->ol_r->wsabuf.buf, status), netp::make_ref<address>( &(*(ctx->ol_r->from_ptr)), sizeof(sockaddr_in) ));
		status = netp::OK;
		__do_io_read_from(status, m_io_ctx);
		NETP_ASSERT((iocp_ctx*)m_io_ctx == ctx);
		if (m_chflag & int(channel_flag::F_WATCH_READ)) {
			status = __iocp_do_WSARecvfrom(ctx->ol_r, (SOCKADDR*)ctx->ol_r->from_ptr, ctx->ol_r->from_len_ptr);
			if (status == netp::OK) {
				ctx->ol_r->action_status |= AS_WAIT_IOCP;
			} else {
				___do_io_read_done(status);
			}
		}
	}

	int socket_channel_iocp::__iocp_do_WSARecvfrom(ol_ctx* olctx, SOCKADDR* from, int* fromlen) {
		NETP_ASSERT((olctx->action_status & AS_WAIT_IOCP) == 0);
		ol_ctx_reset(olctx);
		DWORD flags = 0;
		*fromlen = sizeof(sockaddr_in);
		//sockaddr_in from_;
		//int fromlen_ = sizeof(sockaddr_in);
		int ec = ::WSARecvFrom(olctx->fd, &olctx->wsabuf, 1, NULL, &flags, from, fromlen, &olctx->ol, NULL);

		if (ec == NETP_SOCKET_ERROR) {
			ec = netp_socket_get_last_errno();
			if (ec == netp::E_WSA_IO_PENDING) {
				ec = netp::OK;
			}
		}
		return ec;
	}

	void socket_channel_iocp::__iocp_do_WSARecv_done(int status, io_ctx* ctx_) {

		NETP_ASSERT(L->in_event_loop());
		NETP_ASSERT(!ch_is_listener());
		NETP_ASSERT(m_io_ctx == ctx_);
		iocp_ctx* ctx = (iocp_ctx*)ctx_;
		NETP_ASSERT(m_chflag & int(channel_flag::F_WATCH_READ));
		if (NETP_LIKELY(status) > 0) {
			NETP_ASSERT(ULONG(status) <= ctx->ol_r->wsabuf.len);
			channel::ch_fire_read(netp::make_ref<netp::packet>(ctx->ol_r->wsabuf.buf, status));
			status = netp::OK;
		}
		else if (status == 0) {
			status = netp::E_SOCKET_GRACE_CLOSE;
		} else {
			NETP_TRACE_SOCKET("[socket][%s]WSARecv error: %d", ch_info().c_str(), status);
		}
		__do_io_read(status, m_io_ctx);
		if (m_chflag & int(channel_flag::F_WATCH_READ)) {
			status = __iocp_do_WSARecv(ctx->ol_r);
			if (status == netp::OK) {
				ctx->ol_r->action_status |= AS_WAIT_IOCP;
			} else {
				___do_io_read_done(status);
			}
		}
	}

	int socket_channel_iocp::__iocp_do_WSARecv(ol_ctx* olctx) {
		NETP_ASSERT((olctx->action_status & AS_WAIT_IOCP) == 0);
		ol_ctx_reset(olctx);
		DWORD flags = 0;
		int ec = ::WSARecv(olctx->fd, &olctx->wsabuf, 1, NULL, &flags, &olctx->ol, NULL);
		if (ec == NETP_SOCKET_ERROR) {
			ec = netp_socket_get_last_errno();
			if (ec == netp::E_WSA_IO_PENDING) {
				ec = netp::OK;
			}
		}
		return ec;
	}

	void socket_channel_iocp::__iocp_do_WSASend_done(int status, io_ctx* ctx_) {
		NETP_ASSERT(L->in_event_loop());
		NETP_ASSERT(m_io_ctx == ctx_);
		NETP_ASSERT((m_chflag & int(channel_flag::F_WATCH_WRITE)) != 0);

		if (status < 0) {
			__do_io_write_done(status);
			return;
		}
		iocp_ctx* ctx = (iocp_ctx*)ctx_;
		NETP_ASSERT(m_tx_bytes > 0);
		socket_outbound_entry entry = m_tx_entry_q.front();
		NETP_ASSERT(entry.data != nullptr);
		m_tx_bytes -= status;
		entry.data->skip(status);
		if (entry.data->len() == 0) {
			entry.write_promise->set(netp::OK);
			m_tx_entry_q.pop_front();
		}
		status = netp::OK;
		if (m_tx_bytes > 0) {
			status = __iocp_do_WSASend(ctx->ol_w);
			if (status == netp::OK) {
				ctx->ol_w->action_status |= AS_WAIT_IOCP;
				return;
			}
		}
		__do_io_write_done(status);
	}

	//one shot one packet
	int socket_channel_iocp::__iocp_do_WSASend(ol_ctx* olctx) {
		NETP_ASSERT(L->in_event_loop());
		NETP_ASSERT(olctx != nullptr);
		NETP_ASSERT((olctx->action_status & (AS_WAIT_IOCP)) == 0);

		NETP_ASSERT(m_tx_bytes > 0);
		socket_outbound_entry& entry = m_tx_entry_q.front();
		olctx->wsabuf = { ULONG(entry.data->len()), (char*)entry.data->head() };
		ol_ctx_reset(olctx);
		int rt = ::WSASend(m_fd, &olctx->wsabuf, 1, NULL, 0, &olctx->ol, NULL);
		if (rt == NETP_SOCKET_ERROR) {
			rt = netp_socket_get_last_errno();
			if (rt == netp::E_WSA_IO_PENDING) {
				rt = netp::OK;
			}
		}
		//https://docs.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsasend
		//ACCORDING TO MSDN: nonblocking should not return with this value
		//NETP_ASSERT(wrt == netp::E_WSAEINTR); 
		return rt;
	}

} //end of ns

#endif //NETP_HAS_POLLER_IOCP