#ifndef _NETP_POLLER_IOCP_HPP
#define _NETP_POLLER_IOCP_HPP

#include <netp/core.hpp>

#if defined(NETP_HAS_POLLER_IOCP)

#include <netp/poller_abstract.hpp>
#include <netp/os/winsock_helper.hpp>

#define NETP_IOCP_INTERRUPT_COMPLETE_KEY (-13)
#define NETP_IOCP_INTERRUPT_COMPLETE_PARAM (-13)

namespace netp {

	enum iocp_ol_action {
		ACCEPTEX,
		WSAREAD,
		WSASEND,
		CONNECTEX,
		WSARECVFROM,
		CALL_MAX
	};

	enum action_status {
		AS_WAIT_IOCP = 1 << 0,
		AS_DONE = 1 << 1,
		AS_CH_END = 1 << 2
	};

#define NETP_IOCP_BUF_SIZE (32*1024)
	struct aio_ctx;
	struct ol_ctx
	{
		WSAOVERLAPPED ol;
		SOCKET fd;
		SOCKET accept_fd;
		aio_ctx* aioctx;
		u8_t action;
		u8_t action_status;
		fn_aio_event_t fn_ol_done;
		WSABUF wsabuf;
		char buf[NETP_IOCP_BUF_SIZE];
	};

	__NETP_FORCE_INLINE static ol_ctx* ol_ctx_allocate(SOCKET fd) {
		ol_ctx* olctx = netp::allocator<ol_ctx>::malloc(1);
		::memset(&olctx->ol, 0, sizeof(olctx->ol));
		olctx->fd = fd;
		olctx->accept_fd = NETP_INVALID_SOCKET;
		olctx->action = u8_t(-1);
		olctx->action_status = 0;
		new ((fn_aio_event_t*)&(olctx->fn_ol_done))(fn_aio_event_t)();
		olctx->wsabuf = { NETP_IOCP_BUF_SIZE, (olctx->buf) };
		return olctx;
	}
	__NETP_FORCE_INLINE static void ol_ctx_deallocate(ol_ctx* ctx) {
		netp::allocator<ol_ctx>::free(ctx);
	}

	__NETP_FORCE_INLINE static void ol_ctx_reset(ol_ctx* ctx) {
		NETP_ASSERT(ctx != 0);
		::memset(&ctx->ol, 0, sizeof(ctx->ol));
	}

	struct aio_ctx {
		aio_ctx* prev, * next;
		SOCKET fd;
		ol_ctx* ol_r;
		ol_ctx* ol_w;
		fn_aio_event_t fn_notify;
	};

	inline static aio_ctx* aio_ctx_allocate( SOCKET fd ) {
		aio_ctx* ctx = netp::allocator<aio_ctx>::malloc(1);
		ctx->fd = fd;
		ctx->ol_r = ol_ctx_allocate(fd);
		ctx->ol_r->aioctx = ctx;
		ctx->ol_w = ol_ctx_allocate(fd);
		ctx->ol_w->aioctx = ctx;

		new ((fn_aio_event_t*)&(ctx->fn_notify))(fn_aio_event_t)();
		return ctx;
	}

	inline static void aio_ctx_deallocate(aio_ctx* ctx) {
		NETP_ASSERT(ctx->fn_notify == nullptr);

		ctx->ol_r->action_status |= (AS_CH_END|AS_DONE);
		if ((ctx->ol_r->action_status & AS_WAIT_IOCP) == 0) {
			ol_ctx_deallocate(ctx->ol_r);
		}
		ctx->ol_w->action_status |= (AS_CH_END | AS_DONE);
		if ((ctx->ol_w->action_status & AS_WAIT_IOCP) == 0) {
			ol_ctx_deallocate(ctx->ol_w);
		}
		netp::allocator<aio_ctx>::free(ctx);
	}

