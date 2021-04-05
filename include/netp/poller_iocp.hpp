#ifndef _NETP_POLLER_IOCP_HPP
#define _NETP_POLLER_IOCP_HPP

#include <netp/core.hpp>

#if defined(NETP_HAS_POLLER_IOCP)

#include <netp/poller_abstract.hpp>
#include <netp/os/winsock_helper.hpp>
#include <netp/socket_api.hpp>

namespace netp {

	enum iocp_ol_action {
		ACCEPTEX,
		WSAREAD,
		WSASEND,
		CONNECTEX,
		CALL_MAX
	};

	enum action_status {
		AS_WAIT_IOCP = 1 << 0,
		AS_DONE = 1 << 1,
		AS_CH_END = 1 << 2
	};
	struct io_ctx;

	struct iocp_ctx;
	struct ol_ctx
	{
		WSAOVERLAPPED ol;
		SOCKET fd;
		SOCKET accept_fd;
		iocp_ctx* iocpctx;
		u8_t action;
		u8_t action_status;
		fn_io_event_t fn_ol_done;
		WSABUF wsabuf;
		char* rcvbuf;
		struct sockaddr_in* from_ptr;
		int* from_len_ptr;
	};

	__NETP_FORCE_INLINE static ol_ctx* ol_ctx_allocate(SOCKET fd) {
		ol_ctx* olctx = netp::allocator<ol_ctx>::malloc(1);
		::memset(&olctx->ol, 0, sizeof(olctx->ol));
		olctx->fd = fd;
		olctx->accept_fd = NETP_INVALID_SOCKET;
		olctx->action = u8_t(-1);
		olctx->action_status = 0;
		new ((fn_io_event_t*)&(olctx->fn_ol_done))(fn_io_event_t)();
		olctx->wsabuf = { 0,0 };
		olctx->rcvbuf = 0;
		return olctx;
	}

	__NETP_FORCE_INLINE static void ol_ctx_deallocate(ol_ctx* ctx) {
		if (ctx->rcvbuf != 0) {
			netp::allocator<char>::free(ctx->rcvbuf);
			ctx->rcvbuf = 0;
		}
		netp::allocator<ol_ctx>::free(ctx);
	}

	__NETP_FORCE_INLINE static void ol_ctx_reset(ol_ctx* ctx) {
		::memset(&ctx->ol, 0, sizeof(ctx->ol));
	}

	struct iocp_ctx {
		iocp_ctx* prev, * next;
		SOCKET fd;
		ol_ctx* ol_r;
		ol_ctx* ol_w;
		fn_io_event_t fn_notify;
	};

	inline static iocp_ctx* iocp_ctx_allocate( SOCKET fd ) {
		iocp_ctx* ctx = netp::allocator<iocp_ctx>::malloc(1);
		ctx->fd = fd;
		ctx->ol_r = ol_ctx_allocate(fd);
		ctx->ol_r->iocpctx = ctx;
		ctx->ol_w = ol_ctx_allocate(fd);
		ctx->ol_w->iocpctx = ctx;
		new ((fn_io_event_t*)&(ctx->fn_notify))(fn_io_event_t)();
		return ctx;
	}

	inline static void iocp_ctx_deallocate(iocp_ctx* ctx) {
		NETP_ASSERT(ctx->fn_notify == nullptr);
		NETP_ASSERT(ctx->ol_r->fn_ol_done == nullptr);
		NETP_ASSERT(ctx->ol_w->fn_ol_done == nullptr);

		ctx->ol_r->action_status |= (AS_CH_END|AS_DONE);
		if ((ctx->ol_r->action_status & AS_WAIT_IOCP) == 0) {
			ol_ctx_deallocate(ctx->ol_r);
		}
		ctx->ol_w->action_status |= (AS_CH_END | AS_DONE);
		if ((ctx->ol_w->action_status & AS_WAIT_IOCP) == 0) {
			ol_ctx_deallocate(ctx->ol_w);
		}
		netp::allocator<iocp_ctx>::free(ctx);
	}

