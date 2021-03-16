#ifndef _NETP_POLLER_IOCP_HPP
#define _NETP_POLLER_IOCP_HPP

#include <netp/core.hpp>

#if defined(NETP_HAS_POLLER_IOCP)

#include <netp/io_event_loop.hpp>
#include <netp/os/winsock_helper.hpp>

#define NETP_IOCP_INTERRUPT_COMPLETE_KEY (-17)
#define NETP_IOCP_INTERRUPT_COMPLETE_PARAM (-17)
#define NETP_IOCP_BUFFER_SIZE (1024*64)

namespace netp {

	class poller_iocp final :
		public io_event_loop
	{
		enum iocp_ol_action {
			ACCEPTEX,
			WSAREAD,
			WSASEND,
			CONNECTEX,
			CALL_MAX
		};

		enum iocp_ol_type {
			READ,
			WRITE
		};

		enum action_status {
			AS_WAIT_IOCP = 1 << 0,
			AS_DONE = 1 << 1,
		};

		enum channel_end {
			CH_END_NO = 0,
			CH_END_YES = 1
		};

		struct iocp_overlapped_ctx
		{
			WSAOVERLAPPED overlapped;
			SOCKET fd;
			SOCKET accept_fd;
			u8_t action;
			u8_t action_status;
			u8_t is_ch_end;
			fn_overlapped_io_event fn_overlapped;
			fn_iocp_event_t fn_iocp_done;
			WSABUF wsabuf;
			byte_t buf[NETP_IOCP_BUFFER_SIZE];
		};

		struct iocp_ctx :
			public non_atomic_ref_base
		{
			SOCKET fd;
			fn_iocp_event_t fn_notify;
			iocp_overlapped_ctx* ol_ctxs[iocp_ol_type::WRITE+1];
		};

		//@TODO, we need a specific pool for this kinds of object
		__NETP_FORCE_INLINE static iocp_overlapped_ctx* iocp_olctx_malloc() {
			iocp_overlapped_ctx* olctx = netp::allocator<iocp_overlapped_ctx>::malloc(1);
			::memset(&olctx->overlapped, 0, sizeof(olctx->overlapped));
			olctx->fd = NETP_INVALID_SOCKET;
			olctx->accept_fd = NETP_INVALID_SOCKET;
			olctx->action = -1;
			olctx->action_status = 0;
			olctx->is_ch_end = CH_END_NO;
			new ((fn_overlapped_io_event*)&(olctx->fn_overlapped))(fn_overlapped_io_event)();
			new ((fn_iocp_event_t*)&(olctx->fn_iocp_done))(fn_iocp_event_t)();
			olctx->wsabuf = { NETP_IOCP_BUFFER_SIZE, (char*)&olctx->buf };
			return olctx;
		}
		__NETP_FORCE_INLINE static void iocp_olctx_free(iocp_overlapped_ctx* ctx) {
			netp::allocator<iocp_overlapped_ctx>::free(ctx);
		}

		__NETP_FORCE_INLINE static void iocp_olctx_reset_overlapped(iocp_overlapped_ctx* ctx) {
			NETP_ASSERT(ctx != 0);
			::memset(&ctx->overlapped, 0, sizeof(ctx->overlapped));
		}

		typedef std::unordered_map<SOCKET, NRP<iocp_ctx>> iocp_ctx_map_t;
		typedef std::pair<SOCKET, NRP<iocp_ctx>> iocp_ctx_pair_t;

		HANDLE m_handle;
		iocp_ctx_map_t m_ctxs;

		void _do_poller_interrupt_wait() override {
			NETP_ASSERT(m_handle != nullptr);
			BOOL postrt = ::PostQueuedCompletionStatus(m_handle, (DWORD)NETP_IOCP_INTERRUPT_COMPLETE_PARAM, (ULONG_PTR)NETP_IOCP_INTERRUPT_COMPLETE_KEY, 0);
			if (postrt == FALSE) {
				NETP_ERR("PostQueuedCompletionStatus failed: %d", netp_socket_get_last_errno());
				return;
			}
			NETP_TRACE_IOE("[iocp]interrupt_wait");
		}

		inline static int _do_accept_ex(iocp_overlapped_ctx* olctx) {
			NETP_ASSERT(olctx != nullptr);
			int ec = olctx->fn_overlapped((void*)&olctx->accept_fd);
			if (olctx->accept_fd == NETP_INVALID_SOCKET) {
				ec = netp_socket_get_last_errno();
				NETP_TRACE_IOE("[iocp][#%u]_do_accept_ex create fd failed: %d", olctx->fd, ec);
				return ec;
			}

			NETP_TRACE_IOE("[iocp][#%u]_do_accept_ex begin,new fd: %u", olctx->fd, olctx->accept_fd);

			const static LPFN_ACCEPTEX lpfnAcceptEx = (LPFN_ACCEPTEX)netp::os::load_api_ex_address(netp::os::API_ACCEPT_EX);
			NETP_ASSERT(lpfnAcceptEx != 0);
			BOOL acceptrt = lpfnAcceptEx(olctx->fd, olctx->accept_fd, olctx->buf, 0,
				sizeof(struct sockaddr_in) + 16, sizeof(struct sockaddr_in) + 16,
				nullptr, &(olctx->overlapped));

			if (!acceptrt)
			{
				ec = netp_socket_get_last_errno();
				if (ec == netp::E_WSA_IO_PENDING) {
					ec = netp::OK;
				} else {
					NETP_TRACE_IOE("[iocp][#%u]_do_accept_ex acceptex failed: %d", olctx->fd, netp_socket_get_last_errno());
					NETP_CLOSE_SOCKET(olctx->accept_fd);
				}
			}
			return ec;
		}

		inline static int _do_read(iocp_overlapped_ctx* olctx) {
			iocp_olctx_reset_overlapped(olctx);
			DWORD flags = 0;
			int ec = ::WSARecv(olctx->fd, &olctx->wsabuf, 1, nullptr, &flags, &olctx->overlapped, nullptr);
			if (ec == -1) {
				ec = netp_socket_get_last_errno();
				if (ec == netp::E_WSA_IO_PENDING) {
					ec = netp::OK;
				}
			}
			return ec;
		}

		void _handle_iocp_event(iocp_overlapped_ctx* olctx, DWORD& dwTrans, int& ec) {
			NETP_ASSERT(olctx != nullptr);
			switch (olctx->action) {
			case iocp_ol_action::WSAREAD:
			{
				NETP_ASSERT(olctx->fd != NETP_INVALID_SOCKET);
				NETP_ASSERT(olctx->accept_fd == NETP_INVALID_SOCKET);
				NETP_ASSERT(olctx->fn_iocp_done != nullptr);
				if ( ec !=0 && (olctx->action_status&AS_DONE)) {
					//@NOTE: even the user has called aio_end_read(), he could get a incoming data from system
						//pending data len
					//user has called aio_end_read() ;
					//store len into accept_fd
					olctx->accept_fd = dwTrans;
					return;
				}

				ec = olctx->fn_iocp_done( { olctx->fd, ec == 0 ? (int)dwTrans : ec, olctx->buf } );
				if (ec == netp::E_CHANNEL_OVERLAPPED_OP_TRY && (olctx->action_status&AS_DONE) == 0 ) {
					ec = _do_read(olctx);
					if (ec == netp::OK) {
						olctx->action_status |= AS_WAIT_IOCP;
					} else {
						olctx->fn_iocp_done({ olctx->fd, ec ,0 });
					}
				}
			}
			break;
			case iocp_ol_action::WSASEND:
			{
				NETP_ASSERT(olctx->fd != NETP_INVALID_SOCKET);
				NETP_ASSERT(olctx->accept_fd == NETP_INVALID_SOCKET);
				NETP_ASSERT(olctx->fn_iocp_done != nullptr);
				ec = olctx->fn_iocp_done({ olctx->fd, ec == 0 ? (int)dwTrans : ec, 0 });
				if (ec == netp::E_CHANNEL_OVERLAPPED_OP_TRY && (olctx->action_status&AS_DONE) == 0 ) {
					ec = olctx->fn_overlapped((void*)(&olctx->overlapped));
					if (ec == netp::OK) {
						olctx->action_status |= AS_WAIT_IOCP;
					} else {
						olctx->fn_iocp_done({ olctx->fd, ec, 0 });
					}
				}
			}
			break;
			case iocp_ol_action::ACCEPTEX:
			{
				NETP_ASSERT(olctx->fd != NETP_INVALID_SOCKET);
				NETP_ASSERT(olctx->accept_fd != NETP_INVALID_SOCKET);
				NETP_ASSERT(olctx->fn_iocp_done != nullptr);

				if (ec == netp::OK) {
					ec = ::setsockopt(olctx->accept_fd, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char*)&olctx->fd, sizeof(olctx->fd));
				}

				if (NETP_LIKELY(ec == 0)) {
					NETP_TRACE_IOE("[iocp][#%u]accept done, new fd: %u", olctx->fd, olctx->accept_fd);
					ec = olctx->fn_iocp_done({ olctx->accept_fd,ec, olctx->buf });
				} else {
					NETP_CLOSE_SOCKET(olctx->accept_fd);
				}

				if (ec == netp::E_CHANNEL_OVERLAPPED_OP_TRY && (olctx->action_status & AS_DONE) == 0 ) {
					ec = _do_accept_ex(olctx);
					if (ec == netp::OK) {
						olctx->action_status |= AS_WAIT_IOCP;
					} else {
						olctx->fn_iocp_done({ NETP_INVALID_SOCKET, ec, 0 });
					}
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
				NETP_ASSERT(olctx->fn_iocp_done != nullptr);
				olctx->fn_iocp_done({ olctx->fd, ec == 0 ? (int)dwTrans : ec, 0 });
			}
			break;
			default:
			{
				NETP_THROW("unknown io event flag");
			}
			}
		}
	public:
		poller_iocp(poller_cfg const& cfg) :
			io_event_loop(T_IOCP, cfg),
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
			__LOOP_EXIT_WAITING__();

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
			iocp_overlapped_ctx* olctx=(CONTAINING_RECORD(ol, iocp_overlapped_ctx, overlapped));
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

			if (olctx->is_ch_end == CH_END_YES) {
				iocp_olctx_free(olctx);
			} else {
				_handle_iocp_event(olctx, len, ec);
			}
#endif
		}

