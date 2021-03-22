#ifndef _NETP_SOCKET_CH_IOCP_HPP_
#define _NETP_SOCKET_CH_IOCP_HPP_

#include <queue>
#include <netp/socket.hpp>

#ifdef NETP_HAS_POLLER_IOCP
#include <netp/poller_iocp.hpp>

#ifdef NETP_DEFAULT_LISTEN_BACKLOG
	#undef NETP_DEFAULT_LISTEN_BACKLOG
#endif
#define NETP_DEFAULT_LISTEN_BACKLOG SOMAXCONN


namespace netp {

	class socket_iocp final :
		public socket
	{
		u32_t m_wsabuf_size;

	public:
		socket_iocp(NRP<socket_cfg> const& cfg) :
			socket(cfg),
			m_wsabuf_size(cfg->wsabuf_size)
		{
			NETP_ASSERT(cfg->L != nullptr);
		}

		~socket_iocp()
		{
		}

	private:
		void __aio_begin_done(aio_ctx* ctx_) {
			m_chflag |= int(channel_flag::F_IO_EVENT_LOOP_BEGIN_DONE);
#ifdef NETP_HAS_POLLER_IOCP
			iocp_ctx* ctx = (iocp_ctx*)ctx_;
			if (L->poller_type() == T_IOCP) {
				NETP_ASSERT(ctx->ol_r->rcvbuf == 0);
				const int from_sockaddr_in_reserve = sizeof(struct sockaddr_in) + 16;
				const int from_reserve = is_udp() ? (from_sockaddr_in_reserve + sizeof(int)) : 0;
				ctx->ol_r->rcvbuf = netp::allocator<char>::malloc(from_reserve + m_wsabuf_size);
				NETP_ALLOC_CHECK(ctx->ol_r->rcvbuf, from_reserve + m_wsabuf_size);

				ctx->ol_r->wsabuf = { m_wsabuf_size, (ctx->ol_r->rcvbuf + from_reserve) };
				if (is_udp()) {
					ctx->ol_r->from_ptr = (struct sockaddr_in*)ctx->ol_r->rcvbuf;
					ctx->ol_r->from_len_ptr = (int*)(ctx->ol_r->rcvbuf + from_sockaddr_in_reserve);
				}
			}
#endif
		}
	private:
		void _do_aio_accept(fn_channel_initializer_t const& fn_accepted_initializer, NRP<socket_cfg> const& ccfg) {
			NETP_ASSERT(L->in_event_loop());

			if (m_chflag & int(channel_flag::F_WATCH_READ)) {
				NETP_TRACE_SOCKET("[socket][%s][_do_aio_accept]F_WATCH_READ state already", info().c_str());
				return;
			}

			if (m_chflag & int(channel_flag::F_READ_SHUTDOWN)) {
				NETP_TRACE_SOCKET("[socket][%s][_do_aio_accept]cancel for rd closed already", info().c_str());
				return;
			}

			NETP_TRACE_SOCKET("[socket][%s][_do_aio_accept]watch AIO_READ", info().c_str());

#ifdef NETP_HAS_POLLER_IOCP
			NETP_ASSERT(L->type() == T_IOCP);
			int rt = __iocp_do_AcceptEx(m_aio_ctx->ol_r);
			if (rt != netp::OK) {
				ch_errno() = rt;
				ch_close();
				return;
			}

			m_chflag |= int(channel_flag::F_WATCH_READ);
			rt = L->aio_do(aio_action::READ, m_aio_ctx);
			NETP_ASSERT(rt == netp::OK);
			m_aio_ctx->ol_r->action = iocp_ol_action::ACCEPTEX;
			m_aio_ctx->ol_r->action_status |= AS_WAIT_IOCP;
			m_aio_ctx->ol_r->fn_ol_done = std::bind(&socket::__iocp_do_AcceptEx_done, NRP<socket>(this), fn_accepted_initializer, ccfg, std::placeholders::_1, std::placeholders::_2);
#else
			//@TODO: provide custome accept feature
			//const fn_aio_event_t _fn = cb_accepted == nullptr ? std::bind(&socket::__cb_async_accept_impl, NRP<socket>(this), std::placeholders::_1) : cb_accepted;
			int rt = L->aio_do(aio_action::READ, m_aio_ctx);
			if (rt == netp::OK) {
				m_chflag |= int(channel_flag::F_WATCH_READ);
				m_aio_ctx->fn_read = std::bind(&socket::__cb_aio_accept_impl, NRP<socket>(this), fn_accepted_initializer, ccfg, std::placeholders::_1, std::placeholders::_2);
			}
#endif
		}

		inline void _do_aio_end_accept() {
#ifdef NETP_HAS_POLLER_IOCP
			if (m_aio_ctx->ol_r->accept_fd != NETP_INVALID_SOCKET) {
				NETP_CLOSE_SOCKET(m_aio_ctx->ol_r->accept_fd);
				m_aio_ctx->ol_r->accept_fd = NETP_INVALID_SOCKET;
			}
#endif
			NETP_ASSERT(L->in_event_loop());
			ch_aio_end_read();
		}

#ifdef NETP_HAS_POLLER_IOCP

		inline int __iocp_do_AcceptEx(ol_ctx* olctx) {
			NETP_ASSERT(olctx != nullptr);
			olctx->accept_fd = netp::open(*m_api, m_family, m_type, m_protocol);
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
				}
				else {
					NETP_TRACE_IOE("[iocp][#%u]_do_accept_ex acceptex failed: %d", olctx->fd, netp_socket_get_last_errno());
					NETP_CLOSE_SOCKET(olctx->accept_fd);
				}
			}
			return ec;
		}

		void __iocp_do_AcceptEx_done(fn_channel_initializer_t const& fn_accepted_initializer, NRP<socket_cfg> const& ccfg, int status, aio_ctx* ctx) {
			NETP_ASSERT(L->in_event_loop());
			NETP_ASSERT((ctx->ol_r->action_status & AS_DONE) == 0);

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

				const address raddr(*raddr_in);
				const address laddr(*laddr_in);

				if (raddr == laddr) {
					NETP_WARN("[socket][%s][accept]raddr == laddr, force close: %u", info().c_str(), nfd);
					NETP_CLOSE_SOCKET(nfd);
				}
				else {
					NETP_ASSERT(laddr.port() == m_laddr.port());
					NETP_ASSERT(raddr_in->sin_family == m_family);

					try {
						__do_create_accepted_socket(nfd, laddr, raddr, fn_accepted_initializer, ccfg);
					}
					catch (netp::exception& e) {
						NETP_ERR("[#%d]accept new fd exception: [%d]%s\n%s(%d) %s\n%s", m_fd,
							e.code(), e.what(), e.file(), e.line(), e.function(), e.callstack());
						NETP_CLOSE_SOCKET(nfd);
					}
					catch (std::exception& e) {
						NETP_ERR("[#%d]accept new fd exception, e: %s", m_fd, e.what());
						NETP_CLOSE_SOCKET(nfd);
					}
					catch (...) {
						NETP_ERR("[#%d]accept new fd exception, e: %d", m_fd, netp_socket_get_last_errno());
						NETP_CLOSE_SOCKET(nfd);
					}
				}
			}

			if (IS_ERRNO_EQUAL_WOULDBLOCK(status) || status == netp::OK) {
				status = __iocp_do_AcceptEx(ctx->ol_r);
				if (status == netp::OK) {
					ctx->ol_r->action_status |= AS_WAIT_IOCP;
					return;
				}
			}

			ch_errno() = status;
			ch_close();
		}

		int __iocp_do_ConnectEx(void* ol_) {
			NETP_ASSERT(L->in_event_loop());

			WSAOVERLAPPED* ol = (WSAOVERLAPPED*)ol_;
			NETP_ASSERT(!m_raddr.is_null());

			sockaddr_in addr;
			::memset(&addr, 0, sizeof(addr));

			addr.sin_family = m_family;
			addr.sin_port = m_raddr.nport();
			addr.sin_addr.s_addr = m_raddr.nipv4();
			socklen_t socklen = sizeof(addr);
			const static LPFN_CONNECTEX fn_connectEx = (LPFN_CONNECTEX)netp::os::load_api_ex_address(netp::os::API_CONNECT_EX);
			NETP_ASSERT(fn_connectEx != 0);
			int ec = netp::OK;
			const BOOL connrt = fn_connectEx(m_fd, (SOCKADDR*)(&addr), socklen, 0, 0, 0, ol);
			if (connrt == FALSE) {
				ec = netp_socket_get_last_errno();
				if (ec == netp::E_WSA_IO_PENDING) {
					NETP_DEBUG("[socket][iocp][#%u]socket __connectEx E_WSA_IO_PENDING", m_fd, connrt);
					ec = netp::OK;
				}
			}
			NETP_DEBUG("[socket][iocp][#%u]connectex ok", m_fd);
			return ec;
		}

		void __iocp_do_WSARecvfrom_done(int status, aio_ctx* ctx) {
			if (status > 0) {
				NETP_ASSERT(ULONG(status) <= ctx->ol_r->wsabuf.len);
				channel::ch_fire_readfrom(netp::make_ref<netp::packet>(ctx->ol_r->wsabuf.buf, status), address(*(ctx->ol_r->from_ptr)));
				status = netp::OK;
				__cb_aio_read_from_impl(status, m_aio_ctx);
			}
			else {
				NETP_TRACE_SOCKET("[socket][%s]WSARecvfrom error: %d", info().c_str(), len);
			}
			if (m_chflag & int(channel_flag::F_WATCH_READ)) {
				status = __iocp_do_WSARecvfrom(m_aio_ctx->ol_r, (SOCKADDR*)ctx->ol_r->from_ptr, ctx->ol_r->from_len_ptr);
				if (status == netp::OK) {
					m_aio_ctx->ol_r->action_status |= AS_WAIT_IOCP;
				}
				else {
					___aio_read_impl_done(status);
				}
			}
		}

		void __iocp_do_WSARecv_done(int status, aio_ctx* ctx) {
			NETP_ASSERT(L->in_event_loop());
			NETP_ASSERT(!ch_is_listener());
			NETP_ASSERT(m_chflag & int(channel_flag::F_WATCH_READ));
			if (NETP_LIKELY(status) > 0) {
				NETP_ASSERT(ULONG(status) <= ctx->ol_r->wsabuf.len);
				channel::ch_fire_read(netp::make_ref<netp::packet>(ctx->ol_r->wsabuf.buf, status));
				status = netp::OK;
			}
			else if (status == 0) {
				status = netp::E_SOCKET_GRACE_CLOSE;
			}
			else {
				NETP_TRACE_SOCKET("[socket][%s]WSARecv error: %d", info().c_str(), len);
			}
			__cb_aio_read_impl(status, m_aio_ctx);
			if (m_chflag & int(channel_flag::F_WATCH_READ)) {
				status = __iocp_do_WSARecv(m_aio_ctx->ol_r);
				if (status == netp::OK) {
					m_aio_ctx->ol_r->action_status |= AS_WAIT_IOCP;
				}
				else {
					___aio_read_impl_done(status);
				}
			}
		}

		void __iocp_do_WSASend_done(int status, aio_ctx*) {
			NETP_ASSERT(L->in_event_loop());
			NETP_ASSERT((m_chflag & int(channel_flag::F_WATCH_WRITE)) != 0);
			if (status < 0) {
				socket::__handle_aio_write_impl_done(status);
				return;
			}

			NETP_ASSERT(m_noutbound_bytes > 0);
			socket_outbound_entry entry = m_outbound_entry_q.front();
			NETP_ASSERT(entry.data != nullptr);
			m_noutbound_bytes -= status;
			entry.data->skip(status);
			if (entry.data->len() == 0) {
				entry.write_promise->set(netp::OK);
				m_outbound_entry_q.pop_front();
			}
			status = netp::OK;
			if (m_noutbound_bytes > 0) {
				status = __iocp_do_WSASend(m_aio_ctx->ol_w);
				if (status == netp::OK) {
					m_aio_ctx->ol_w->action_status |= AS_WAIT_IOCP;
					return;
				}
			}
			socket::__handle_aio_write_impl_done(status);
		}

		//one shot one packet
		int __iocp_do_WSASend(ol_ctx* olctx) {
			NETP_ASSERT(L->in_event_loop());
			NETP_ASSERT(olctx != nullptr);
			NETP_ASSERT((olctx->action_status & (AS_WAIT_IOCP)) == 0);

			NETP_ASSERT(m_noutbound_bytes > 0);
			socket_outbound_entry& entry = m_outbound_entry_q.front();
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

		inline static int __iocp_do_WSARecv(ol_ctx* olctx) {
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
		inline static int __iocp_do_WSARecvfrom(ol_ctx* olctx, SOCKADDR* from, int* fromlen) {
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
#endif
	public:
		__NETP_FORCE_INLINE channel_id_t ch_id() const override { return m_fd; }
		std::string ch_info() const override { return info(); }

		void ch_set_bdlimit(u32_t limit) override {
			L->execute([s = NRP<socket>(this), limit]() {
				s->m_outbound_limit = limit;
				s->m_outbound_budget = s->m_outbound_limit;
			});
		};

		void ch_write_impl(NRP<packet> const& outlet, NRP<promise<int>> const& chp) override;
		void ch_write_to_impl(NRP<packet> const& outlet, netp::address const& to, NRP<promise<int>> const& chp) override;

		void ch_close_read_impl(NRP<promise<int>> const& closep) override
		{
			NETP_ASSERT(L->in_event_loop());
			NETP_TRACE_SOCKET("[socket][%s]ch_close_read_impl, _ch_do_close_read, errno: %d, flag: %d", info().c_str(), ch_errno(), m_chflag);
			int prt = netp::OK;
			if (m_chflag & (int(channel_flag::F_READ_SHUTDOWNING) | int(channel_flag::F_CLOSE_PENDING) | int(channel_flag::F_CLOSING))) {
				prt = (netp::E_OP_INPROCESS);
			}
			else if ((m_chflag & int(channel_flag::F_READ_SHUTDOWN)) != 0) {
				prt = (netp::E_CHANNEL_WRITE_CLOSED);
			}
			else {
				_ch_do_close_read();
			}

			if (closep) { closep->set(prt); }
		}

		void ch_close_write_impl(NRP<promise<int>> const& chp) override;
		void ch_close_impl(NRP<promise<int>> const& chp) override;

		void ch_aio_read(fn_aio_event_t const& fn_read = nullptr) {
			if (!L->in_event_loop()) {
				L->schedule([s = NRP<socket>(this), fn_read]()->void {
					s->ch_aio_read(fn_read);
				});
				return;
			}
			NETP_ASSERT((m_chflag & int(channel_flag::F_READ_SHUTDOWNING)) == 0);
			if (m_chflag & int(channel_flag::F_WATCH_READ)) {
				NETP_TRACE_SOCKET("[socket][%s]aio_action::READ, ignore, flag: %d", info().c_str(), m_chflag);
				return;
			}

			if (m_chflag & int(channel_flag::F_READ_SHUTDOWN)) {
				NETP_ASSERT((m_chflag & int(channel_flag::F_WATCH_READ)) == 0);
				if (fn_read != nullptr) {
					fn_read(netp::E_CHANNEL_READ_CLOSED, nullptr);
				}
				return;
			}

#ifdef NETP_HAS_POLLER_IOCP
			NETP_ASSERT(L->type() == T_IOCP);
			if (m_aio_ctx->ol_r->accept_fd != NETP_INVALID_SOCKET) {
				m_chflag |= int(channel_flag::F_WATCH_READ);
				L->schedule([fn_read, so = NRP<socket>(this)]() {
					if (so->m_chflag & int(channel_flag::F_WATCH_READ)) {
						so->m_aio_ctx->ol_r->action = iocp_ol_action::WSAREAD;
						int rt = so->L->aio_do(aio_action::READ, so->m_aio_ctx);
						NETP_ASSERT(rt == netp::OK);
						int status = int(so->m_aio_ctx->ol_r->accept_fd);
						so->m_aio_ctx->ol_r->accept_fd = NETP_INVALID_SOCKET;
						if (fn_read != nullptr) {
							fn_read(status, so->m_aio_ctx);
						}
						else {
							so->__iocp_do_WSARecv_done(status, so->m_aio_ctx);
						}
					}
					else {
						fn_read(netp::E_CHANNEL_ABORT, nullptr);
					}
				});
				//schedule for the last read
				return;
			}

			int rt = is_tcp() ? __iocp_do_WSARecv(m_aio_ctx->ol_r) :
				__iocp_do_WSARecvfrom(m_aio_ctx->ol_r, (SOCKADDR*)m_aio_ctx->ol_r->from_ptr, m_aio_ctx->ol_r->from_len_ptr);
			if (rt != netp::OK) {
				if (fn_read != nullptr) { fn_read(rt, 0); }
				return;
			}

			rt = L->aio_do(aio_action::READ, m_aio_ctx);
			NETP_ASSERT(rt == netp::OK);

			m_chflag |= int(channel_flag::F_WATCH_READ);
			m_aio_ctx->ol_r->action = iocp_ol_action::WSAREAD;
			m_aio_ctx->ol_r->action_status |= AS_WAIT_IOCP;
			m_aio_ctx->ol_r->fn_ol_done = fn_read != nullptr ? fn_read :
				is_tcp() ? std::bind(&socket::__iocp_do_WSARecv_done, NRP<socket>(this), std::placeholders::_1, std::placeholders::_2) :
				std::bind(&socket::__iocp_do_WSARecvfrom_done, NRP<socket>(this), std::placeholders::_1, std::placeholders::_2);
#else
			int rt = L->aio_do(aio_action::READ, m_aio_ctx);
			if (rt == netp::OK) {
				m_chflag |= int(channel_flag::F_WATCH_READ);
				m_aio_ctx->fn_read = fn_read != nullptr ? fn_read :
					is_udp() ? std::bind(&socket::__cb_aio_read_from_impl, NRP<socket>(this), std::placeholders::_1, std::placeholders::_2) : 
					std::bind(&socket::__cb_aio_read_impl, NRP<socket>(this), std::placeholders::_1, std::placeholders::_2);
			}
#endif

			NETP_TRACE_IOE("[socket][%s]aio_action::READ", info().c_str());
		}

		void ch_aio_end_read() {
			if (!L->in_event_loop()) {
				L->schedule([_so = NRP<socket>(this)]()->void {
					_so->ch_aio_end_read();
				});
				return;
			}

			if ((m_chflag & int(channel_flag::F_WATCH_READ))) {
				m_chflag &= ~int(channel_flag::F_WATCH_READ);
				L->aio_do(aio_action::END_READ, m_aio_ctx);

#ifdef NETP_HAS_POLLER_IOCP
				m_aio_ctx->ol_r->fn_ol_done = nullptr;
#else
				m_aio_ctx->fn_read = nullptr;
#endif
				NETP_TRACE_IOE("[socket][%s]aio_action::END_READ", info().c_str());
			}
		}

		void ch_aio_write(fn_aio_event_t const& fn_write = nullptr) override {
			if (!L->in_event_loop()) {
				L->schedule([_so = NRP<socket>(this), fn_write]()->void {
					_so->ch_aio_write(fn_write);
				});
				return;
			}

			if (m_chflag & int(channel_flag::F_WATCH_WRITE)) {
				NETP_ASSERT(m_chflag & int(channel_flag::F_CONNECTED));
				if (fn_write != nullptr) {
					fn_write(netp::E_SOCKET_OP_ALREADY, 0);
				}
				return;
			}

			if (m_chflag & int(channel_flag::F_WRITE_SHUTDOWN)) {
				NETP_ASSERT((m_chflag & int(channel_flag::F_WATCH_WRITE)) == 0);
				NETP_TRACE_SOCKET("[socket][%s]aio_action::WRITE, cancel for wr closed already", info().c_str());
				if (fn_write != nullptr) {
					fn_write(netp::E_CHANNEL_WRITE_CLOSED, 0);
				}
				return;
			}

#ifdef NETP_HAS_POLLER_IOCP
			NETP_ASSERT(L->type() == T_IOCP);
			if (m_noutbound_bytes == 0) {
				if (fn_write != nullptr) {
					fn_write(netp::E_CHANNEL_OUTGO_LIST_EMPTY, 0);
				}
				return;
			}
			int rt = __iocp_do_WSASend(m_aio_ctx->ol_w);
			if (rt != netp::OK) {
				if (fn_write != nullptr) { fn_write(rt, m_aio_ctx); };
				return;
			}

			m_chflag |= int(channel_flag::F_WATCH_WRITE);
			rt = L->aio_do(aio_action::WRITE, m_aio_ctx);
			NETP_ASSERT(rt == netp::OK);
			m_aio_ctx->ol_w->action = iocp_ol_action::WSASEND;
			m_aio_ctx->ol_w->action_status |= AS_WAIT_IOCP;
			m_aio_ctx->ol_w->fn_ol_done = fn_write != nullptr ? fn_write :
				std::bind(&socket::__iocp_do_WSASend_done, NRP<socket>(this), std::placeholders::_1, std::placeholders::_2);
#else
			int rt = L->aio_do(aio_action::WRITE, m_aio_ctx);
			if (rt == netp::OK) {
				m_chflag |= int(channel_flag::F_WATCH_WRITE);
				m_aio_ctx->fn_write = fn_write != nullptr ? fn_write :
					std::bind(&socket::__cb_aio_write_impl, NRP<socket>(this), std::placeholders::_1, std::placeholders::_2);
			}
#endif
		}

		void ch_aio_end_write() override {
			if (!L->in_event_loop()) {
				L->schedule([_so = NRP<socket>(this)]()->void {
					_so->ch_aio_end_write();
				});
				return;
			}

			if (m_chflag & int(channel_flag::F_WATCH_WRITE)) {
				m_chflag &= ~int(channel_flag::F_WATCH_WRITE);

				L->aio_do(aio_action::END_WRITE, m_aio_ctx);
#ifdef NETP_HAS_POLLER_IOCP
				NETP_ASSERT(L->type() == T_IOCP);
				m_aio_ctx->ol_w->fn_ol_done = nullptr;
#else
				m_aio_ctx->fn_write = nullptr;
#endif
				NETP_TRACE_IOE("[socket][%s]aio_action::END_WRITE", info().c_str());
			}
		}

		void ch_aio_connect(fn_aio_event_t const& fn = nullptr) override {
			NETP_ASSERT(fn != nullptr);
			if (m_chflag & int(channel_flag::F_WATCH_WRITE)) {
				return;
			}
#ifdef NETP_HAS_POLLER_IOCP
			NETP_ASSERT(L->type() == T_IOCP);
			int rt = __iocp_do_ConnectEx(m_aio_ctx->ol_w);
			if (rt != netp::OK) {
				fn(rt, m_aio_ctx);
				return;
			}
			rt = L->aio_do(aio_action::WRITE, m_aio_ctx);
			NETP_ASSERT(rt == netp::OK);
			m_chflag |= int(channel_flag::F_WATCH_WRITE);
			m_aio_ctx->ol_w->action = iocp_ol_action::CONNECTEX;
			m_aio_ctx->ol_w->action_status |= AS_WAIT_IOCP;
			m_aio_ctx->ol_w->fn_ol_done = fn;
#else
			ch_aio_write(fn);
#endif
		}

		void ch_aio_end_connect() override {
			NETP_ASSERT(!ch_is_passive());
#ifdef NETP_HAS_POLLER_IOCP
			NETP_ASSERT(L->type() == T_IOCP);
			if (m_chflag & int(channel_flag::F_WATCH_WRITE)) {
				m_chflag &= ~int(channel_flag::F_WATCH_WRITE);
				L->aio_do(aio_action::END_WRITE, m_aio_ctx);
				m_aio_ctx->ol_w->fn_ol_done = nullptr;
			}
#else
			ch_aio_end_write();
#endif
		}
	};
}

#endif //IOCP
#endif