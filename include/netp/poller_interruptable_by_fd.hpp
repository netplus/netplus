#ifndef _NETP_POLLER_INTERRUPTABLE_BY_FD_HPP
#define _NETP_POLLER_INTERRUPTABLE_BY_FD_HPP

#include <netp/list.hpp>
#include <netp/smart_ptr.hpp>
#include <netp/address.hpp>
#include <netp/poller_abstract.hpp>
#include <netp/io_monitor.hpp>
#include <netp/socket_api.hpp>

#define _NETP_DEBUG_INTERRUPT_

namespace netp {
	struct fdinterrupt_monitor final:
		public io_monitor
	{
		SOCKET fdr;
		SOCKET fdw;
		io_ctx* ctx;
		std::atomic<bool> is_sigset;
		fdinterrupt_monitor(SOCKET fdr_, SOCKET fdw_):
			fdr(fdr_),
			fdw(fdw_),
			ctx(0),
			is_sigset(false)
		{}

		virtual void io_notify_terminating(int, io_ctx*) override {}
		virtual void io_notify_write(int, io_ctx*) override {}
		virtual void io_notify_read(int status, io_ctx*) override {
			if (status == netp::OK) {
#ifdef _NETP_DEBUG_INTERRUPT_
				NETP_ASSERT(is_sigset.load(std::memory_order_acquire));
				size_t nbytes = 0;
#endif
				byte_t tmp[4] = { 0 };
				int ec = netp::OK;

				do {
#ifdef NETP_HAS_PIPE
					ssize_t c = ::read(fdr, tmp, 4);
					if (c == ssize_t(-1)) {
						ec = netp_socket_get_last_errno();
						_NETP_REFIX_EWOULDBLOCK(ec);
					} else if (c == 0) {
						//Is the following case possible ? 
						//c ==0 && ec == E_EINTR
						ec = netp_socket_get_last_errno();
						if (ec == 0) {
							ec = netp::E_PIPE_CLOSED;
						}
					}
#else
					u32_t c = netp::recv(fdr, tmp, 4, ec, 0);
					(void)c;
#endif

#ifdef _NETP_DEBUG_INTERRUPT_
					nbytes += c;
				} while (ec == netp::OK);
#else
				} while (ec == netp::E_EINTR);
#endif

				is_sigset.store(false, std::memory_order_release);
#ifdef _NETP_DEBUG_INTERRUPT_
				NETP_ASSERT(nbytes <= 1, "nbytes: %u", nbytes );
#endif
			}
		}

		inline void interrupt_wait() {
			bool f = false;
			if (!is_sigset.compare_exchange_strong(f, true, std::memory_order_acq_rel, std::memory_order_acquire)) {
				//save one write
				return;
			}

			int ec;
			const char interrutp_i = 'i';
			#ifdef NETP_HAS_PIPE
			do {
				int c = ::write(fdw, (const void*)&interrutp_i, 1);
				if (c == 1) {
					break;
				}
				ec = netp_socket_get_last_errno();
				if (ec == netp::E_EINTR) { continue; }
				NETP_WARN("[fdinterrupt_monitor][##%u]interrupt pipe failed: %d", fdw, ec);
			} while (fdw != NETP_INVALID_SOCKET);
#else
			u32_t c = netp::send(fdw, (byte_t const* const)&interrutp_i, 1, ec, 0);
			(void)c;
			if (NETP_UNLIKELY(ec != netp::OK)) {
				NETP_WARN("[fdinterrupt_monitor][##u]interrupt send failed: %d", fdw, ec);
			}
#endif
		}
	};

	class poller_interruptable_by_fd :
		public netp::poller_abstract
	{
	public:
		io_ctx m_io_ctx_list;
		NRP<fdinterrupt_monitor> m_fdintr;

#ifdef NETP_DEBUG_IO_CTX_
		long m_io_ctx_count_alloc;
		long m_io_ctx_count_free;
#endif

		poller_interruptable_by_fd() :
			poller_abstract(),
			m_fdintr(nullptr)
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

			m_fdintr = netp::make_ref<fdinterrupt_monitor>(fds[0],fds[1]);
			io_ctx* ctx = io_begin(fds[0], m_fdintr);
			NETP_ASSERT(ctx != 0);
			rt = io_do(io_action::READ, ctx);
			NETP_ASSERT(rt == netp::OK, "fd: %d, rt: %d, errno: %d", ctx->fd, rt, netp_socket_get_last_errno() );
			m_fdintr->ctx = ctx;
			NETP_VERBOSE("[poller_interruptable_by_fd]__init_interrupt_fd done, fd_r: %u, m_fd_w: %u", fds[0], fds[1]);
		}

		void __deinit_interrupt_fd() {
			NETP_VERBOSE("[poller_interruptable_by_fd]__deinit_interrupt_fd begin, fd_r: %u, m_fd_w: %u", m_fdintr->fdr, m_fdintr->fdw);
			io_do(io_action::END_READ, m_fdintr->ctx);
			netp::close(m_fdintr->fdr);
			netp::close(m_fdintr->fdw);
			m_fdintr->fdr = NETP_INVALID_SOCKET;
			m_fdintr->fdw = NETP_INVALID_SOCKET;
			io_end(m_fdintr->ctx);
			m_fdintr->ctx = nullptr;

			NETP_VERBOSE("[poller_interruptable_by_fd]__deinit_interrupt_fd done");
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
			m_fdintr->interrupt_wait();
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
#ifdef _NETP_DEBUG
				NETP_ASSERT((ctx->flag & io_flag::IO_READ) == 0);
#endif
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
#ifdef _NETP_DEBUG
				NETP_ASSERT((ctx->flag & io_flag::IO_WRITE) == 0);
#endif
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
					NETP_ASSERT((_ctx->fd > 0) && (_ctx->iom != nullptr));
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