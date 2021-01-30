#ifndef _NETP_IOCP_POLLER_HPP
#define _NETP_IOCP_POLLER_HPP

#include <netp/core.hpp>

#ifdef NETP_ENABLE_IOCP
#include <netp/io_event_loop.hpp>
#include <netp/os/winsock_helper.hpp>
#define NETP_IOCP_INTERRUPT_COMPLETE_KEY (-17)
#define NETP_IOCP_INTERRUPT_COMPLETE_PARAM (-17)
#define NETP_IOCP_BUFFER_SIZE (1024*64)

namespace netp {

	class iocp_poller final :
		public io_event_loop
	{
		class socket;

		enum iocp_call {
			ACCEPT,
			READ,
			WRITE,
			CONNECT
		};

		enum action_status {
			AS_NORMAL,
			AS_BLOCKED,
			AS_ERROR
		};

		enum iocp_flag {
			AIO_READ		= 0x01,
			AIO_WRITE		= 0x02,
			F_ACCEPT	= 0x04,
			F_CONNECT	= 0x08
		};

		struct iocp_ctx;
		struct iocp_overlapped_ctx:
			public ref_base
		{
			WSAOVERLAPPED overlapped;
			NSP<iocp_ctx>iocpctx;
			SOCKET fd;
			SOCKET accept_fd;
			int action:24;
			int action_status:8;
			fn_overlapped_io_event fn_overlapped;
			fn_aio_event_t fn;
			WSABUF wsabuf;
			char buf[NETP_IOCP_BUFFER_SIZE];
		};

		struct iocp_ctx: 
			public ref_base
		{
			SOCKET fd;
			int flag;
			NRP<iocp_overlapped_ctx> ol_ctxs[iocp_action::MAX];
		};

		inline static NRP<iocp_overlapped_ctx> iocp_make_olctx() {
			NRP<iocp_overlapped_ctx> ctx = netp::make_ref<iocp_overlapped_ctx>();
			::memset(&ctx->overlapped, 0, sizeof(ctx->overlapped));
			ctx->fd = NETP_INVALID_SOCKET;
			ctx->accept_fd = NETP_INVALID_SOCKET;
			ctx->action = -1;
			ctx->wsabuf = { NETP_IOCP_BUFFER_SIZE, (char*)&ctx->buf };
			return ctx;
		}

		inline static void iocp_reset_olctx(NRP<iocp_overlapped_ctx> const& ctx) {
			NETP_ASSERT(ctx != nullptr);
			::memset(&ctx->overlapped, 0, sizeof(ctx->overlapped));
		}

		inline static void iocp_reset_accept_olctx(NRP<iocp_overlapped_ctx> const& ctx) {
			NETP_ASSERT(ctx != nullptr);
			::memset(&ctx->overlapped, 0, sizeof(ctx->overlapped));
			ctx->fd = NETP_INVALID_SOCKET;
		}

		typedef std::unordered_map<SOCKET, NRP<iocp_ctx>> iocp_ctx_map_t;
		typedef std::pair<SOCKET, NRP<iocp_ctx>> iocp_ctx_pair_t;

		HANDLE m_handle;
		iocp_ctx_map_t m_ctxs;

		void _do_poller_interrupt_wait() override {
			NETP_ASSERT(m_handle != nullptr);
			BOOL postrt = ::PostQueuedCompletionStatus(m_handle, (DWORD)NETP_IOCP_INTERRUPT_COMPLETE_PARAM, (ULONG_PTR)NETP_IOCP_INTERRUPT_COMPLETE_KEY,0 );
			if (postrt == FALSE ) {
				NETP_ERR("PostQueuedCompletionStatus failed: %d", netp_socket_get_last_errno());
				return;
			}
			NETP_TRACE_IOE("[iocp]interrupt_wait" );
		}

		inline static int _do_accept_ex(NRP<iocp_overlapped_ctx>& ctx) {
			NETP_ASSERT(ctx != nullptr);
			const SOCKET newfd = ctx->fn_overlapped((void*)&ctx->overlapped);
			if (newfd == NETP_INVALID_SOCKET) {
				int ec = netp_socket_get_last_errno();
				NETP_TRACE_IOE("[iocp][#%u]_do_accept_ex create fd failed: %d", ctx->fd, netp_socket_get_last_errno());
				ctx->fn({ NETP_INVALID_SOCKET, netp_socket_get_last_errno(), nullptr });
				return ec;
			}

			ctx->accept_fd = newfd;
			NETP_TRACE_IOE("[iocp][#%u]_do_accept_ex begin,new fd: %u", ctx->fd, ctx->accept_fd );

			LPFN_ACCEPTEX lpfnAcceptEx = (LPFN_ACCEPTEX)netp::os::load_api_ex_address(netp::os::API_ACCEPT_EX);
			NETP_ASSERT(lpfnAcceptEx != 0);
			BOOL acceptrt = lpfnAcceptEx(ctx->fd, newfd, ctx->buf, 0,
				sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16,
				nullptr, &(ctx->overlapped));

			if (!acceptrt)
			{
				int ec = netp_socket_get_last_errno();
				if (ec != netp::E_ERROR_IO_PENDING) {
					NETP_TRACE_IOE("[iocp][#%u]_do_accept_ex acceptex failed: %d", ctx->fd, netp_socket_get_last_errno());
					NETP_CLOSE_SOCKET(newfd);
					ctx->accept_fd = NETP_INVALID_SOCKET;
					ctx->fn({ NETP_INVALID_SOCKET, ec, nullptr });
					return ec;
				}
			}
			return netp::OK;
		}

		inline static int _do_read(NRP<iocp_overlapped_ctx>& olctx) {
			iocp_reset_ctx(olctx);
			DWORD flags = 0;
			if (::WSARecv(olctx->fd, &olctx->wsabuf, 1, nullptr, &flags, &olctx->overlapped, nullptr) == SOCKET_ERROR) {
				int ec = netp_socket_get_last_errno();
				if (ec != netp::E_ERROR_IO_PENDING) {
					olctx->action_status = AS_ERROR;
					olctx->fn({ olctx->fd, ec, nullptr });
					return ec;
				}
			}
			return netp::OK;
		}

		inline void _handle_iocp_event(LPOVERLAPPED& ol, DWORD& dwTrans, int& ec) {
			NRP<iocp_overlapped_ctx> olctx(CONTAINING_RECORD(ol, iocp_overlapped_ctx, overlapped));
			NETP_ASSERT(olctx != nullptr);
			switch (olctx->action) {
			case READ:
			{
				NETP_ASSERT(olctx->fd != NETP_INVALID_SOCKET);
				NETP_ASSERT(olctx->accept_fd == NETP_INVALID_SOCKET);
				olctx->fn({ olctx->fd, ec == 0 ? (int)dwTrans : ec, olctx->wsabuf.buf });
				if (olctx->action_status == AS_NORMAL ) {
					_do_read(olctx);
				} else {
					olctx->iocp_ctx = nullptr;
				}
			}
			break;
			case WRITE:
			{
				NETP_ASSERT(olctx->fd != NETP_INVALID_SOCKET );
				NETP_ASSERT(olctx->accept_fd == NETP_INVALID_SOCKET);
				olctx->fn({ olctx->fd, ec == 0 ? (int)dwTrans : ec, nullptr });
			}
			break;
			case ACCEPT:
			{
				NETP_ASSERT(olctx->fd != NETP_INVALID_SOCKET);
				NETP_ASSERT(olctx->accept_fd != NETP_INVALID_SOCKET);
				NETP_ASSERT(olctx->fn != nullptr);

				if (ec == netp::OK) {
					ec = ::setsockopt(olctx->accept_fd, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char*)&olctx->fd, sizeof(olctx->fd));
				}

				if (NETP_LIKELY(ec == 0)) {
					NETP_TRACE_IOE("[iocp][#%u]accept done, new fd: %u", olctx->fd, olctx->accept_fd);
					olctx->fn({ olctx->accept_fd,ec, olctx->wsabuf.buf });
				} else {
					NETP_CLOSE_SOCKET(ctx->accept_fd);
				}

				NETP_ASSERT(olctx->action_status == AS_NORMAL);
				if (olctx->action_status == AS_NORMAL) {
					_do_accept_ex(olctx);
				} else {
					olctx->iocp_ctx = nullptr;
				}
			}
			break;
			case CONNECT:
			{
				NETP_ASSERT(olctx->fd != NETP_INVALID_SOCKET);
				NETP_ASSERT(olctx->accept_fd == NETP_INVALID_SOCKET);
				olctx->iocp_ctx = nullptr;
				if (ec == netp::OK) {
					ec = ::setsockopt(olctx->fd, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, nullptr, 0);
					if (ec == NETP_SOCKET_ERROR) {
						ec = netp_socket_get_last_errno();
					}
				}
				olctx->fn({ olctx->fd, ec == 0 ? (int)dwTrans : ec, nullptr });
			}
			break; 
			default:
			{
				NETP_THROW("unknown io event flag");
			}
			}
		}
	public:
			iocp_poller() :
				io_event_loop(T_IOCP),
				m_handle(nullptr)
			{}

