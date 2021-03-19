#ifndef _NETP_EPOLL_POLLER_HPP_
#define _NETP_EPOLL_POLLER_HPP_

#include <sys/epoll.h>
#include <syscall.h>
#include <poll.h>

#include <netp/core.hpp>
#include <netp/poller_abstract.hpp>
#include <netp/socket_api.hpp>

namespace netp {

	class poller_epoll final:
		public poller_abstract
	{
		int m_epfd;

	public:
		poller_epoll():
			poller_abstract(),
			m_epfd(-1)
		{
		}

		~poller_epoll() {
			NETP_ASSERT( m_epfd == -1 );
		}

		int watch(u8_t flag, aio_ctx* ctx) override {
			NETP_ASSERT( ctx->fd != NETP_INVALID_SOCKET);
//			const u8_t f2 = (!(--flag)) + 1;

			struct epoll_event epEvent =
			{
#ifdef NETP_IO_POLLER_EPOLL_USE_ET
				EPOLLET|EPOLLPRI|EPOLLHUP|EPOLLERR,
#else
				EPOLLLT|EPOLLPRI|EPOLLHUP|EPOLLERR,
#endif
				{(void*)ctx}
			};

			int epoll_op = EPOLL_CTL_ADD;
			if ( 0 != ctx->flag ) {
				epEvent.events |= (EPOLLIN|EPOLLOUT);
				epoll_op = EPOLL_CTL_MOD;
			} else {
                const static int _s_flag_epollin_epollout_map[] = {
                    EPOLLIN,EPOLLOUT
                };
				epEvent.events |= _s_flag_epollin_epollout_map[--flag];
			}

			NETP_TRACE_IOE("[watch]fd: %d, op:%d, evts: %u", ctx->fd, epoll_op, epEvent.events);
			return epoll_ctl(m_epfd, epoll_op, ctx->fd, &epEvent);
		}

		int unwatch( u8_t flag, aio_ctx* ctx ) override {

			NETP_ASSERT(ctx->fd != NETP_INVALID_SOCKET);
			//const u8_t f2 = (!(--flag)) + 1;

			struct epoll_event epEvent =
			{
#ifdef NETP_IO_POLLER_EPOLL_USE_ET
				EPOLLET|EPOLLPRI|EPOLLHUP|EPOLLERR|EPOLLIN|EPOLLOUT,
#else
				EPOLLLT|EPOLLPRI|EPOLLHUP|EPOLLERR|EPOLLIN|EPOLLOUT,
#endif
				{(void*)ctx}
			};

			int epoll_op = EPOLL_CTL_MOD;
			if ( 0 == (ctx->flag & (~flag)) ) {
				epEvent.events &= ~(EPOLLIN | EPOLLOUT);
				epoll_op = EPOLL_CTL_DEL;
			} else {
				const static int _s_flag_epollin_epollout_map[] = {
					EPOLLIN,EPOLLOUT
				};
				epEvent.events &= ~(_s_flag_epollin_epollout_map[--flag]);
			}
			NETP_TRACE_IOE("[unwatch]fd: %d, op:%d, evts: %u", ctx->fd, epoll_op, epEvent.events);
			return epoll_ctl(m_epfd,epoll_op,ctx->fd,&epEvent) ;
		}

	public:
		void init() override {
			//the size argument is ignored since Linux 2.6.8, but must be greater than zero
			m_epfd = epoll_create(NETP_EPOLL_CREATE_HINT_SIZE);
			if (-1 == m_epfd) {
				NETP_THROW("create epoll handle failed");
			}
			NETP_DEBUG("[EPOLL]init write epoll handle ok");
			poller_abstract::init();
		}

		void deinit() override {
			poller_abstract::deinit();
			NETP_ASSERT(m_epfd != NETP_INVALID_SOCKET);
			int rt = ::close(m_epfd);
			if (-1 == rt) {
				NETP_THROW("EPOLL::deinit epoll handle failed");
			}
			m_epfd = -1;
			NETP_TRACE_IOE("[EPOLL] EPOLL::deinit() done");
		}

		void poll(long long wait_in_nano, std::atomic<bool>& W) override {
			NETP_ASSERT( m_epfd != NETP_INVALID_SOCKET );

			struct epoll_event epEvents[NETP_EPOLL_PER_HANDLE_SIZE];
			int wait_in_mill = wait_in_nano != ~0 ? wait_in_nano / 1000000: ~0;
			int nEvents = epoll_wait(m_epfd, epEvents,NETP_EPOLL_PER_HANDLE_SIZE, wait_in_mill);
			__LOOP_EXIT_WAITING__(W);
			if ( -1 == nEvents ) {
				NETP_ERR("[EPOLL][##%u]epoll wait event failed!, errno: %d", m_epfd, netp_socket_get_last_errno() );
				return ;
			}

			for( int i=0;i<nEvents;++i) {

				NETP_ASSERT( epEvents[i].data.ptr != nullptr );
				aio_ctx* ctx =(static_cast<aio_ctx*> (epEvents[i].data.ptr)) ;
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

				if ( ((events&EPOLLIN) || ec != netp::OK) && ctx->fn_read != nullptr ) {
					NETP_ASSERT(ctx->flag & u8_t(aio_flag::AIO_READ));
					ctx->fn_read(ec, ctx);
				}
				if ( ( (events&EPOLLOUT) || ec != netp::OK) && ctx->fn_write != nullptr ) {
					NETP_ASSERT(ctx->flag & u8_t(aio_flag::AIO_WRITE));
					ctx->fn_write(ec, ctx);
				}
				events &= ~(EPOLLOUT|EPOLLIN);

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
