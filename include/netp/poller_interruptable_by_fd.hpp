#ifndef _NETP_POLLER_INTERRUPTABLE_BY_FD_HPP
#define _NETP_POLLER_INTERRUPTABLE_BY_FD_HPP

#include <netp/list.hpp>
#include <netp/smart_ptr.hpp>
#include <netp/address.hpp>
#include <netp/poller_abstract.hpp>
#include <netp/io_monitor.hpp>
#include <netp/socket_api.hpp>

namespace netp {
	struct interrupt_fd_monitor final: 
		public io_monitor
	{
		SOCKET fd;
		io_ctx* ctx;
		interrupt_fd_monitor(SOCKET fd_):
			fd(fd_),
			ctx(0)
		{}

		virtual void io_notify_terminating(int, io_ctx*) override {
		}
		virtual void io_notify_read(int status, io_ctx*) override {
			if (status == netp::OK) {
				byte_t tmp[8] = {0};
				int ec = netp::OK;
				do {
#ifdef NETP_HAS_PIPE
					u32_t c = ::read(ctx->fd, tmp, 8);
					ec = netp_socket_get_last_errno();
#else
					u32_t c = netp::recv(ctx->fd, tmp, 8, ec, 0);
					//if (c == 1) {
					//	NETP_ASSERT(ec == netp::OK);
					//	NETP_ASSERT(tmp[0] == 'i', "c: %d", tmp[0]);
					//}
					(void)c;
#endif
				} while (ec == netp::OK);
			}
		}
		virtual void io_notify_write(int , io_ctx* ) override {
		}
	};