			void _do_poller_init() override {
				NETP_ASSERT(m_handle == nullptr);
				m_handle = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, (u_long)0, 1);
				NETP_ALLOC_CHECK(m_handle, sizeof(m_handle));
			}

			void _do_poller_deinit() override {
				NETP_ASSERT(m_handle != nullptr);
				_do_poller_interrupt_wait();
				::CloseHandle(m_handle);
				m_handle = nullptr;
			}

		void _do_poll(long long wait_in_nano) override {
			NETP_ASSERT(m_handle > 0);
			int ec = 0;
			const long long wait_in_milli = wait_in_nano != ~0 ? (wait_in_nano / 1000000L) : ~0;

//#define NETP_USE_GET_QUEUED_COMPLETION_STATUS_EX
#ifdef NETP_USE_GET_QUEUED_COMPLETION_STATUS_EX
			OVERLAPPED_ENTRY entrys[64];
			ULONG n = 64;
			NETP_TRACE_IOE("[iocp]GetQueuedCompletionStatusEx wait: %llu:", dw_wait_in_nano );
			BOOL getOk = ::GetQueuedCompletionStatusEx(m_handle, &entrys[0], n,&n, (DWORD)(dw_wait_in_nano),FALSE);
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

				NETP_ASSERT(fd>0&&fd!=NETP_INVALID_SOCKET);
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
			DWORD len;
			SOCKET fd;
			LPOVERLAPPED ol;
			BOOL getOk = ::GetQueuedCompletionStatus(m_handle, &len, (PULONG_PTR)&fd, &ol, (DWORD)wait_in_milli);

			if (NETP_UNLIKELY(getOk== FALSE)) {
				ec = netp_socket_get_last_errno();
				if (ec == netp::E_WAIT_TIMEOUT) {
					NETP_DEBUG("[iocp]GetQueuedCompletionStatus return: %d", ec );
					return;
				}
				//did not dequeue a completion packet from the completion port
				if (ol == nullptr) {
					NETP_DEBUG("[iocp]GetQueuedCompletionStatus return: %d, no packet dequeue", ec );
					return;
				}
				NETP_ASSERT(len == 0);
				NETP_ASSERT(ec != 0);
				NETP_INFO("[iocp]GetQueuedCompletionStatus failed, %d", ec);
			}
			if (fd == NETP_IOCP_INTERRUPT_COMPLETE_KEY) {
				NETP_ASSERT(ol == nullptr);
				NETP_TRACE_IOE("[iocp]GetQueuedCompletionStatus waken signal");
				return;
			}
			NETP_ASSERT(ol != nullptr);
			if (NETP_UNLIKELY(ec != 0)) {
				//UPDATE EC
				DWORD dwTrans = 0;
				DWORD dwFlags = 0;
				if (FALSE == ::WSAGetOverlappedResult(fd, ol, &dwTrans, FALSE, &dwFlags)) {
					ec = netp_socket_get_last_errno();
					NETP_DEBUG("[#%d]dwTrans: %d, dwFlags: %d, update ec: %d", fd, dwTrans, dwFlags, ec);
				}
				NETP_ASSERT(dwTrans == len);
			}
			_handle_iocp_event(ol, len, ec);
	#endif
		}

		void _do_watch(u8_t flag, SOCKET fd, fn_aio_event_t const& fn) override {
			NETP_ASSER(!"what: wrong call");
		}

		void _do_unwatch(u8_t flag, SOCKET fd) override
		{
			NETP_ASSER(!"what: wrong call");

		}

		void _do_begin(SOCKET fd, fn_aio_event_t const& fn) {
			NETP_TRACE_IOE("[#%d][CreateIoCompletionPort]init", fd);
			NETP_ASSERT(m_handle != nullptr);
			HANDLE bindcp = ::CreateIoCompletionPort((HANDLE)fd, m_handle, (u_long)fd, 0);
			if (bindcp == nullptr) {
				int ec = netp_socket_get_last_errno();
				fn({ fd,ec,nullptr });
				NETP_TRACE_IOE("[#%d][CreateIoCompletionPort]init error: %d", fd, ec);
			}

			iocp_ctx_map_t::iterator it = m_ctxs.find(fd);
			NETP_ASSERT(it != m_ctxs.end());
			NRP<iocp_ctx> iocpctx = netp::make_ref<iocp_ctx>();
			iocpctx->fd = fd;
			iocpctx->flag = 0;

			for (int i = iocp_call::ACCEPT; i <= iocp_call::CONNECT; ++i) {
				iocpctx->ol_ctxs[i] = iocp_make_ctx();
				iocpctx->ol_ctxs[i]->action = i;
				iocpctx->ol_ctxs[i]->fd = fd;
				iocpctx->ol_ctxs[i]->action_status = AS_NORMAL;
			}

			m_ctxs.insert({ fd, iocpctx });
			fn({ { fd,netp::OK,nullptr } });
		}

		void _do_iocp_end(SOCKET fd, fn_aio_event_t const& fn) {
			iocp_ctx_map_t::iterator&& it = m_ctxs.find(fd);
			NETP_ASSERT(it != m_ctxs.end());
			m_ctxs.erase(it);
			NETP_TRACE_IOE("[#%d][CreateIoCompletionPort]deinit", fd);
		}

		void do_call(iocp_action act, SOCKET fd, fn_overlapped_io_event const& fn_overlapped, fn_aio_event_t const& fn) override {

			iocp_ctx_map_t::iterator&& it = m_ctxs.find(fd);

			switch (act) {
			case iocp_action::BEGIN:
			{
				_do_begin(fd, fn);
			}
			break;
			case iocp_action::END:
			{
				_do_end(fd, fn);
			}
			break;
			case iocp_action::READ:
			{
				NETP_ASSERT(it != m_ctxs.end());
				iocp_ctx->flag |= iocp_flag::AIO_READ;
				NSP<iocp_ctx> iocpctx = it->second;
				NRP<iocp_overlapped_ctx>& olctx = iocp_ctx->ol_ctxs[iocp_call::READ];
				NETP_ASSERT(olctx != nullptr);
				olctx->fn = fn;
				const int rt = _do_read(olctx);
				if (rt == netp::OK) {
					ctx->iocp_ctx = iocp_ctx;
				}
			}
			break;
			case iocp_action::END_READ:
			{
				NSP<iocp_ctx>&& iocpctx = it->second;
				NETP_ASSERT(iocpctx != nullptr);
				iocpctx->flag &= ~iocp_flag::AIO_READ;
				iocpctx->ol_ctxs[READ]->action_status = AS_BLOCKED;
			}
			break;
			case iocp_action::WRITE:
			{
				NETP_ASSERT(it != m_ctxs.end());
				NRP<iocp_ctx>&& iocpctx = it->second;
				NETP_ASSERT(iocpctx != nullptr);

				iocpctx->flag |= iocp_flag::AIO_WRITE;
				NRP<iocp_overlapped_ctx>& olctx = iocpctx->ol_ctxs[iocp_call::WRITE];
				NETP_ASSERT(olctx != nullptr);
				iocp_reset_ctx(olctx);
				olctx->fn = fn;
				const overlapped_return_t wrt = fn_overlapped((void*)&olctx->overlapped);
				if (wrt != netp::OK) {
					fn({ olctx->fd, (overlapped_return_t)(wrt & 0xFFFFFFFF), nullptr });
				}
			}
			break;
			case iocp_action::ACCEPT:
			{
				NETP_ASSERT(it != m_ctxs.end());
				NRP<iocp_ctx>&& iocpctx = it->second;
				NETP_ASSERT(iocpctx != nullptr);

				iocpctx->flag |= iocp_flag::F_ACCEPT;
				NRP<iocp_overlapped_ctx>& olctx = iocpctx->ol_ctxs[iocp_call::ACCEPT];
				NETP_ASSERT(olctx != nullptr);
				iocp_reset_ctx(olctx);
				olctx->fn = fn;
				olctx->fn_overlapped = fn_overlapped;
				olctx->fd = fd;
				const int rt = _do_accept_ex(olctx);
				if (rt == netp::OK) {
					olctx->iocpctx = iocpctx;
				}
			}
			break;
			case iocp_action::CONNECT:
			{
				NETP_ASSERT(it != m_ctxs.end());
				NRP<iocp_ctx>&& iocpctx = it->second;
				NETP_ASSERT(iocpctx != nullptr);

				NRP<iocp_overlapped_ctx>& olctx = iocpctx->ol_ctxs[iocp_call::CONNECT];

				NETP_ASSERT(olctx != nullptr);
				_iocp_ctxs->flag |= F_CONNECT;
				iocp_reset_ctx(olctx);
				olctx->fn = fn;

				const overlapped_return_t crt = fn_overlapped((void*)&olctx->overlapped);
				if (crt != netp::OK) {
					fn({ olctx->fd, (overlapped_return_t)(crt & 0xFFFFFFFF), nullptr });
				} else {
					olctx->iocpctx = iocpctx;
				}
			}
		}
	};
}
#endif //NETP_ENABLE_IOCP
#endif