	class poller_iocp final :
		public poller_abstract
	{
		HANDLE m_handle;
		aio_ctx m_aio_ctx_list;

#ifdef NETP_DEBUG_AIO_CTX_
		long m_aio_ctx_count_alloc;
		long m_aio_ctx_count_free;

		long m_ol_count_alloc;
		long m_ol_count_free;
#endif

		void interrupt_wait() override {
			NETP_ASSERT(m_handle != nullptr);
			BOOL postrt = ::PostQueuedCompletionStatus(m_handle, (DWORD)NETP_IOCP_INTERRUPT_COMPLETE_PARAM, (ULONG_PTR)NETP_IOCP_INTERRUPT_COMPLETE_KEY, 0);
			if (postrt == FALSE) {
				NETP_ERR("PostQueuedCompletionStatus failed: %d", netp_socket_get_last_errno());
				return;
			}
			NETP_TRACE_IOE("[iocp]interrupt_wait");
		}

		void _handle_iocp_event(ol_ctx* olctx, DWORD& dwTrans, int& ec) {
			NETP_ASSERT(olctx != nullptr);
			switch (olctx->action) {
			case iocp_ol_action::WSAREAD:
			{
				NETP_ASSERT(olctx->fd != NETP_INVALID_SOCKET);
				NETP_ASSERT(olctx->accept_fd == NETP_INVALID_SOCKET);
				if (olctx->action_status&AS_DONE) {
					olctx->accept_fd = ec == 0 ? dwTrans : ec;
					return;
				}
				NETP_ASSERT(olctx->fn_ol_done != nullptr);
				olctx->fn_ol_done(ec == 0 ? (int)dwTrans : ec, olctx->aioctx);
			}
			break;
			case iocp_ol_action::WSASEND:
			{
				NETP_ASSERT(olctx->fd != NETP_INVALID_SOCKET);
				NETP_ASSERT(olctx->accept_fd == NETP_INVALID_SOCKET);
				if (olctx->action_status & AS_DONE) {
					return;
				}
				NETP_ASSERT(olctx->fn_ol_done != nullptr);
				olctx->fn_ol_done(ec == 0 ? (int)dwTrans : ec, olctx->aioctx );
			}
			break;
			case iocp_ol_action::ACCEPTEX:
			{
				NETP_ASSERT(olctx->fd != NETP_INVALID_SOCKET);
				if (olctx->action_status & AS_DONE) {
					if (olctx->accept_fd != NETP_INVALID_SOCKET) {
						NETP_CLOSE_SOCKET(olctx->accept_fd);
						olctx->accept_fd = NETP_INVALID_SOCKET;
					}
					return;
				}

				NETP_ASSERT(olctx->accept_fd != NETP_INVALID_SOCKET);
				NETP_ASSERT(olctx->fn_ol_done != nullptr);

				if (ec == netp::OK) {
					ec = netp::default_socket_api.setsockopt(olctx->accept_fd, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char*)&olctx->fd, sizeof(olctx->fd));
				}

				if (NETP_LIKELY(ec == 0)) {
					NETP_TRACE_IOE("[iocp][#%u]accept done, new fd: %u", olctx->fd, olctx->accept_fd);
					olctx->fn_ol_done(ec, olctx->aioctx);
				} else {
					NETP_CLOSE_SOCKET(olctx->accept_fd);
				}
			}
			break;
			case iocp_ol_action::CONNECTEX:
			{
				NETP_ASSERT(olctx->fd != NETP_INVALID_SOCKET);
				NETP_ASSERT(olctx->accept_fd == NETP_INVALID_SOCKET);
				if (ec == netp::OK) {
					ec = ::setsockopt(olctx->fd, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, nullptr, 0);
					if (ec == NETP_SOCKET_ERROR) {
						ec = netp_socket_get_last_errno();
					}
				}
				if (olctx->action_status & AS_DONE) {
					return;
				}
				NETP_ASSERT(olctx->fn_ol_done != nullptr);
				olctx->fn_ol_done(ec == 0 ? (int)dwTrans : ec, olctx->aioctx );
			}
			break;
			}
		}
	public:
		poller_iocp() :
			poller_abstract(),
			m_handle(nullptr)
#ifdef NETP_DEBUG_AIO_CTX_
		,m_aio_ctx_count_alloc(0)
		,m_aio_ctx_count_free(0)
		,m_ol_count_alloc(0)
		,m_ol_count_free(0)
#endif
		{}