		void _do_iocp_begin(SOCKET fd, fn_iocp_event_t const& fn_iocp) {
			NETP_TRACE_IOE("[#%d][CreateIoCompletionPort]init", fd);
			NETP_ASSERT(m_handle != nullptr);
			HANDLE bindcp = ::CreateIoCompletionPort((HANDLE)fd, m_handle, (u_long)fd, 0);
			if (bindcp == nullptr) {
				int ec = netp_socket_get_last_errno();
				fn_iocp({ fd,ec,0 });
				NETP_TRACE_IOE("[#%d][CreateIoCompletionPort]init error: %d", fd, ec);
				return;
			}

			iocp_ctx_map_t::iterator&& it = m_ctxs.find(fd);
			NETP_ASSERT(it == m_ctxs.end());
			NRP<iocp_ctx> iocpctx = netp::make_ref<iocp_ctx>();
			iocpctx->fd = fd;
			iocpctx->fn_notify = fn_iocp;

			iocp_overlapped_ctx* ol_r = iocp_olctx_malloc();
			NETP_ALLOC_CHECK(ol_r, sizeof(iocp_overlapped_ctx));
			iocpctx->ol_ctxs[READ] = ol_r;
			ol_r->fd = fd;
			ol_r->action_status = 0;
			ol_r->is_ch_end = CH_END_NO;

			iocp_overlapped_ctx* ol_w = iocp_olctx_malloc();
			NETP_ALLOC_CHECK(ol_w, sizeof(iocp_overlapped_ctx));
			iocpctx->ol_ctxs[WRITE] = ol_w;
			ol_w->fd = fd;
			ol_w->action_status = 0;
			ol_r->is_ch_end = CH_END_NO;

			m_ctxs.insert({ fd, iocpctx });
			fn_iocp({ fd,netp::OK, 0 } );
			NETP_TRACE_IOE("[#%d][_do_iocp_begin]", fd);
		}

