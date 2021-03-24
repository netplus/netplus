#ifndef  _NETP_SELECT_POLLER_HPP_
#define _NETP_SELECT_POLLER_HPP_

#include <netp/core.hpp>
#include <netp/socket_api.hpp>
#include <netp/poller_abstract.hpp>

namespace netp {

	class poller_select final :
		public poller_interruptable_by_fd
	{
		enum fds_idx {
			fds_e = 0,
			fds_r = 1,
			fds_w = 2
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

			FD_ZERO(&m_fds[fds_e]);
			FD_ZERO(&m_fds[fds_r]);
			FD_ZERO(&m_fds[fds_w]);

			SOCKET max_fd_v = (SOCKET)0;
			io_ctx* ctx;
			for (ctx = m_io_ctx_list.next; ctx != &m_io_ctx_list; ctx = ctx->next) {
				for (int i = io_flag::IO_READ; i < io_flag::IO_FLAG_MAX; ++i) {
					if (ctx->flag&i) {
						FD_SET((ctx->fd), &m_fds[i]);
						if (2 == i) {
							FD_SET((ctx->fd), &m_fds[fds_e]);
						}
						if (ctx->fd > max_fd_v) {
							max_fd_v = ctx->fd;
						}
					}
				}
			}

			int nready = ::select((int)(max_fd_v + 1), &m_fds[fds_r], &m_fds[fds_w], &m_fds[fds_e], tv); //only read now
			__LOOP_EXIT_WAITING__(W);

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
				if (FD_ISSET(ctx->fd, &m_fds[IO_READ])) {
					//FD_CLR(fd, &m_fds[i]);
					--nready;

					NETP_ASSERT(ctx->fn_read != nullptr);
					ctx->fn_read(status, ctx);
					continue;
				}

				if (FD_ISSET(ctx->fd, &m_fds[IO_WRITE])) {
					//FD_CLR(fd, &m_fds[i]);
 					--nready;
					//fn_read might result in fn_write be reset
					if(ctx->fn_write != nullptr) ctx->fn_write(status, ctx);
					continue;
				}

				if (status != netp::OK) {
					if(ctx->fn_read != nullptr)  ctx->fn_read(status, ctx) ;
					if(ctx->fn_write != nullptr ) ctx->fn_write(status, ctx) ;
				}
			}
		}

#ifdef _NETP_WIN
    #pragma warning(pop)
#endif
	};
}
#endif//