		void init() override {
			netp::list_init(&m_aio_ctx_list);
			NETP_ASSERT(m_handle == nullptr);
			m_handle = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, (u_long)0, 1);
			NETP_ALLOC_CHECK(m_handle, sizeof(m_handle));
		}

		void deinit() override {
			NETP_ASSERT(m_handle != nullptr);
			interrupt_wait();
			::CloseHandle(m_handle);
			m_handle = nullptr;
#ifdef NETP_DEBUG_AIO_CTX_
			NETP_ASSERT(m_aio_ctx_count_alloc == m_aio_ctx_count_free);
			NETP_ASSERT(m_ol_count_alloc == m_ol_count_free);
#endif
		}

		void poll(long long wait_in_nano, std::atomic<bool>& W) override {
			NETP_ASSERT(m_handle > 0);
			const long long wait_in_milli = wait_in_nano != ~0 ? (wait_in_nano / 1000000L) : ~0;
			//INFINITE == -1
			// 
			//#define NETP_USE_GET_QUEUED_COMPLETION_STATUS_EX
#ifdef NETP_USE_GET_QUEUED_COMPLETION_STATUS_EX
			OVERLAPPED_ENTRY entrys[64];
			ULONG n = 64;
			NETP_TRACE_IOE("[iocp]GetQueuedCompletionStatusEx wait: %llu:", dw_wait_in_nano);
			BOOL getOk = ::GetQueuedCompletionStatusEx(m_handle, &entrys[0], n, &n, (DWORD)(dw_wait_in_nano), FALSE);
			NETP_TRACE_IOE("[iocp]GetQueuedCompletionStatusEx wakenup");
			if (NETP_UNLIKELY(getOk) == FALSE) {
				ec = netp_socket_get_last_errno();
				NETP_TRACE_IOE("[iocp]GetQueuedCompletionStatusEx return: %d", ec);
				return;
			}
			NETP_ASSERT(n > 0);

			for (ULONG i = 0; i < n; ++i) {
				ULONG_PTR fd = entrys[i].lpCompletionKey;
				if (fd == NETP_IOCP_INTERRUPT_COMPLETE_KEY) {
					NETP_TRACE_IOE("[iocp]GetQueuedCompletionStatusEx wakenup for interrupt");
					NETP_ASSERT(entrys[i].dwNumberOfBytesTransferred == NETP_IOCP_INTERRUPT_COMPLETE_PARAM);
					continue;
				}

				NETP_ASSERT(fd > 0 && fd != NETP_INVALID_SOCKET);
				LPOVERLAPPED ol = entrys[i].lpOverlapped;
				NETP_ASSERT(ol != 0);

				DWORD dwTrans;
				DWORD dwFlags = 0;
				if (FALSE == ::WSAGetOverlappedResult(fd, ol, &dwTrans, FALSE, &dwFlags)) {
					ec = netp_socket_get_last_errno();
					NETP_DEBUG("[#%d]dwTrans: %d, dwFlags: %d, update ec: %d", fd, dwTrans, dwFlags, ec);
				}
				if (ec == netp::E_WSAENOTSOCK) {
					return;
				}
				_handle_iocp_event(ol, dwTrans, ec);
			}
#else
			int ec = 0;
			DWORD len;
			ULONG_PTR ckey; // it'w our fd here
			LPOVERLAPPED ol;
			BOOL getOk = ::GetQueuedCompletionStatus(m_handle, &len, &ckey, &ol, (DWORD)wait_in_milli);
			__LOOP_EXIT_WAITING__(W);

			if (NETP_UNLIKELY(getOk == FALSE)) {
				ec = netp_socket_get_last_errno();
				if (ec == netp::E_WAIT_TIMEOUT) {
					NETP_DEBUG("[iocp]GetQueuedCompletionStatus return: %d", ec);
					return;
				}

				//did not dequeue a completion packet from the completion port
				if (ol == nullptr) {
					NETP_DEBUG("[iocp]GetQueuedCompletionStatus return: %d, no packet dequeue", ec);
					return;
				}
				NETP_ASSERT(len == 0);
				NETP_ASSERT(ec != 0);
				NETP_INFO("[iocp]GetQueuedCompletionStatus failed, %d", ec);
			}
			if (ckey == NETP_IOCP_INTERRUPT_COMPLETE_KEY) {
				NETP_ASSERT(ol == nullptr);
				NETP_TRACE_IOE("[iocp]GetQueuedCompletionStatus waken signal");
				return;
			}
			NETP_ASSERT(ol != nullptr);
			ol_ctx* olctx=(CONTAINING_RECORD(ol, ol_ctx, ol));
			olctx->action_status &= ~AS_WAIT_IOCP;

			if (NETP_UNLIKELY(ec != 0)) {
				//UPDATE EC
				DWORD dwTrans = 0;
				DWORD dwFlags = 0;
				if (FALSE == ::WSAGetOverlappedResult(olctx->fd, ol, &dwTrans, FALSE, &dwFlags)) {
					ec = netp_socket_get_last_errno();
					NETP_DEBUG("[#%d]dwTrans: %d, dwFlags: %d, update ec: %d", ckey, dwTrans, dwFlags, ec);
				}
				NETP_ASSERT(dwTrans == len);
			}

			if (olctx->action_status&AS_CH_END) {
				ol_ctx_deallocate(olctx);
#ifdef NETP_DEBUG_AIO_CTX_
				++m_ol_count_free;
#endif
			} else {
				_handle_iocp_event(olctx, len, ec);
			}
#endif
		}

		virtual aio_ctx* aio_begin(SOCKET fd) {
			NETP_ASSERT(fd > 0);

			NETP_TRACE_IOE("[#%d][CreateIoCompletionPort]init", fd);
			NETP_ASSERT(m_handle != nullptr);
			HANDLE bindcp = ::CreateIoCompletionPort((HANDLE)fd, m_handle, 0, 0);
			if (bindcp == nullptr) {
				NETP_TRACE_IOE("[#%d][CreateIoCompletionPort]init error: %d", fd);
				return 0;
			}

			aio_ctx* ctx = netp::aio_ctx_allocate(fd);
			if (ctx == 0) {
				return 0;
			}
			netp::list_append(&m_aio_ctx_list, ctx);
#ifdef NETP_DEBUG_AIO_CTX_
			++m_aio_ctx_count_alloc;
			m_ol_count_alloc += 2;
#endif
			return ctx;
		}

		virtual void aio_end(aio_ctx* ctx) {

#ifdef NETP_DEBUG_AIO_CTX_
			++m_aio_ctx_count_free;
			if ( (ctx->ol_r->action_status & AS_WAIT_IOCP) == 0) {
				++m_ol_count_free;
			}
			if ((ctx->ol_w->action_status & AS_WAIT_IOCP) == 0) {
				++m_ol_count_free;
			}
#endif

			netp::list_delete(ctx);
			netp::aio_ctx_deallocate(ctx);
		}

		int aio_do(aio_action act, aio_ctx* ctx) override {
				switch (act) {
				case aio_action::READ:
				{
					ol_ctx* olctx = ctx->ol_r;
					NETP_ASSERT(olctx != nullptr);
					olctx->action_status &= ~AS_DONE;
				}
				break;
				case aio_action::END_READ:
				{
					ol_ctx* olctx = ctx->ol_r;
					NETP_ASSERT(olctx != nullptr);
					olctx->action_status |= AS_DONE;
				}
				break;
				case aio_action::WRITE:
				{
					ol_ctx* olctx = ctx->ol_r;
					NETP_ASSERT(olctx != nullptr);
					olctx->action_status &= ~AS_DONE;
				}
				break;
				case aio_action::END_WRITE:
				{
					ol_ctx* olctx = ctx->ol_r;
					NETP_ASSERT(olctx != nullptr);
					olctx->action_status |= AS_DONE;
				}
				break;
				case aio_action::NOTIFY_TERMINATING:
				{
					aio_ctx* _ctx, *_ctx_n;
					for (_ctx = m_aio_ctx_list.next, _ctx_n = _ctx->next; _ctx != &m_aio_ctx_list; _ctx = _ctx_n,_ctx_n = _ctx->next) {
						NETP_ASSERT( _ctx->fd >0 );
						NETP_ASSERT(_ctx->fn_notify != nullptr);

						if (_ctx->ol_r->fn_ol_done != nullptr) {
							_ctx->ol_r->fn_ol_done(E_IO_EVENT_LOOP_NOTIFY_TERMINATING, _ctx);
						}
						if (_ctx->ol_w->fn_ol_done != nullptr) {
							_ctx->ol_w->fn_ol_done(E_IO_EVENT_LOOP_NOTIFY_TERMINATING, _ctx);
						}
						//in case , close would result in _ctx->fn_notify be nullptr
						if (_ctx->fn_notify != nullptr) {
							_ctx->fn_notify(E_IO_EVENT_LOOP_NOTIFY_TERMINATING, _ctx);
						}
					}
				}
				break;
				}
				return netp::OK;
			}

		virtual int watch(u8_t, aio_ctx*) override { return netp::OK; }
		virtual int unwatch(u8_t, aio_ctx*) override { return netp::OK; }
	};
}

#endif //NETP_HAS_POLLER_IOCP

#endif