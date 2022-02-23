#ifndef  _NETP_SELECT_POLLER_HPP_
#define _NETP_SELECT_POLLER_HPP_

#include <netp/core.hpp>
#include <netp/socket_api.hpp>
#include <netp/poller_interruptable_by_fd.hpp>
#include <netp/benchmark.hpp>

namespace netp {

	class poller_select final :
		public poller_interruptable_by_fd
	{
		enum fds_idx {
			fds_r = 0,
			fds_w = 1,
			fds_e = 2
		};
		fd_set m_fds[3];
		bool m_polling;
	private:
		public:
			poller_select():
				poller_interruptable_by_fd(io_poller_type::T_SELECT),
				m_polling(false)
			{
			}

			~poller_select() {}

			int watch(u8_t, io_ctx*) override
			{
				return netp::OK;
			}

			int unwatch(u8_t, io_ctx*) override
			{
				return netp::OK;
			}

			//@note: ctx be removed right after the loop of close(fd)
			// edge case a:
			//for the following case:
			//1) for_each(cur_ctx: ctx_list) (
			//2) cur_ctx->fd poll error 
			//3) close(cur_ctx->fd), schedule uninstall for cur_ctx
			//4) nfd = socket(), append to new ctx list tail
			//5) cur_ctx->fd == nfd, FD_ISSET(fd, read_fd_list) (we get here: if this line happens before ch_io_read(new ctx)

			//the solution for edge case a is skip the pending ctx
			//1) add a pending flag if in polling state
			//2) remove this flag at the polling beginning
			//3) check flag for evt

			virtual io_ctx* io_begin(SOCKET fd, NRP<io_monitor> const& iom) override {
				io_ctx* ctx = poller_interruptable_by_fd::io_begin(fd, iom);
				if (m_polling) {
					ctx->flag |= io_flag::IO_ADD_PENDING;
				}
				return ctx;
			}

#ifdef _NETP_WIN
    #pragma warning(push)
    #pragma warning(disable:4389)
#endif
			void poll(i64_t wait_in_nano, std::atomic<bool>& W) override {
				FD_ZERO(&m_fds[fds_r]);
				FD_ZERO(&m_fds[fds_w]);
				FD_ZERO(&m_fds[fds_e]);

				SOCKET max_fd_v = (SOCKET)0;
				io_ctx* ctx;
				m_polling = true;
				for (ctx = m_io_ctx_list.next; ctx != &m_io_ctx_list; ctx = ctx->next) {
					ctx->flag &= ~io_flag::IO_ADD_PENDING;
					if (ctx->flag & io_flag::IO_READ) {
						FD_SET((ctx->fd), &m_fds[fds_r]);
						if (ctx->fd > max_fd_v) {
							max_fd_v = ctx->fd;
						}
					}
					if (ctx->flag & io_flag::IO_WRITE) {
						FD_SET((ctx->fd), &m_fds[fds_w]);
						FD_SET((ctx->fd), &m_fds[fds_e]);
						if (ctx->fd > max_fd_v) {
							max_fd_v = ctx->fd;
						}
					}
				}

			timeval _tv = { static_cast<long>(wait_in_nano / 1000000000ULL), static_cast<long>(wait_in_nano % 1000000000ULL) / 1000 };
			timeval* tv = wait_in_nano != ~0 ? &_tv : 0 ;
			int nready = ::select((int)(max_fd_v + 1), &m_fds[fds_r], &m_fds[fds_w], &m_fds[fds_e], tv); //only read now
			NETP_POLLER_WAIT_EXIT(wait_in_nano, W);

			if (nready <= 0 ) {
				m_polling = false;
				const int ec = netp_socket_get_last_errno();
				if (ec != 0) {
					NETP_ERR("[event_loop][select]select error, errno: %d", netp_socket_get_last_errno());
				}
				return;
			}

			io_ctx* ctx_n;
			for (ctx = (m_io_ctx_list.next), ctx_n = ctx->next; ctx != &m_io_ctx_list && nready>0; ctx = ctx_n, ctx_n = ctx->next) {
				if (ctx->flag&io_flag::IO_ADD_PENDING) {
					//newly added 
					continue;
				}
				bool hit = false;
				int status = netp::OK;
				//it's safe to do reference, cuz io_end would be scheduled on the next loop
				NRP<io_monitor>& IOM = ctx->iom;
				if (FD_ISSET(ctx->fd, &m_fds[fds_e])) {
					socklen_t optlen = sizeof(int);
					int getrt = ::getsockopt(ctx->fd, SOL_SOCKET, SO_ERROR, (char*)&status, &optlen);
					if (getrt == -1) {
						status = netp_socket_get_last_errno();
						NETP_VERBOSE("[select]socket getsockopt failed, fd: %d, errno: %d", ctx->fd, status);
					} else {
						status = NETP_NEGATIVE(status);
					}
					if (status == netp::OK) { status = netp::E_SOCKET_SELECT_EXCEPT; }
					//status = netp::E_SOCKET_SELECT_EXCEPT;
				}

				if ( (ctx->flag&io_flag::IO_READ) && ((status != netp::OK) || FD_ISSET(ctx->fd, &m_fds[fds_r])) ) {
					hit = true;
					IOM->io_notify_read(status, ctx);
				}
				if ( (ctx->flag&io_flag::IO_WRITE) && ((status != netp::OK) ||FD_ISSET(ctx->fd, &m_fds[fds_w])) ) {
					//fn_read might result in fn_write be reset
					hit = true;
					IOM->io_notify_write(status, ctx);
				}
				if (hit) { --nready; }
			}
			m_polling = false;
		}

#ifdef _NETP_WIN
    #pragma warning(pop)
#endif
	};
}
#endif//