		void _do_iocp_end(SOCKET fd, fn_iocp_event_t const& fn) {
			iocp_ctx_map_t::iterator&& it = m_ctxs.find(fd);
			NETP_ASSERT(it != m_ctxs.end());
			NRP<iocp_ctx>& iocpctx = it->second;
			
			iocpctx->ol_ctxs[iocp_ol_type::READ]->fn_overlapped = nullptr;
			iocpctx->ol_ctxs[iocp_ol_type::READ]->fn_iocp_done = nullptr;
			iocpctx->ol_ctxs[iocp_ol_type::READ]->action_status |= AS_DONE;
			iocpctx->ol_ctxs[iocp_ol_type::READ]->is_ch_end = CH_END_YES;
			if ((iocpctx->ol_ctxs[iocp_ol_type::READ]->action_status & AS_WAIT_IOCP) == 0){
				iocp_olctx_free(iocpctx->ol_ctxs[iocp_ol_type::READ]);
			}
			iocpctx->ol_ctxs[iocp_ol_type::WRITE]->fn_overlapped = nullptr;
			iocpctx->ol_ctxs[iocp_ol_type::WRITE]->fn_iocp_done = nullptr;
			iocpctx->ol_ctxs[iocp_ol_type::WRITE]->action_status |= AS_DONE;
			iocpctx->ol_ctxs[iocp_ol_type::WRITE]->is_ch_end = CH_END_YES;
			if ((iocpctx->ol_ctxs[iocp_ol_type::WRITE]->action_status & AS_WAIT_IOCP) == 0) {
				iocp_olctx_free(iocpctx->ol_ctxs[iocp_ol_type::WRITE]);
			}

			iocpctx->fn_notify = nullptr;
			m_ctxs.erase(it);
			fn({ fd,0,0 });
			NETP_TRACE_IOE("[#%d][_do_iocp_end]", fd);
		}

