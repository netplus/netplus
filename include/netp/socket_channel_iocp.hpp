#ifndef _NETP_SOCKET_CH_IOCP_HPP_
#define _NETP_SOCKET_CH_IOCP_HPP_

#include <queue>
#include <netp/socket_channel.hpp>

#ifdef NETP_HAS_POLLER_IOCP
#include <netp/poller_iocp.hpp>

#ifdef NETP_DEFAULT_LISTEN_BACKLOG
	#undef NETP_DEFAULT_LISTEN_BACKLOG
#endif
#define NETP_DEFAULT_LISTEN_BACKLOG SOMAXCONN

namespace netp {
	class socket_channel_iocp final :
		public socket_channel
	{
		u32_t m_wsabuf_size;

	public:
		socket_channel_iocp(NRP<socket_cfg> const& cfg) :
			socket_channel(cfg),
			m_wsabuf_size(cfg->wsabuf_size)
		{
			NETP_ASSERT(cfg->L != nullptr);
		}

		~socket_channel_iocp()
		{
		}

	private:
		int __iocp_do_AcceptEx(ol_ctx* olctx);
		void __iocp_do_AcceptEx_done(fn_channel_initializer_t const& fn_initializer, int status, aio_ctx* ctx);
		int __iocp_do_ConnectEx(void* ol_);
		void __iocp_do_WSARecvfrom_done(int status, aio_ctx* ctx);
		void __iocp_do_WSARecv_done(int status, aio_ctx* ctx);
		void __iocp_do_WSASend_done(int status, aio_ctx* ctx);

		//one shot one packet
		int __iocp_do_WSASend(ol_ctx* olctx);
		int __iocp_do_WSARecv(ol_ctx* olctx);
		int __iocp_do_WSARecvfrom(ol_ctx* olctx, SOCKADDR* from, int* fromlen);

		void __aio_begin_done(aio_ctx* ctx_) override {
			socket_channel::__aio_begin_done(ctx_);
			iocp_ctx* ctx = (iocp_ctx*)ctx_;
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

	public:
		void ch_aio_begin(fn_aio_event_t const& fn_begin_done) override {
			NETP_ASSERT(is_nonblocking());

			if (!L->in_event_loop()) {
				L->schedule([s = NRP<socket_channel>(this), fn_begin_done]() {
					s->ch_aio_begin(fn_begin_done);
				});
				return;
			}

			NETP_ASSERT((m_chflag & (int(channel_flag::F_IO_EVENT_LOOP_BEGIN_DONE))) == 0);
			
			m_aio_ctx = L->aio_begin(m_fd);
			if (m_aio_ctx == 0) {
				ch_errno() = netp::E_AIO_BEGIN_FAILED;
				m_chflag |= int(channel_flag::F_READ_ERROR);//for assert check
				ch_close(nullptr);
				fn_begin_done(netp::E_AIO_BEGIN_FAILED, 0);
				return;
			}
			((iocp_ctx*)m_aio_ctx)->fn_notify = std::bind(&socket_channel_iocp::__aio_notify_terminating, NRP<socket_channel_iocp>(this), std::placeholders::_1, std::placeholders::_2);
			__aio_begin_done(m_aio_ctx);
			fn_begin_done(netp::OK, m_aio_ctx);
		}

		void __ch_clean() override {
			ch_deinit();
			if (m_chflag & int(channel_flag::F_IO_EVENT_LOOP_BEGIN_DONE)) {
				((iocp_ctx*)m_aio_ctx)->fn_notify = nullptr;
				L->aio_end(m_aio_ctx);
			}
		}

		/*
		void ch_aio_end() override {
			NETP_ASSERT(L->in_event_loop());
			NETP_ASSERT(m_outbound_entry_q.size() == 0);
			NETP_ASSERT(m_noutbound_bytes == 0);
			NETP_ASSERT(m_chflag & int(channel_flag::F_CLOSED));
			NETP_ASSERT((m_chflag & (int(channel_flag::F_WATCH_READ) | int(channel_flag::F_WATCH_WRITE))) == 0);
			NETP_TRACE_SOCKET("[socket][%s]aio_action::END, flag: %d", ch_info().c_str(), m_chflag);

			///NOTE ON WINDOWS&IOCP
			//Any pending overlapped sendand receive operations(WSASend / WSASendTo / WSARecv / WSARecvFrom with an overlapped socket) issued by any thread in this process are also canceled.Any event, completion routine, or completion port action specified for these overlapped operations is performed.The pending overlapped operations fail with the error status WSA_OPERATION_ABORTED.
			//Refer to: https://docs.microsoft.com/en-us/windows/win32/api/winsock/nf-winsock-closesocket
			ch_fire_closed(close());
			if (m_chflag & int(channel_flag::F_IO_EVENT_LOOP_BEGIN_DONE)) {
				//delay one tick to hold this and iocp_ctx*
				L->schedule([so = NRP<socket_channel_iocp>(this)]() {
					((iocp_ctx*)so->m_aio_ctx)->fn_notify = nullptr;
					so->L->aio_end(so->m_aio_ctx);
				});
			}
		}
	*/

		void ch_aio_accept(fn_channel_initializer_t const& fn_accepted_initializer) override {
			NETP_ASSERT(L->in_event_loop());
			if (m_chflag & int(channel_flag::F_WATCH_READ)) {
				NETP_TRACE_SOCKET("[socket][%s][_do_aio_accept]F_WATCH_READ state already", ch_info().c_str());
				return;
			}
			if (m_chflag & int(channel_flag::F_READ_SHUTDOWN)) {
				NETP_TRACE_SOCKET("[socket][%s][_do_aio_accept]cancel for rd closed already", ch_info().c_str());
				return;
			}

			NETP_TRACE_SOCKET("[socket][%s][_do_aio_accept]watch AIO_READ", ch_info().c_str());
			NETP_ASSERT(L->poller_type() == T_IOCP);
			iocp_ctx* ctx = (iocp_ctx*)m_aio_ctx;
			int rt = __iocp_do_AcceptEx(ctx->ol_r);
			if (rt != netp::OK) {
				ch_errno() = rt;
				m_chflag |= int(channel_flag::F_READ_ERROR);
				ch_close();
				return;
			}

			m_chflag |= int(channel_flag::F_WATCH_READ);
			rt = L->aio_do(aio_action::READ, m_aio_ctx);
			NETP_ASSERT(rt == netp::OK);
			ctx->ol_r->action = iocp_ol_action::ACCEPTEX;
			ctx->ol_r->action_status |= AS_WAIT_IOCP;
			ctx->ol_r->fn_ol_done = std::bind(&socket_channel_iocp::__iocp_do_AcceptEx_done, NRP<socket_channel_iocp>(this), fn_accepted_initializer, std::placeholders::_1, std::placeholders::_2);
		}

		void ch_aio_end_accept() override {
			NETP_ASSERT(L->in_event_loop());
			iocp_ctx* ctx = (iocp_ctx*)m_aio_ctx;

			if (ctx->ol_r->accept_fd != NETP_INVALID_SOCKET) {
				NETP_CLOSE_SOCKET(ctx->ol_r->accept_fd);
				ctx->ol_r->accept_fd = NETP_INVALID_SOCKET;
			}
			ch_aio_end_read();
		}


		void ch_aio_read(fn_aio_event_t const& fn_read = nullptr) {
			if (!L->in_event_loop()) {
				L->schedule([s = NRP<socket_channel>(this), fn_read]()->void {
					s->ch_aio_read(fn_read);
				});
				return;
			}
			NETP_ASSERT((m_chflag & int(channel_flag::F_READ_SHUTDOWNING)) == 0);
			if (m_chflag & int(channel_flag::F_WATCH_READ)) {
				NETP_TRACE_SOCKET("[socket][%s]aio_action::READ, ignore, flag: %d", ch_info().c_str(), m_chflag);
				return;
			}

			if (m_chflag & int(channel_flag::F_READ_SHUTDOWN)) {
				NETP_ASSERT((m_chflag & int(channel_flag::F_WATCH_READ)) == 0);
				if (fn_read != nullptr) {
					fn_read(netp::E_CHANNEL_READ_CLOSED, nullptr);
				}
				return;
			}

			iocp_ctx* ctx = (iocp_ctx*)m_aio_ctx;
			if (ctx->ol_r->accept_fd != NETP_INVALID_SOCKET) {
				m_chflag |= int(channel_flag::F_WATCH_READ);
				L->schedule([fn_read, so = NRP<socket_channel_iocp>(this), ctx]() {
					if (so->m_chflag & int(channel_flag::F_WATCH_READ)) {
						ctx->ol_r->action = iocp_ol_action::WSAREAD;
						int rt = so->L->aio_do(aio_action::READ, so->m_aio_ctx);
						NETP_ASSERT(rt == netp::OK);
						int status = int(ctx->ol_r->accept_fd);
						ctx->ol_r->accept_fd = NETP_INVALID_SOCKET;
						if (fn_read != nullptr) {
							fn_read(status, so->m_aio_ctx);
						} else {
							so->__iocp_do_WSARecv_done(status, (aio_ctx*)ctx);
						}
					}
					else {
						fn_read(netp::E_CHANNEL_ABORT, nullptr);
					}
				});
				//schedule for the last read
				return;
			}

			int rt = is_udp() ? __iocp_do_WSARecvfrom(ctx->ol_r, (SOCKADDR*)ctx->ol_r->from_ptr, ctx->ol_r->from_len_ptr) :
				__iocp_do_WSARecv(ctx->ol_r);

			if (rt != netp::OK) {
				if (fn_read != nullptr) { fn_read(rt, 0); }
				return;
			}

			rt = L->aio_do(aio_action::READ, m_aio_ctx);
			NETP_ASSERT(rt == netp::OK);

			m_chflag |= int(channel_flag::F_WATCH_READ);
			ctx->ol_r->action = iocp_ol_action::WSAREAD;
			ctx->ol_r->action_status |= AS_WAIT_IOCP;
			ctx->ol_r->fn_ol_done = fn_read != nullptr ? fn_read : 
				is_udp() ? std::bind(&socket_channel_iocp::__iocp_do_WSARecvfrom_done, NRP<socket_channel_iocp>(this), std::placeholders::_1, std::placeholders::_2) :
				std::bind(&socket_channel_iocp::__iocp_do_WSARecv_done, NRP<socket_channel_iocp>(this), std::placeholders::_1, std::placeholders::_2);

			NETP_TRACE_IOE("[socket][%s]aio_action::READ", ch_info().c_str());
		}

		void ch_aio_end_read() {
			if (!L->in_event_loop()) {
				L->schedule([_so = NRP<socket_channel_iocp>(this)]()->void {
					_so->ch_aio_end_read();
				});
				return;
			}

			if ((m_chflag & int(channel_flag::F_WATCH_READ))) {
				m_chflag &= ~int(channel_flag::F_WATCH_READ);
				L->aio_do(aio_action::END_READ, m_aio_ctx);
				((iocp_ctx*)m_aio_ctx)->ol_r->fn_ol_done = nullptr;
				NETP_TRACE_IOE("[socket][%s]aio_action::END_READ", ch_info().c_str());
			}
		}

		void ch_aio_write(fn_aio_event_t const& fn_write = nullptr) override {
			if (!L->in_event_loop()) {
				L->schedule([_so = NRP<socket_channel_iocp>(this), fn_write]()->void {
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
				NETP_TRACE_SOCKET("[socket][%s]aio_action::WRITE, cancel for wr closed already", ch_info().c_str());
				if (fn_write != nullptr) {
					fn_write(netp::E_CHANNEL_WRITE_CLOSED, 0);
				}
				return;
			}

			if (m_noutbound_bytes == 0) {
				if (fn_write != nullptr) {
					fn_write(netp::E_CHANNEL_OUTGO_LIST_EMPTY, 0);
				}
				return;
			}
			iocp_ctx* ctx = (iocp_ctx*)m_aio_ctx;
			int rt = __iocp_do_WSASend(ctx->ol_w);
			if (rt != netp::OK) {
				if (fn_write != nullptr) { fn_write(rt, m_aio_ctx); };
				return;
			}

			m_chflag |= int(channel_flag::F_WATCH_WRITE);
			rt = L->aio_do(aio_action::WRITE, m_aio_ctx);
			NETP_ASSERT(rt == netp::OK);
			ctx->ol_w->action = iocp_ol_action::WSASEND;
			ctx->ol_w->action_status |= AS_WAIT_IOCP;
			ctx->ol_w->fn_ol_done = fn_write != nullptr ? fn_write :
				std::bind(&socket_channel_iocp::__iocp_do_WSASend_done, NRP<socket_channel_iocp>(this), std::placeholders::_1, std::placeholders::_2);
		}

		void ch_aio_end_write() override {
			if (!L->in_event_loop()) {
				L->schedule([_so = NRP<socket_channel_iocp>(this)]()->void {
					_so->ch_aio_end_write();
				});
				return;
			}

			if (m_chflag & int(channel_flag::F_WATCH_WRITE)) {
				m_chflag &= ~int(channel_flag::F_WATCH_WRITE);

				L->aio_do(aio_action::END_WRITE, m_aio_ctx);
				((iocp_ctx*)m_aio_ctx)->ol_w->fn_ol_done = nullptr;
				NETP_TRACE_IOE("[socket][%s]aio_action::END_WRITE", ch_info().c_str());
			}
		}

		void ch_aio_connect(fn_aio_event_t const& fn = nullptr) override {
			NETP_ASSERT(fn != nullptr);
			if (m_chflag & int(channel_flag::F_WATCH_WRITE)) {
				return;
			}

			iocp_ctx* ctx = (iocp_ctx*)m_aio_ctx;
			int rt = __iocp_do_ConnectEx(ctx->ol_w);
			if (rt != netp::OK) {
				fn(rt, m_aio_ctx);
				return;
			}
			rt = L->aio_do(aio_action::WRITE, m_aio_ctx);
			NETP_ASSERT(rt == netp::OK);
			m_chflag |= int(channel_flag::F_WATCH_WRITE);
			ctx->ol_w->action = iocp_ol_action::CONNECTEX;
			ctx->ol_w->action_status |= AS_WAIT_IOCP;
			ctx->ol_w->fn_ol_done = fn;
		}

		void ch_aio_end_connect() override {
			NETP_ASSERT(!ch_is_passive());
			if (m_chflag & int(channel_flag::F_WATCH_WRITE)) {
				m_chflag &= ~int(channel_flag::F_WATCH_WRITE);
				L->aio_do(aio_action::END_WRITE, m_aio_ctx);
				((iocp_ctx*)m_aio_ctx)->ol_w->fn_ol_done = nullptr;
			}
		}
	};
}

#endif //IOCP
#endif