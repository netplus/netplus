#ifndef  _NETP_SELECT_POLLER_HPP_
#define _NETP_SELECT_POLLER_HPP_

#include <netp/core.hpp>
#include <netp/socket_api.hpp>
#include <netp/poller_interruptable_by_fd.hpp>

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

	private:
		public:
			poller_select():
				poller_interruptable_by_fd()
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

#ifdef _NETP_WIN
    #pragma warning(push)
    #pragma warning(disable:4389)
#endif
		void poll(long long wait_in_nano, std::atomic<bool>& W) override {
			timeval _tv = { 0,0 };
			timeval* tv = 0;
			if (wait_in_nano != ~0) {
				if (wait_in_nano>0) {
					_tv.tv_sec = static_cast<long>(wait_in_nano / 1000000000ULL);
					_tv.tv_usec = static_cast<long>(wait_in_nano % 1000000000ULL) / 1000;
				}
				tv = &_tv;
			}

			FD_ZERO(&m_fds[fds_r]);
			FD_ZERO(&m_fds[fds_w]);
			FD_ZERO(&m_fds[fds_e]);

			SOCKET max_fd_v = (SOCKET)0;
			io_ctx* ctx;
			for (ctx = m_io_ctx_list.next; ctx != &m_io_ctx_list; ctx = ctx->next) {
				if (ctx->flag&io_flag::IO_READ) { 
					FD_SET((ctx->fd), &m_fds[fds_r]);
					if (ctx->fd > max_fd_v) {
						max_fd_v = ctx->fd;
					}
				}
				if (ctx->flag&io_flag::IO_WRITE) {
					FD_SET((ctx->fd), &m_fds[fds_w]);
					FD_SET((ctx->fd), &m_fds[fds_e]);
					if (ctx->fd > max_fd_v) {
						max_fd_v = ctx->fd;
					}
				}
			}

			int nready = ::select((int)(max_fd_v + 1), &m_fds[fds_r], &m_fds[fds_w], &m_fds[fds_e], tv); //only read now
			NETP_POLLER_WAIT_EXIT(wait_in_nano,W);

			if (nready == 0) {
				return;
			} else if (nready == -1) {
				//notice 10038
				NETP_ERR("[io_event_loop][select]select error, errno: %d", netp_socket_get_last_errno());
				return;
			}

			io_ctx* ctx_n;
			for (ctx = (m_io_ctx_list.next), ctx_n = ctx->next; ctx != &m_io_ctx_list && nready>0; ctx = ctx_n, ctx_n = ctx->next) {
				int status = netp::OK;
				//it's safe to do reference, cuz io_end would be scheduled on the next loop
				NRP<io_monitor>& IOM = ctx->iom;
				if (FD_ISSET(ctx->fd, &m_fds[fds_e])) {
					//FD_CLR(fd, &m_fds[fds_e]);
					--nready;

					socklen_t optlen = sizeof(int);
					int getrt = ::getsockopt(ctx->fd, SOL_SOCKET, SO_ERROR, (char*)&status, &optlen);
					if (getrt == -1) {
						status = netp_socket_get_last_errno();
					} else {
						status = NETP_NEGATIVE(status);
					}
					NETP_DEBUG("[select]socket getsockopt failed, fd: %d, errno: %d", ctx->fd, status);
					NETP_ASSERT(status != netp::OK);
				}
				if (FD_ISSET(ctx->fd, &m_fds[fds_r])) {
					//FD_CLR(fd, &m_fds[i]);
					--nready;

					NETP_ASSERT(ctx->flag&io_flag::IO_READ);
					IOM->io_notify_read(status, ctx);
					continue;
				}

				if (FD_ISSET(ctx->fd, &m_fds[fds_w])) {
					//FD_CLR(fd, &m_fds[i]);
 					--nready;
					//fn_read might result in fn_write be reset
					if(ctx->flag&io_flag::IO_WRITE) IOM->io_notify_write(status, ctx);
					continue;
				}

				if (status != netp::OK) {
					if(ctx->flag&io_flag::IO_READ) IOM->io_notify_read(status, ctx);
					if(ctx->flag&io_flag::IO_WRITE ) IOM->io_notify_write(status, ctx) ;
				}
			}
		}

#ifdef _NETP_WIN
    #pragma warning(pop)
#endif
	};
}
#endif//