	class poller_interruptable_by_fd :
		public netp::poller_abstract
	{
	public:
		io_ctx m_io_ctx_list;
		NRP<interrupt_fd_monitor> m_fd_monitor_r;
		SOCKET m_fd_w;

#ifdef NETP_DEBUG_IO_CTX_
		long m_io_ctx_count_alloc;
		long m_io_ctx_count_free;
#endif

		poller_interruptable_by_fd() :
			poller_abstract(),
			m_fd_monitor_r(nullptr),
			m_fd_w(NETP_INVALID_SOCKET)
#ifdef NETP_DEBUG_IO_CTX_
			,m_io_ctx_count_alloc(0),
			m_io_ctx_count_free(0)
#endif
		{}

		~poller_interruptable_by_fd() {}

		void __init_interrupt_fd() {
			int rt;
			SOCKET fds[2] = {NETP_INVALID_SOCKET, NETP_INVALID_SOCKET};
#ifdef NETP_HAS_PIPE
			while (pipe(fds) == -1) {
				netp::this_thread::yield();
			}
#else
			rt = netp::socketpair(int(NETP_AF_INET), int(NETP_SOCK_STREAM), int(NETP_PROTOCOL_TCP), fds);
			NETP_ASSERT(rt == netp::OK, "rt: %d", rt);
			rt = netp::set_nodelay(fds[1], true);
			NETP_ASSERT(rt == netp::OK, "rt: %d", rt);
#endif
			NETP_VERBOSE("[poller_interruptable_by_fd]init pipe done, fds[r]: %u, fds[w]: %u", fds[0], fds[1]);

			rt = netp::set_nonblocking(fds[0],true);
			NETP_ASSERT(rt == netp::OK, "rt: %d", rt);

			rt = netp::set_nonblocking(fds[1],true);
			NETP_ASSERT(rt == netp::OK, "rt: %d", rt);

			m_fd_monitor_r = netp::make_ref<interrupt_fd_monitor>(fds[0]);
			io_ctx* ctx = io_begin(fds[0], m_fd_monitor_r);
			NETP_ASSERT(ctx != 0);
			rt = io_do(io_action::READ, ctx);
			NETP_ASSERT(rt == netp::OK, "fd: %d, rt: %d, errno: %d", ctx->fd, rt, netp_socket_get_last_errno() );
			m_fd_monitor_r->ctx = ctx;
			m_fd_w = fds[1];
			NETP_VERBOSE("[poller_interruptable_by_fd]__init_interrupt_fd done, fds[r]: %u, fds[w]: %u, fd_r: %u, m_fd_w: %u", fds[0], fds[1], m_fd_monitor_r->fd, m_fd_w);
		}

		void __deinit_interrupt_fd() {
			NETP_VERBOSE("[poller_interruptable_by_fd]__deinit_interrupt_fd begin, fd_r: %u, m_fd_w: %u", m_fd_monitor_r->fd, m_fd_w);
			io_do(io_action::END_READ, m_fd_monitor_r->ctx);
			io_end(m_fd_monitor_r->ctx);

			netp::close(m_fd_monitor_r->fd);
			netp::close(m_fd_w);
			m_fd_monitor_r->fd = (SOCKET)NETP_INVALID_SOCKET;
			m_fd_w = (SOCKET)NETP_INVALID_SOCKET;
			NETP_VERBOSE("[poller_interruptable_by_fd]__deinit_interrupt_fd done");

			//NETP_ASSERT(m_ctxs.size() == 0);
			NETP_ASSERT(NETP_LIST_IS_EMPTY(&m_io_ctx_list), "m_io_ctx_list not empty");
		}

		void init() {
			netp::list_init(&m_io_ctx_list);
#ifdef NETP_DEBUG_IO_CTX_
			m_io_ctx_count_alloc = 0;
			m_io_ctx_count_free = 0;
#endif
			__init_interrupt_fd();
		}

		void deinit() {
			__deinit_interrupt_fd();
#ifdef NETP_DEBUG_IO_CTX_
			NETP_ASSERT(m_io_ctx_count_alloc == m_io_ctx_count_free);
#endif
		}

		virtual void interrupt_wait() override {
			int ec;
#ifdef NETP_HAS_PIPE
			do {
				char buf = 1;
				int c = write(m_fd_w, &buf, 1);
				if (c == 1) {
					break;
				}
				int ec = netp_socket_get_last_errno();
				if (ec == netp::E_EINTR) {
					continue;
				}
				NETP_WARN("[poller_interruptable_by_fd][##%u]interrupt pipe failed: %d", m_fd_w, ec);
		} while (1);
#else
			const byte_t interrutp_a[1] = { (byte_t)'i' };
			u32_t c = netp::send(m_fd_w, interrutp_a, 1, ec, 0);
			if (NETP_UNLIKELY(ec != netp::OK)) {
				NETP_WARN("[poller_interruptable_by_fd][##u]interrupt send failed: %d", m_fd_w, ec);
			}
			(void)c;
#endif
		}

		virtual io_ctx* io_begin(SOCKET fd, NRP<io_monitor> const& iom) override {
			io_ctx* ctx = netp::io_ctx_allocate(fd,iom);
			netp::list_append(&m_io_ctx_list, ctx);

#ifdef NETP_DEBUG_IO_CTX_
			++m_io_ctx_count_alloc;
#endif
			return ctx;
		}

		//
		virtual void io_end(io_ctx* ctx) override {
			netp::list_delete(ctx);
			netp::io_ctx_deallocate(ctx);

#ifdef NETP_DEBUG_IO_CTX_
			++m_io_ctx_count_free;
#endif
		}

		virtual int io_do(io_action act, io_ctx* ctx) override {
			switch (act) {
			case io_action::READ:
			{
				NETP_TRACE_IOE("[poller_interruptable_by_fd][#%d]io_action::READ", ctx->fd);
				NETP_ASSERT((ctx->flag & io_flag::IO_READ) == 0);
				int rt = watch(io_flag::IO_READ, ctx);
				if (netp::OK == rt) {
					ctx->flag |= io_flag::IO_READ;
				}
				return rt;
			}
			break;
			case io_action::END_READ:
			{
				NETP_TRACE_IOE("[poller_interruptable_by_fd][type:%d][#%d]io_action::END_READ", ctx->fd);
				if (ctx->flag & io_flag::IO_READ) {
					ctx->flag &= ~io_flag::IO_READ;
					//we need this condition check ,cuz epoll might fail to watch
					return unwatch(io_flag::IO_READ, ctx);
				}
				return netp::OK;
			}
			break;
			case io_action::WRITE:
			{
				NETP_TRACE_IOE("[poller_interruptable_by_fd][type:%d][#%d]io_action::WRITE", ctx->fd);
				NETP_ASSERT((ctx->flag & io_flag::IO_WRITE) == 0);
				int rt = watch(io_flag::IO_WRITE, ctx);
				if (netp::OK == rt) {
					ctx->flag |= io_flag::IO_WRITE;
				}
				return rt;
			}
			break;
			case io_action::END_WRITE:
			{
				NETP_TRACE_IOE("[poller_interruptable_by_fd][type:%d][#%d]io_action::END_WRITE", ctx->fd);
				if (ctx->flag & io_flag::IO_WRITE) {
					ctx->flag &= ~io_flag::IO_WRITE;
					//we need this condition check ,cuz epoll might fail to watch
					return unwatch(io_flag::IO_WRITE, ctx);
				}
				return netp::OK;
			}
			break;
			case io_action::NOTIFY_TERMINATING:
			{
				NETP_VERBOSE("[poller_interruptable_by_fd]notify terminating...");
				io_ctx* _ctx, * _ctx_n;
				for (_ctx = (m_io_ctx_list.next), _ctx_n = _ctx->next; _ctx != &(m_io_ctx_list); _ctx = _ctx_n, _ctx_n = _ctx->next) {
					if (_ctx->fd == m_fd_monitor_r->fd) {
						continue;
					}

					NETP_ASSERT(_ctx->fd > 0);
					NETP_ASSERT(_ctx->iom != nullptr);

					_ctx->iom->io_notify_terminating(E_IO_EVENT_LOOP_NOTIFY_TERMINATING, _ctx);
				}
				NETP_VERBOSE("[poller_interruptable_by_fd]notify terminating done");
			}
			break;
			case io_action::READ_WRITE:
			{//for compiler warning...
			}
			break;
			}
			return netp::OK;
		}
	};
}

#endif