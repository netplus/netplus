#ifndef _NETP_POLLER_INTERRUPTABLE_BY_FD_HPP
#define _NETP_POLLER_INTERRUPTABLE_BY_FD_HPP

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
				byte_t tmp[1];
				int ec = netp::OK;
				do {
					u32_t c = netp::recv(netp::default_socket_api, ctx->fd, tmp, 1, ec, 0);
					if (c == 1) {
						NETP_ASSERT(ec == netp::OK);
						NETP_ASSERT(tmp[0] == 'i', "c: %d", tmp[0]);
					}
				} while (ec == netp::OK);
			}
		}
		virtual void io_notify_write(int , io_ctx* ) override {
		}
	};

	class poller_interruptable_by_fd :
		public poller_abstract
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
			SOCKET fds[2] = {NETP_INVALID_SOCKET, NETP_INVALID_SOCKET};
			int rt = netp::socketpair(int(NETP_AF_INET), int(NETP_SOCK_STREAM), int(NETP_PROTOCOL_TCP), fds);
			NETP_ASSERT(rt == netp::OK, "rt: %d", rt);

			rt = netp::turnon_nonblocking(netp::default_socket_api, fds[0]);
			NETP_ASSERT(rt == netp::OK, "rt: %d", rt);

			rt = netp::turnon_nonblocking(netp::default_socket_api, fds[1]);
			NETP_ASSERT(rt == netp::OK, "rt: %d", rt);

			rt = netp::turnon_nodelay(netp::default_socket_api, fds[1]);
			NETP_ASSERT(rt == netp::OK, "rt: %d", rt);

			m_fd_monitor_r = netp::make_ref<interrupt_fd_monitor>(fds[0]);
			io_ctx* ctx = io_begin(fds[0], m_fd_monitor_r);
			NETP_ASSERT(ctx != 0);
			rt = io_do(io_action::READ, ctx);
			NETP_ASSERT(rt == netp::OK);
			m_fd_monitor_r->ctx = ctx;
			m_fd_w = fds[1];
		}

		void __deinit_interrupt_fd() {
			io_do(io_action::END_READ, m_fd_monitor_r->ctx);
			io_end(m_fd_monitor_r->ctx);

			NETP_CLOSE_SOCKET(m_fd_monitor_r->fd);
			NETP_CLOSE_SOCKET(m_fd_w);
			m_fd_monitor_r->fd = (SOCKET)NETP_INVALID_SOCKET;
			m_fd_w = (SOCKET)NETP_INVALID_SOCKET;

			NETP_TRACE_IOE("[io_event_loop][default]deinit done");

			//NETP_ASSERT(m_ctxs.size() == 0);
			NETP_ASSERT(NETP_IO_CTX_LIST_IS_EMPTY(&m_io_ctx_list), "m_io_ctx_list not empty");
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
			NETP_ASSERT(m_fd_w > 0);
			int ec;
			const byte_t interrutp_a[1] = { (byte_t)'i' };
			u32_t c = netp::send(netp::default_socket_api, m_fd_w, interrutp_a, 1, ec, 0);
			if (NETP_UNLIKELY(ec != netp::OK)) {
				NETP_WARN("[io_event_loop]interrupt send failed: %d", ec);
			}
			(void)c;
		}

		virtual io_ctx* io_begin(SOCKET fd, NRP<io_monitor> const& iom) override {
			io_ctx* ctx = netp::io_ctx_allocate(fd,iom);
			netp::list_append(&m_io_ctx_list, ctx);

#ifdef NETP_DEBUG_IO_CTX_
			++m_io_ctx_count_alloc;
#endif
			return ctx;
		}

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
				NETP_TRACE_IOE("[io_event_loop][#%d]io_action::READ", ctx->fd);
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
				NETP_TRACE_IOE("[io_event_loop][type:%d][#%d]io_action::END_READ", ctx->fd);
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
				NETP_TRACE_IOE("[io_event_loop][type:%d][#%d]io_action::WRITE", ctx->fd);
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
				NETP_TRACE_IOE("[io_event_loop][type:%d][#%d]io_action::END_WRITE", ctx->fd);
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
				NETP_DEBUG("[io_event_loop]notify terminating...");
				io_ctx* _ctx, * _ctx_n;
				for (_ctx = (m_io_ctx_list.next), _ctx_n = _ctx->next; _ctx != &(m_io_ctx_list); _ctx = _ctx_n, _ctx_n = _ctx->next) {
					if (_ctx->fd == m_fd_monitor_r->fd) {
						continue;
					}

					NETP_ASSERT(_ctx->fd > 0);
					NETP_ASSERT(_ctx->iom != nullptr);

					NRP<io_monitor>& IOM = _ctx->iom;
					if (_ctx->flag & io_flag::IO_READ) {
						IOM->io_notify_read(E_IO_EVENT_LOOP_NOTIFY_TERMINATING, _ctx);
					}
					if (_ctx->flag & io_flag::IO_WRITE) {
						IOM->io_notify_write(E_IO_EVENT_LOOP_NOTIFY_TERMINATING, _ctx);
					}
					IOM->io_notify_terminating(E_IO_EVENT_LOOP_NOTIFY_TERMINATING, _ctx);
				}
				NETP_DEBUG("[io_event_loop]notify terminating done");
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