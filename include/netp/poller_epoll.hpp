#ifndef _NETP_EPOLL_POLLER_HPP_
#define _NETP_EPOLL_POLLER_HPP_

#include <sys/epoll.h>
#include <syscall.h>
#include <poll.h>

#include <netp/core.hpp>
#include <netp/poller_interruptable_by_fd.hpp>
#include <netp/socket_api.hpp>
//#include <netp/benchmark.hpp>

namespace netp {

	class poller_epoll final:
		public poller_interruptable_by_fd
	{
//		char _HEAD[64];
		int m_epfd;
//		int m_epfd1;
//		int m_epfd2;
//		int m_epfd3;
//		char _TAIL[64];

	public:
		poller_epoll():
			poller_interruptable_by_fd(),
			m_epfd(NETP_INVALID_SOCKET)
		{
//			std::memset(_HEAD, 0, 64);
//			std::memset(_TAIL, 0, 64);
		}

		~poller_epoll() {
			NETP_ASSERT( m_epfd == NETP_INVALID_SOCKET);
//			char _aa[64] = { 0 };
//			NETP_ASSERT(std::memcmp(_TAIL, _aa, 64) == 0);
//			NETP_ASSERT(std::memcmp(_HEAD, _aa, 64) == 0);
		}

		int watch(u8_t flag, io_ctx* ctx) override {
			NETP_ASSERT( ctx->fd != NETP_INVALID_SOCKET);
			struct epoll_event epEvent =
			{
#ifdef NETP_IO_POLLER_EPOLL_USE_ET
				EPOLLET|EPOLLPRI|EPOLLRDHUP|EPOLLHUP|EPOLLERR,
#else
				EPOLLLT|EPOLLPRI|EPOLLRDHUP|EPOLLHUP|EPOLLERR,
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

		int unwatch( u8_t flag, io_ctx* ctx ) override {
			NETP_ASSERT(ctx->fd != NETP_INVALID_SOCKET);

			struct epoll_event epEvent =
			{
#ifdef NETP_IO_POLLER_EPOLL_USE_ET
				EPOLLET|EPOLLPRI|EPOLLRDHUP|EPOLLHUP|EPOLLERR|EPOLLIN|EPOLLOUT,
#else
				EPOLLLT|EPOLLPRI|EPOLLRDHUP|EPOLLHUP|EPOLLERR|EPOLLIN|EPOLLOUT,
#endif
				{(void*)ctx}
			};

			int epoll_op = EPOLL_CTL_MOD;
			if ( 0 == (ctx->flag & (~flag)) ) {
				epEvent.events &= ~(EPOLLIN|EPOLLOUT);
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
			NETP_VERBOSE("[EPOLL][##%u]init epoll handle ok", m_epfd);
			poller_interruptable_by_fd::init();
		}

		void deinit() override {
			poller_interruptable_by_fd::deinit();
			NETP_ASSERT(m_epfd != NETP_INVALID_SOCKET);
			NETP_VERBOSE("[EPOLL][##%u]EPOLL::deinit() begin", m_epfd);
			int rt = netp::close(m_epfd);
			if (-1 == rt) {
				NETP_THROW("EPOLL::deinit epoll handle failed");
			}
			NETP_VERBOSE("[EPOLL][##%u]EPOLL::deinit() done", m_epfd );
			m_epfd = NETP_INVALID_SOCKET;
		}

		void poll(i64_t wait_in_nano, std::atomic<bool>& W) override {
			NETP_ASSERT( m_epfd != NETP_INVALID_SOCKET );

			struct epoll_event epEvents[NETP_EPOLL_PER_HANDLE_SIZE];
			const int wait_in_mill = (wait_in_nano != ~0 ? (wait_in_nano / i64_t(1000000)): ~0);
//			char _mk_buf[128] = { 0 };
//			snprintf(_mk_buf, 128, "epollwait(%d)", wait_in_mill);
//			netp::benchmark mk(_mk_buf);
			int nEvents = epoll_wait(m_epfd, epEvents,NETP_EPOLL_PER_HANDLE_SIZE, wait_in_mill);
//			mk.mark("epoll_wait return");
			NETP_POLLER_WAIT_EXIT(wait_in_nano, W);
			if ( -1 == nEvents ) {
				NETP_ERR("[EPOLL][##%u]epoll wait event failed!, errno: %d", m_epfd, netp_socket_get_last_errno() );
				return ;
			}

			for( int i=0;i<nEvents;++i) {
#ifdef _NETP_DEBUG
				NETP_ASSERT( epEvents[i].data.ptr != nullptr );
#endif
				io_ctx* ctx =(static_cast<io_ctx*> (epEvents[i].data.ptr)) ;
#ifdef _NETP_DEBUG
				NETP_ASSERT(ctx->fd != NETP_INVALID_SOCKET);
#endif
				uint32_t events = ((epEvents[i].events) & 0xFFFFFFFF) ;
				//NETP_TRACE_IOE( "[EPOLL][##%u][#%d]EVT: events(%d)", m_epfd, ctx->fd, events );
				//TRACE_IOE( "[EPOLL][##%d][#%d]EVT: (EPOLLERR|EPOLLHUB), post IOE_ERROR", m_epfd, ctx->fd );
				//refer to https://stackoverflow.com/questions/52976152/tcp-when-is-epollhup-generated
				//@note:
				// 1) FIN_SENT&FIN_RECV result  in hub
				// 2) FIN_RECV result rdhub
				// 3) for EPOLLERR, just notify read|write, if there is a error ,let read|write to handle it
				// 4) EPOLLRDHUP|EPOLLIN would arrive at the same time (but it's not sometimes), keep reading until read() return 0 to avoid a miss
				//		4.1) alternative solution is to ignore read if we get EPOLLRDHUB, in this case, we might miss some pending data in rcvbuf 
				int ec = (events&EPOLLHUP) ? netp::E_SOCKET_EPOLLHUP : netp::OK;
				NRP<io_monitor>& iom = ctx->iom;
				if ((ctx->flag&u8_t(io_flag::IO_READ)) && ((events&(EPOLLRDHUP|EPOLLERR|EPOLLIN)) || (ec != netp::OK)) ) {
					iom->io_notify_read(ec, ctx);
				}
				//read error might result in write act be cancelled, just cancel it 
				if ((ctx->flag&u8_t(io_flag::IO_WRITE)) && ((events&(EPOLLERR|EPOLLOUT)) || (ec != netp::OK)) ) {
					iom->io_notify_write(ec, ctx);
				}
				if (events&EPOLLPRI) {
					NETP_ERR("[EPOLL][##%d][#%d]EVT: EPOLLPRI", m_epfd, ctx->fd);
					//NETP_THROW("EPOLLPRI arrive!!!");
				}

#ifdef _NETP_DEBUG
				events &= ~(EPOLLPRI|EPOLLERR|EPOLLRDHUP|EPOLLHUP|EPOLLIN|EPOLLOUT);
				NETP_ASSERT( events == 0, "evt: %d", events );
#endif
			}
		}
	};
}
#endif
