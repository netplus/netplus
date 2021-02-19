#ifndef _NETP_EPOLL_POLLER_HPP_
#define _NETP_EPOLL_POLLER_HPP_

#include <sys/epoll.h>
#include <syscall.h>
#include <poll.h>

#include <netp/core.hpp>
#include <netp/io_event_loop.hpp>
#include <netp/socket_api.hpp>

namespace netp {

	class poller_epoll final:
		public io_event_loop
	{
		int m_epfd;

	public:
		poller_epoll(poller_cfg const& cfg):
			io_event_loop(T_EPOLL,cfg),
			m_epfd(-1)
		{
		}

		~poller_epoll() {
			NETP_ASSERT( m_epfd == -1 );
		}

		int _do_watch(SOCKET fd, u8_t flag, NRP<watch_ctx> const& ctx) override {
			NETP_ASSERT(fd != NETP_INVALID_SOCKET);
			NETP_ASSERT(in_event_loop());
			const u8_t f2 = (!(--flag)) + 1;

			struct epoll_event epEvent =
			{
#ifdef NETP_IO_MODE_EPOLL_USE_ET
				EPOLLET|EPOLLPRI|EPOLLHUP|EPOLLERR,
#else
				EPOLLLT|EPOLLPRI|EPOLLHUP|EPOLLERR,
#endif
				{(void*)ctx.get()}
			};

			int epoll_op = EPOLL_CTL_ADD;
			if (ctx->iofn[f2] != nullptr) {
				epEvent.events |= (EPOLLIN|EPOLLOUT);
				epoll_op = EPOLL_CTL_MOD;
			} else {
                const static int _s_flag_epollin_epollout_map[] = {
                    EPOLLIN,EPOLLOUT
                };
				epEvent.events |= _s_flag_epollin_epollout_map[flag];
			}

			NETP_TRACE_IOE("fd: %d, op:%d, evts: %u", fd, epoll_op, epEvent.events);
			return epoll_ctl(m_epfd, epoll_op, fd, &epEvent);
		}

		int _do_unwatch( SOCKET fd, u8_t flag, NRP<watch_ctx> const& ctx ) override {

			NETP_ASSERT(fd != NETP_INVALID_SOCKET);
			NETP_ASSERT(in_event_loop());
			const u8_t f2 = (!(--flag)) + 1;

			struct epoll_event epEvent =
			{
#ifdef NETP_IO_MODE_EPOLL_USE_ET
				EPOLLET|EPOLLPRI|EPOLLHUP|EPOLLERR|EPOLLIN|EPOLLOUT,
#else
				EPOLLLT|EPOLLPRI|EPOLLHUP|EPOLLERR|EPOLLIN|EPOLLOUT,
#endif
				{(void*)ctx.get()}
			};

			int epoll_op = EPOLL_CTL_MOD;
			if (ctx->iofn[f2] == nullptr) {
				epEvent.events &= ~(EPOLLIN | EPOLLOUT);
				epoll_op = EPOLL_CTL_DEL;
			} else {
				const static int _s_flag_epollin_epollout_map[] = {
					EPOLLIN,EPOLLOUT
				};
				epEvent.events &= ~(_s_flag_epollin_epollout_map[flag]);
			}
			return epoll_ctl(m_epfd,epoll_op,fd,&epEvent) ;
		}

	public:
		void _do_poller_init() override {
			//the size argument is ignored since Linux 2.6.8, but must be greater than zero
			m_epfd = epoll_create(NETP_EPOLL_CREATE_HINT_SIZE);
			if (-1 == m_epfd) {
				NETP_THROW("create epoll handle failed");
			}
			NETP_DEBUG("[EPOLL]init write epoll handle ok");
			io_event_loop::_do_poller_init();
		}

		void _do_poller_deinit() override {
			io_event_loop::_do_poller_deinit();

			NETP_ASSERT(m_epfd != NETP_INVALID_SOCKET);
			int rt = ::close(m_epfd);
			if (-1 == rt) {
				NETP_THROW("EPOLL::deinit epoll handle failed");
			}
			m_epfd = -1;
			NETP_TRACE_IOE("[EPOLL] EPOLL::deinit() done");
		}

		void _do_poll(long long wait_in_nano) override {
			NETP_ASSERT( m_epfd != NETP_INVALID_SOCKET );
			NETP_ASSERT(in_event_loop());

			struct epoll_event epEvents[NETP_EPOLL_PER_HANDLE_SIZE];
			int wait_in_mill = wait_in_nano != ~0 ? wait_in_nano / 1000000: ~0;
			int nEvents = epoll_wait(m_epfd, epEvents,NETP_EPOLL_PER_HANDLE_SIZE, wait_in_mill);
			__LOOP_EXIT_WAITING__();
			if ( -1 == nEvents ) {
				NETP_ERR("[EPOLL][##%u]epoll wait event failed!, errno: %d", m_epfd, netp_socket_get_last_errno() );
				return ;
			}

			for( int i=0;i<nEvents;++i) {

				NETP_ASSERT( epEvents[i].data.ptr != nullptr );
				NRP<watch_ctx> ctx (static_cast<watch_ctx*> (epEvents[i].data.ptr)) ;
				NETP_ASSERT(ctx->fd != NETP_INVALID_SOCKET);

				uint32_t events = ((epEvents[i].events) & 0xFFFFFFFF) ;
				//NETP_TRACE_IOE( "[EPOLL][##%u][#%d]EVT: events(%d)", m_epfd, ctx->fd, events );

				int ec = netp::OK;
				if( NETP_UNLIKELY(events&(EPOLLERR|EPOLLHUP)) ) {
					//TRACE_IOE( "[EPOLL][##%d][#%d]EVT: (EPOLLERR|EPOLLHUB), post IOE_ERROR", m_epfd, ctx->fd );
					//refer to https://stackoverflow.com/questions/52976152/tcp-when-is-epollhup-generated

					if ((events&EPOLLERR) != 0) {
						socklen_t optlen = sizeof(int);
						int getrt = ::getsockopt(ctx->fd, SOL_SOCKET, SO_ERROR, (char*)&ec, &optlen);
						if (getrt == -1) {
							ec = netp_socket_get_last_errno();
						} else {
							NETP_ASSERT(ec != netp::OK);
							ec = NETP_NEGATIVE(ec);
						}
					} else if ((events&EPOLLHUP) != 0) {
						ec = netp::E_SOCKET_EPOLLHUP;
					} else {
						ec = netp::E_UNKNOWN;
					}
					events &= ~(EPOLLERR | EPOLLHUP);
				}

				const static int _s_flag_epollin_epollout_map[] = {
					0,EPOLLIN,EPOLLOUT
				};

				for (i8_t i = aio_flag::AIO_READ; i < aio_flag::AIO_FLAG_MAX; ++i) {
					if (events & _s_flag_epollin_epollout_map[i]) {
						events &= ~(_s_flag_epollin_epollout_map[i]);
#ifdef NETP_DEBUG_WATCH_CTX_FLAG
						NETP_ASSERT( ((ctx->flag& (i)) && ctx->iofn[i] != nullptr) , "fd: %d, flag: %d, ec", ctx->fd, ctx->flag, ec );
#endif
						ctx->iofn[i](ec);
						continue;
					}

					ec != netp::OK && ctx->iofn[i] != nullptr ? ctx->iofn[i](ec) : (void)0;
				}

				if (events&EPOLLPRI) {
					events &= ~EPOLLPRI;
					NETP_ERR("[EPOLL][##%d][#%d]EVT: EPOLLPRI", m_epfd, ctx->fd);
					//NETP_THROW("EPOLLPRI arrive!!!");
				}

				NETP_ASSERT( events == 0, "evt: %d", events );
			}
		}
	};
}
#endif
