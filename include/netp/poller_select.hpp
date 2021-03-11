#ifndef  _NETP_SELECT_POLLER_HPP_
#define _NETP_SELECT_POLLER_HPP_

#include <netp/core.hpp>
#include <netp/io_event_loop.hpp>
#include <netp/socket_api.hpp>

namespace netp {

	class poller_select final :
		public io_event_loop
	{
		enum fds_idx {
			fds_e = 0,
			fds_r = 1,
			fds_w = 2
		};
		fd_set m_fds[3];

	private:
		public:
			poller_select(poller_cfg const& cfg):
				io_event_loop(T_SELECT, cfg)
			{
			}

			~poller_select() {}

			int _do_watch(SOCKET, u8_t, NRP<watch_ctx> const&) override
			{
				return netp::OK;
			}

			int _do_unwatch(SOCKET,u8_t, NRP<watch_ctx> const&) override
			{
				return netp::OK;
			}

#ifdef _NETP_WIN
    #pragma warning(push)
    #pragma warning(disable:4389)
#endif
		void _do_poll(long long wait_in_nano) override {
			NETP_ASSERT(in_event_loop());

			timeval _tv = { 0,0 };
			timeval* tv = 0;
			if (wait_in_nano != ~0) {
				if (wait_in_nano>0) {
					_tv.tv_sec = static_cast<long>(wait_in_nano / 1000000000ULL);
					_tv.tv_usec = static_cast<long>(wait_in_nano % 1000000000ULL) / 1000;
				}
				tv = &_tv;
			}

#ifdef DEBUG
			netp::size_t total_ctxs = m_ctxs.size();
#endif
			FD_ZERO(&m_fds[fds_e]);
			FD_ZERO(&m_fds[fds_r]);
			FD_ZERO(&m_fds[fds_w]);

			SOCKET max_fd_v = (SOCKET)0;

			watch_ctx_map_t::iterator&& it = m_ctxs.begin();
			while( it != m_ctxs.end() ) {
				NRP<watch_ctx> const& ctx = (it++)->second ;
				for (int i = aio_flag::AIO_READ; i <aio_flag::AIO_FLAG_MAX; ++i) {
					if (ctx->iofn[i] != nullptr) {
#ifdef NETP_DEBUG_WATCH_CTX_FLAG
						NETP_ASSERT((ctx->flag&i), "fd: %d, flag: %u", ctx->fd, ctx->flag);
#endif
						FD_SET((ctx->fd), &m_fds[i]);
						if (2==i) {
							FD_SET((ctx->fd), &m_fds[fds_e]);
						}
						if (ctx->fd > max_fd_v) {
							max_fd_v = ctx->fd;
						}
					}
				}
			}

			int nready = ::select((int)(max_fd_v + 1), &m_fds[fds_r], &m_fds[fds_w], &m_fds[fds_e], tv); //only read now
			__LOOP_EXIT_WAITING__();

			if (nready == 0) {
				return;
			} else if (nready == -1) {
				//notice 10038
				NETP_ERR("[io_event_loop][select]select error, errno: %d", netp_socket_get_last_errno());
				return;
			}

#ifdef DEBUG
			NETP_ASSERT(total_ctxs == m_ctxs.size());
#endif
			it = m_ctxs.begin();
			while (it != m_ctxs.end() && (nready > 0)) {
				NRP<watch_ctx> ctx = (it++)->second;
				SOCKET const& fd = ctx->fd;
				int ec = netp::OK;

				if (FD_ISSET(fd, &m_fds[fds_e])) {
					//FD_CLR(fd, &m_fds[fds_e]);
					--nready;

					socklen_t optlen = sizeof(int);
					int getrt = ::getsockopt(fd, SOL_SOCKET, SO_ERROR, (char*)&ec, &optlen);
					if (getrt == -1) {
						ec = netp_socket_get_last_errno();
					} else {
						ec = NETP_NEGATIVE(ec);
					}
					NETP_DEBUG("[select]socket getsockopt failed, fd: %d, errno: %d", fd, ec);
					NETP_ASSERT(ec != netp::OK);
				}

				for (int i = aio_flag::AIO_READ; i < aio_flag::AIO_FLAG_MAX; ++i) {
					if (FD_ISSET(fd, &m_fds[i])) {
						//FD_CLR(fd, &m_fds[i]);
						--nready;
#ifdef NETP_DEBUG_WATCH_CTX_FLAG
						NETP_ASSERT((ctx->flag & i), "fd: %d, flag: %u", ctx->fd, ctx->flag);
						NETP_ASSERT(ctx->iofn[i] != nullptr, "fd: %d, flag: %u", ctx->fd, ctx->flag);
#endif
						ctx->iofn[i](ec);
						continue;
					}
					ec != netp::OK && ctx->iofn[i] != nullptr ? ctx->iofn[i](ec):(void)0;
				}
			}
		}

#ifdef _NETP_WIN
    #pragma warning(pop)
#endif
	};
}
#endif//