	class poller_iocp final :
		public poller_abstract
	{
		HANDLE m_handle;
		iocp_ctx m_io_ctx_list;

#ifdef NETP_DEBUG_IO_CTX_
		long m_io_ctx_count_alloc;
		long m_io_ctx_count_free;

		long m_ol_count_alloc;
		long m_ol_count_free;
#endif

		void interrupt_wait() override {
			NETP_ASSERT(m_handle != nullptr);
			BOOL postrt = ::PostQueuedCompletionStatus(m_handle, 0,0,0);
			if (postrt == FALSE) {
				NETP_ERR("PostQueuedCompletionStatus failed: %d", netp_socket_get_last_errno());
				return;
			}
			NETP_TRACE_IOE("[iocp]interrupt_wait");
		}

		inline void _handle_iocp_event(ol_ctx* olctx, int& ec, DWORD& dwTrans) {
			NETP_ASSERT(olctx != nullptr);
			NETP_ASSERT(olctx->fd != NETP_INVALID_SOCKET);
			switch (olctx->action) {
			case iocp_ol_action::WSAREAD:
			{
				NETP_ASSERT(olctx->accept_fd == NETP_INVALID_SOCKET);
				if ((olctx->action_status&AS_DONE) ) {
					if (ec == 0 && (dwTrans > 0)) {
						olctx->accept_fd = dwTrans;
					}
					return;
				}
				NETP_ASSERT(olctx->fn_ol_done != nullptr);
				olctx->fn_ol_done(ec == 0 ? (int)dwTrans : ec, (io_ctx*)olctx->iocpctx);
			}
			break;
			case iocp_ol_action::WSASEND:
			{
				if (olctx->action_status & AS_DONE) {
					return;
				}
				NETP_ASSERT(olctx->fn_ol_done != nullptr);
				olctx->fn_ol_done(ec == 0 ? (int)dwTrans : ec, (io_ctx*)olctx->iocpctx );
			}
			break;
			case iocp_ol_action::ACCEPTEX:
			{
				if (olctx->action_status & AS_DONE) {
					return;
				}

				NETP_ASSERT(olctx->accept_fd != NETP_INVALID_SOCKET);
				NETP_ASSERT(olctx->fn_ol_done != nullptr);

				if (ec == netp::OK) {
					ec = netp::setsockopt(olctx->accept_fd, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char*)&olctx->fd, sizeof(olctx->fd));
				}

				if (NETP_LIKELY(ec == 0)) {
					NETP_TRACE_IOE("[iocp][#%u]accept done, new fd: %u", olctx->fd, olctx->accept_fd);
					olctx->fn_ol_done(ec, (io_ctx*)olctx->iocpctx);
				} else {
					NETP_CLOSE_SOCKET(olctx->accept_fd);
				}
			}
			break;
			case iocp_ol_action::CONNECTEX:
			{
				if (olctx->action_status & AS_DONE) {
					return;
				}
				if (ec == netp::OK) {
					ec = ::setsockopt(olctx->fd, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, nullptr, 0);
					if (ec == NETP_SOCKET_ERROR) {
						ec = netp_socket_get_last_errno();
					}
				}
				NETP_ASSERT(olctx->fn_ol_done != nullptr);
				olctx->fn_ol_done(ec == 0 ? (int)dwTrans : ec, (io_ctx*)olctx->iocpctx );
			}
			break;
			default:
			{
				NETP_ASSERT(!"WHAT!!!, missing ol action");
			}
			}
		}
	public:
		poller_iocp() :
			poller_abstract(),
			m_handle(nullptr)
#ifdef NETP_DEBUG_IO_CTX_
		,m_io_ctx_count_alloc(0)
		,m_io_ctx_count_free(0)
		,m_ol_count_alloc(0)
		,m_ol_count_free(0)
#endif
		{}