		void __do_execute_act() override {
			std::size_t vecs = m_iocp_acts.size();
			if (vecs == 0) {
				return;
			}
			std::size_t acti = 0;
			while (acti < m_iocp_acts.size()) {
				iocp_act_op& actop = m_iocp_acts[acti++];
				iocp_action& act = actop.act;
				SOCKET& fd = actop.fd;
				fn_overlapped_io_event& fn_overlapped=actop.fn_overlapped;
				fn_iocp_event_t& fn_iocp =actop.fn_iocp;

				switch (act) {
				case iocp_action::BEGIN:
				{
					_do_iocp_begin(fd, fn_iocp);
				}
				break;
				case iocp_action::END:
				{
					_do_iocp_end(fd, fn_iocp);
				}
				break;
				case iocp_action::READ:
				{
					iocp_ctx_map_t::iterator&& it = m_ctxs.find(fd);
					NETP_ASSERT(it != m_ctxs.end());
					NRP<iocp_ctx>& iocpctx = it->second;

					iocp_overlapped_ctx* olctx = iocpctx->ol_ctxs[iocp_ol_type::READ];
					NETP_ASSERT(olctx != nullptr);
					NETP_ASSERT(olctx->is_ch_end == 0);
					NETP_ASSERT((olctx->action_status&AS_WAIT_IOCP) == 0);

					int ec;
					if (olctx->accept_fd != NETP_INVALID_SOCKET) {
						//deliver pending data first
						int len = olctx->accept_fd;
						olctx->accept_fd = NETP_INVALID_SOCKET;
						ec = fn_iocp({olctx->fd, len, olctx->buf});
						if (ec != netp::E_CHANNEL_OVERLAPPED_OP_TRY) {
							//cancel read
							return;
						}
					}

					olctx->action = iocp_ol_action::WSAREAD;
					olctx->fn_iocp_done = fn_iocp;
					ec = _do_read(olctx);
					if (ec == netp::OK) {
						olctx->action_status &= ~AS_DONE;
						olctx->action_status |= AS_WAIT_IOCP;
					} else {
						olctx->fn_iocp_done({ olctx->fd, ec, 0 });
					}
				}
				break;
				case iocp_action::END_READ:
				{
					iocp_ctx_map_t::iterator&& it = m_ctxs.find(fd);
					NRP<iocp_ctx>& iocpctx = it->second;
					NETP_ASSERT(iocpctx != nullptr);
					iocpctx->ol_ctxs[iocp_ol_type::READ]->action_status |= AS_DONE;
				}
				break;
				case iocp_action::WRITE:
				{
					iocp_ctx_map_t::iterator&& it = m_ctxs.find(fd);
					NETP_ASSERT(it != m_ctxs.end());
					NRP<iocp_ctx>& iocpctx = it->second;
					NETP_ASSERT(iocpctx != nullptr);

					iocp_overlapped_ctx* olctx = iocpctx->ol_ctxs[iocp_ol_type::WRITE];
					NETP_ASSERT(olctx != nullptr);
					NETP_ASSERT((olctx->action_status & AS_WAIT_IOCP) == 0);
					olctx->action = iocp_ol_action::WSASEND;
					olctx->fn_iocp_done = fn_iocp;
					olctx->fn_overlapped = fn_overlapped;

					iocp_olctx_reset_overlapped(olctx);
					int wrt = fn_overlapped((void*)&olctx->overlapped);
					if (wrt == netp::OK) {
						olctx->action_status &= ~AS_DONE;
						olctx->action_status |= AS_WAIT_IOCP;
					} else {
						olctx->fn_iocp_done({ fd, wrt, 0 });
					}
				}
				break;
				case iocp_action::END_WRITE:
				{
					iocp_ctx_map_t::iterator&& it = m_ctxs.find(fd);
					NETP_ASSERT(it != m_ctxs.end());
					NRP<iocp_ctx>& iocpctx = it->second;
					NETP_ASSERT(iocpctx != nullptr);

					iocp_overlapped_ctx* olctx = iocpctx->ol_ctxs[iocp_ol_type::WRITE];
					olctx->action_status |= AS_DONE;
				}
				break;
				case iocp_action::ACCEPT:
				{
					iocp_ctx_map_t::iterator&& it = m_ctxs.find(fd);
					NETP_ASSERT(it != m_ctxs.end());
					NRP<iocp_ctx>& iocpctx = it->second;
					NETP_ASSERT(iocpctx != nullptr);

					iocp_overlapped_ctx* olctx = iocpctx->ol_ctxs[iocp_ol_type::READ];
					NETP_ASSERT(olctx != nullptr);
					NETP_ASSERT((olctx->action_status & AS_WAIT_IOCP) == 0);

					olctx->fn_iocp_done = fn_iocp;
					olctx->fn_overlapped = fn_overlapped;
					olctx->action = iocp_ol_action::ACCEPTEX;
					iocp_olctx_reset_overlapped(olctx);
					int ec = _do_accept_ex(olctx);
					if (ec == netp::OK) {
						olctx->action_status &= ~AS_DONE;
						olctx->action_status |= AS_WAIT_IOCP;
					} else {
						olctx->fn_iocp_done({ NETP_INVALID_SOCKET, ec, nullptr });
					}
				}
				break;
				case iocp_action::END_ACCEPT:
				{
					iocp_ctx_map_t::iterator&& it = m_ctxs.find(fd);
					NETP_ASSERT(it != m_ctxs.end());
					NRP<iocp_ctx>& iocpctx = it->second;
					NETP_ASSERT(iocpctx != nullptr);

					iocp_overlapped_ctx* olctx = iocpctx->ol_ctxs[iocp_ol_type::READ];
					NETP_ASSERT(olctx != nullptr);
					olctx->action_status |= AS_DONE;
				}
				break;
				case iocp_action::CONNECT:
				{
					iocp_ctx_map_t::iterator&& it = m_ctxs.find(fd);
					NETP_ASSERT(it != m_ctxs.end());
					NRP<iocp_ctx>& iocpctx = it->second;
					NETP_ASSERT(iocpctx != nullptr);

					iocp_overlapped_ctx* olctx = iocpctx->ol_ctxs[iocp_ol_type::WRITE];
					NETP_ASSERT(olctx != nullptr);
					NETP_ASSERT((olctx->action_status& AS_WAIT_IOCP) == 0);

					iocp_olctx_reset_overlapped(olctx);
					olctx->action = iocp_ol_action::CONNECTEX;
					olctx->fn_iocp_done = fn_iocp;

					const int crt = fn_overlapped((void*)&olctx->overlapped);
					if (crt == netp::OK) {
						olctx->action_status |= AS_WAIT_IOCP;
					} else {
						fn_iocp({ olctx->fd, (int)(crt & 0xFFFFFFFF), 0 });
					}
				}
				break;
				case iocp_action::END_CONNECT:
				{
					iocp_ctx_map_t::iterator&& it = m_ctxs.find(fd);
					NETP_ASSERT(it != m_ctxs.end());
					NRP<iocp_ctx>& iocpctx = it->second;
					NETP_ASSERT(iocpctx != nullptr);
					iocp_overlapped_ctx* olctx = iocpctx->ol_ctxs[iocp_ol_type::WRITE];
					NETP_ASSERT(olctx != nullptr);
					olctx->action_status |= AS_DONE;
				}
				break;
				case iocp_action::NOTIFY_TERMINATING:
				{
#ifdef NETP_DEBUG_TERMINATING
					NETP_ASSERT(m_terminated == false);
					m_terminated = true;
#endif
					iocp_ctx_map_t::iterator&& it = m_ctxs.begin();
					while (it != m_ctxs.end()) {
						NRP<iocp_ctx>& iocpctx = (it++)->second;
						NETP_ASSERT(iocpctx != nullptr);
						NETP_ASSERT(iocpctx->fn_notify != nullptr);
						iocpctx->fn_notify({ iocpctx->fd, netp::E_IO_EVENT_LOOP_NOTIFY_TERMINATING,0 });
					}
					//no competitor here, store directly
					NETP_ASSERT(m_state.load(std::memory_order_acquire) == u8_t(loop_state::S_TERMINATING));
					m_state.store(u8_t(loop_state::S_TERMINATED), std::memory_order_release);

					NETP_ASSERT(m_tb != nullptr);
					m_tb->expire_all();
				}
				break;
				}
			}
			m_iocp_acts.clear();
			if (vecs > 8192) {
				iocp_act_op_queue_t().swap(m_iocp_acts);
			}
		}

		int _do_watch(SOCKET , u8_t , NRP<watch_ctx> const& )  override {
			NETP_ASSERT(!"what: wrong call");
			return netp::OK;
		}
		int _do_unwatch(SOCKET , u8_t , NRP<watch_ctx> const& ) override {
			NETP_ASSERT(!"what: wrong call");
			return netp::OK;
		}
	};
}

#endif //NETP_HAS_POLLER_IOCP

#endif