		void init() override {
			netp::list_init(&m_io_ctx_list);
			NETP_ASSERT(m_handle == nullptr);
			m_handle = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, (u_long)0, 1);
			NETP_ALLOC_CHECK(m_handle, sizeof(m_handle));
		}

		void deinit() override {
			NETP_ASSERT(m_handle != nullptr);
			interrupt_wait();
			::CloseHandle(m_handle);
			m_handle = nullptr;
#ifdef NETP_DEBUG_IO_CTX_
			NETP_ASSERT(m_io_ctx_count_alloc == m_io_ctx_count_free);
			NETP_ASSERT(m_ol_count_alloc == m_ol_count_free);
#endif
		}

		void poll(i64_t wait_in_nano, std::atomic<bool>& W) override {
			NETP_ASSERT(m_handle > 0);
			const long long wait_in_milli = wait_in_nano != ~0 ? (wait_in_nano / 1000000L) : ~0;
			//INFINITE == -1
			 
#define NETP_USE_GET_QUEUED_COMPLETION_STATUS_EX
#ifdef NETP_USE_GET_QUEUED_COMPLETION_STATUS_EX
			OVERLAPPED_ENTRY entrys[64];
			ULONG n = 64;
			int ec = netp::OK;
			BOOL getOk = ::GetQueuedCompletionStatusEx(m_handle, &entrys[0], n, &n, (DWORD)(wait_in_milli), FALSE);
			NETP_POLLER_WAIT_EXIT(wait_in_nano, W);
			if (NETP_UNLIKELY(getOk) == FALSE) {
				ec = netp_socket_get_last_errno();
				if (ec == netp::E_WAIT_TIMEOUT) {
					NETP_TRACE_IOE("[iocp]GetQueuedCompletionStatus return: %d", ec);
					return;
				}
				NETP_THROW("GetQueuedCompletionStatusEx failed");
			}
			for (ULONG i = 0; i < n; ++i) {
				LPOVERLAPPED& ol = entrys[i].lpOverlapped;
				if (ol == 0) {
					NETP_TRACE_IOE("[iocp]GetQueuedCompletionStatusEx, no packet dequeue");
					return;
				}
				ol_ctx* olctx = (CONTAINING_RECORD(ol, ol_ctx, ol));
				olctx->action_status &= ~AS_WAIT_IOCP;

				DWORD dwTrans;
				DWORD dwFlags = 0;
				ec = netp::OK;
				if (FALSE == ::WSAGetOverlappedResult(olctx->fd, ol, &dwTrans, FALSE, &dwFlags)) {
					ec = netp_socket_get_last_errno();
					NETP_DEBUG("[#%d]dwTrans: %d, dwFlags: %d, update ec: %d", olctx->fd, dwTrans, dwFlags, ec);
				}

				if (olctx->action_status & AS_CH_END) {
					ol_ctx_deallocate(olctx);
#ifdef NETP_DEBUG_IO_CTX_
					++m_ol_count_free;
#endif
				} else {
					_handle_iocp_event(olctx, ec, dwTrans);
				}
			}
#else
			int ec = 0;
			DWORD dwTrans_;
			ULONG_PTR ckey; // it'w our fd here
			LPOVERLAPPED ol;
			BOOL getOk = ::GetQueuedCompletionStatus(m_handle, &dwTrans_, &ckey, &ol, (DWORD)wait_in_milli);
			NETP_POLLER_WAIT_EXIT(wait_in_nano, W);

			if (NETP_UNLIKELY(getOk == FALSE)) {
				ec = netp_socket_get_last_errno();
				if (ec == netp::E_WAIT_TIMEOUT) {
					NETP_DEBUG("[iocp]GetQueuedCompletionStatus return: %d", ec);
					return;
				}

				NETP_ASSERT(dwTrans_ == 0);
				NETP_ASSERT(ec != 0);
				//leave this line here ,we need to check to free the ol
				NETP_INFO("[iocp]GetQueuedCompletionStatus failed, %d", ec);
				//return;
			}
			//did not dequeue a completion packet from the completion port
			if (ol == 0) {
				NETP_DEBUG("[iocp]GetQueuedCompletionStatus return: %d, no packet dequeue", ec);
				return;
			}
			ol_ctx* olctx=(CONTAINING_RECORD(ol, ol_ctx, ol));
			olctx->action_status &= ~AS_WAIT_IOCP;
			DWORD dwTrans = 0;
			DWORD dwFlags = 0;
			if (FALSE == ::WSAGetOverlappedResult(olctx->fd, ol, &dwTrans, FALSE, &dwFlags)) {
				ec = netp_socket_get_last_errno();
				NETP_DEBUG("[#%d]dwTrans: %d, dwFlags: %d, update ec: %d", ckey, dwTrans, dwFlags, ec);
			}
			NETP_ASSERT(dwTrans == dwTrans_);

			if (olctx->action_status&AS_CH_END) {
				ol_ctx_deallocate(olctx);
#ifdef NETP_DEBUG_IO_CTX_
				++m_ol_count_free;
#endif
			} else {
				_handle_iocp_event(olctx, ec, dwTrans_);
			}
#endif
		}

		virtual io_ctx* io_begin(SOCKET fd, NRP<io_monitor> const& iom) override {
			NETP_ASSERT(fd > 0);
			(void)iom;
			NETP_TRACE_IOE("[#%d][CreateIoCompletionPort]init", fd);
			NETP_ASSERT(m_handle != nullptr);
			HANDLE bindcp = ::CreateIoCompletionPort((HANDLE)fd, m_handle, 0, 0);
			if (bindcp == nullptr) {
				NETP_TRACE_IOE("[#%d][CreateIoCompletionPort]init error: %d", fd);
				return 0;
			}

			iocp_ctx* ctx = netp::iocp_ctx_allocate(fd);
			if (ctx == 0) {
				return 0;
			}
			netp::list_append(&m_io_ctx_list, ctx);
#ifdef NETP_DEBUG_IO_CTX_
			++m_io_ctx_count_alloc;
			m_ol_count_alloc += 2;
#endif
			return (io_ctx*)ctx;
		}

		virtual void io_end(io_ctx* ctx_) override {
			iocp_ctx* ctx = (iocp_ctx*)ctx_;
#ifdef NETP_DEBUG_IO_CTX_
			++m_io_ctx_count_free;
			if ( (ctx->ol_r->action_status & AS_WAIT_IOCP) == 0) {
				++m_ol_count_free;
			}
			if ((ctx->ol_w->action_status & AS_WAIT_IOCP) == 0) {
				++m_ol_count_free;
			}
#endif

			netp::list_delete((iocp_ctx*)ctx);
			netp::iocp_ctx_deallocate((iocp_ctx*)ctx);
		}

		int io_do(io_action act, io_ctx* ctx) override {
				switch (act) {
				case io_action::READ:
				{
					ol_ctx* olctx = ((iocp_ctx*)ctx)->ol_r;
					olctx->action_status &= ~AS_DONE;
				}
				break;
				case io_action::END_READ:
				{
					ol_ctx* olctx = ((iocp_ctx*)ctx)->ol_r;
					olctx->action_status |= AS_DONE;
				}
				break;
				case io_action::WRITE:
				{
					ol_ctx* olctx = ((iocp_ctx*)ctx)->ol_w;
					olctx->action_status &= ~AS_DONE;
				}
				break;
				case io_action::END_WRITE:
				{
					ol_ctx* olctx = ((iocp_ctx*)ctx)->ol_w;
					olctx->action_status |= AS_DONE;
				}
				break;
				case io_action::NOTIFY_TERMINATING:
				{
					iocp_ctx* _ctx, *_ctx_n;
					for (_ctx = m_io_ctx_list.next, _ctx_n = _ctx->next; _ctx != &m_io_ctx_list; _ctx = _ctx_n,_ctx_n = _ctx->next) {
						NETP_ASSERT( _ctx->fd >0 );
						NETP_ASSERT(_ctx->fn_notify != nullptr);

						if (_ctx->ol_r->fn_ol_done != nullptr) {
							_ctx->ol_r->fn_ol_done(E_IO_EVENT_LOOP_NOTIFY_TERMINATING, (io_ctx*)_ctx);
						}
						if (_ctx->ol_w->fn_ol_done != nullptr) {
							_ctx->ol_w->fn_ol_done(E_IO_EVENT_LOOP_NOTIFY_TERMINATING, (io_ctx*)_ctx);
						}
						//in case , close would result in _ctx->fn_notify be nullptr
						if (_ctx->fn_notify != nullptr) {
							_ctx->fn_notify(E_IO_EVENT_LOOP_NOTIFY_TERMINATING, (io_ctx*)_ctx);
						}
					}
				}
				break;
				}
				return netp::OK;
			}

		int watch(u8_t, io_ctx*) override { return netp::OK; }
		int unwatch(u8_t, io_ctx*) override { return netp::OK; }
	};
}
#endif //NETP_HAS_POLLER_IOCP
#endif