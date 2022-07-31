#ifndef _NETP_EPOLL_POLLER_HPP_
#define _NETP_EPOLL_POLLER_HPP_

#include <sys/epoll.h>
#include <syscall.h>
#include <poll.h>

#include <netp/core.hpp>
#include <netp/poller_interruptable_by_fd.hpp>
#include <netp/socket_api.hpp>

#ifdef _NETP_DEBUG
	#define _NETP_DEBUG_EPOLL_EVENTS
#endif

namespace netp {

	const static int _s_flag_epollin_epollout_map[2] = {
		EPOLLIN,EPOLLOUT
	};

	class poller_epoll final:
		public poller_interruptable_by_fd
	{
		int m_epfd;
	public:
		poller_epoll():
			poller_interruptable_by_fd(io_poller_type::T_EPOLL),
			m_epfd(NETP_INVALID_SOCKET)
		{
		}

		~poller_epoll() {
			NETP_ASSERT( m_epfd == NETP_INVALID_SOCKET);
		}

		int watch(u8_t flag, io_ctx* ctx) override {

#ifdef _NETP_DEBUG_EPOLL_EVENTS
			NETP_ASSERT((ctx->fd != NETP_INVALID_SOCKET) && ((flag & ((io_flag::IO_READ | io_flag::IO_WRITE))) == io_flag::IO_READ || (flag & ((io_flag::IO_READ | io_flag::IO_WRITE))) == io_flag::IO_WRITE));
			NETP_ASSERT((flag&ctx->flag) == 0);
			//test forcce lt
			//ctx->flag |= io_flag::IO_EPOLL_NOET;
#endif

			struct epoll_event epEvent =
			{
				(ctx->flag&io_flag::IO_EPOLL_NOET) ? (EPOLLRDHUP|EPOLLHUP|EPOLLERR) : (EPOLLET|EPOLLRDHUP|EPOLLHUP|EPOLLERR),
				{(void*)ctx}
			};

			int epoll_op = EPOLL_CTL_ADD;
			if ( (ctx->flag&flag) == flag ) {
				return netp::OK;
			} else if ( ctx->flag&(io_flag::IO_READ|io_flag::IO_WRITE) ) {
				epoll_op = EPOLL_CTL_MOD;
				epEvent.events |= (EPOLLIN|EPOLLOUT);
			} else {
				epEvent.events |= _s_flag_epollin_epollout_map[--flag];
			}

#ifdef _NETP_DEBUG_EPOLL_EVENTS
			NETP_ASSERT(epEvent.events & (EPOLLIN|EPOLLOUT) );
#endif
			NETP_TRACE_IOE("[EPOLL][##%u][watch][#%u]op: %c, evts: %u", m_epfd, ctx->fd, epoll_op == EPOLL_CTL_MOD ? 'm' : 'a', epEvent.events);
			return epoll_ctl(m_epfd, epoll_op, ctx->fd, &epEvent);
		}

		int unwatch( u8_t flag, io_ctx* ctx ) override {
#ifdef _NETP_DEBUG_EPOLL_EVENTS
			NETP_ASSERT((ctx->fd != NETP_INVALID_SOCKET) && (flag == io_flag::IO_READ || flag == io_flag::IO_WRITE));
#endif
			struct epoll_event epEvent =
			{
				(ctx->flag&io_flag::IO_EPOLL_NOET) ? (EPOLLRDHUP|EPOLLHUP|EPOLLERR) : (EPOLLET|EPOLLRDHUP|EPOLLHUP|EPOLLERR),
				{(void*)ctx}
			};

			int epoll_op = EPOLL_CTL_MOD;
			u8_t remaining_flag = ((ctx->flag&(~flag)) & (io_flag::IO_READ|io_flag::IO_WRITE));
			if (remaining_flag) {
				epEvent.events |= _s_flag_epollin_epollout_map[--remaining_flag];
			} else {
				//@note
				//In kernel versions before 2.6.9, the EPOLL_CTL_DEL operation required a non - NULL pointer in event, even though this argument is ignored.
				//Since Linux 2.6.9, event can be specified as NULL when using EPOLL_CTL_DEL.Applications that need to be portable to kernels before 2.6.9 should specify a non - NULL pointer in event.

				//epEvent.events &= ~(EPOLLIN|EPOLLOUT);
				epoll_op = EPOLL_CTL_DEL;
			}

#ifdef _NETP_DEBUG_EPOLL_EVENTS
			NETP_ASSERT(epoll_op == EPOLL_CTL_MOD ? (epEvent.events & (EPOLLIN|EPOLLOUT)) && (ctx->flag&(io_flag::IO_READ | io_flag::IO_WRITE)) : true);
#endif
			NETP_TRACE_IOE("[EPOLL][##%u][unwatch][#%u]op: %c, evts: %u", m_epfd, ctx->fd, epoll_op == EPOLL_CTL_MOD ? 'm': 'd', epEvent.events);
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
			NETP_ASSERT(m_epfd != NETP_INVALID_SOCKET);

			struct epoll_event epEvents[NETP_EPOLL_PER_HANDLE_SIZE];
			const int wait_in_mill = (wait_in_nano != ~0 ? (wait_in_nano / i64_t(1000000)) : ~0);
			int nEvents = epoll_wait(m_epfd, epEvents, NETP_EPOLL_PER_HANDLE_SIZE, wait_in_mill);
			NETP_POLLER_WAIT_EXIT(wait_in_nano, W);
			if (-1 == nEvents) {
				NETP_ERR("[EPOLL][##%u]epoll wait event failed!, errno: %d", m_epfd, netp_socket_get_last_errno());
				return;
			}

#ifdef _NETP_DEBUG_EPOLL_EVENTS
			//checked from epoll source code, ep_remove would result in epitem be freeed, if ctx->flag & (io_flag::IO_READ|io_flag::IO_WRITE) == 0, the fd should have been removed
			//steps of ep_remove:
			//remove poll_wait queue
			//remove rdlink from list
			for (int i = 0; i < nEvents; ++i) {
				NETP_ASSERT(epEvents[i].data.ptr != nullptr);
				io_ctx* ctx = (static_cast<io_ctx*> (epEvents[i].data.ptr));
				NETP_ASSERT((ctx->fd != NETP_INVALID_SOCKET) && (ctx->flag&(io_flag::IO_READ|io_flag::IO_WRITE)), "fd: %u, flag: %u, event: %u", ctx->fd, ctx->flag, epEvents[i].events);
			}
#endif

			//@note: fda's event might result in unwatch(R|W) for fdb's
			for( int i=0;i<nEvents;++i) {
#ifdef _NETP_DEBUG_EPOLL_EVENTS
				NETP_ASSERT( epEvents[i].data.ptr != nullptr );
#endif
				uint32_t events = ((epEvents[i].events) & 0xFFFFFFFF);
				io_ctx* ctx = (static_cast<io_ctx*> (epEvents[i].data.ptr));
				int sockerr = netp::OK;
				//refer to:https://elixir.bootlin.com/linux/v4.19/source/net/ipv4/tcp.c#L524
				//EPOLLHUP is only sent when the shutdown has been both for read and write (I reckon that the peer shutdowning the write equals to my shutdowning the read). Or when the connection is closed, of course.
				if (events&(EPOLLERR|EPOLLHUP)) {
					//WE NEED THESE ERR INFO
					socklen_t optlen = sizeof(int);
					int readsockfderr = ::getsockopt(ctx->fd, SOL_SOCKET, SO_ERROR, (char*)&sockerr, &optlen);
					(void)readsockfderr;
					if (sockerr == -1) {
						if (events&EPOLLHUP) {
							sockerr = netp::E_SOCKET_EPOLLHUP;
						} else {
							sockerr = netp::E_UNKNOWN;
						}
					}
					else { sockerr=NETP_NEGATIVE(sockerr); }
				}

				//NETP_TRACE_IOE( "[EPOLL][##%u][#%d]EVT: events(%d)", m_epfd, ctx->fd, events );
				//TRACE_IOE( "[EPOLL][##%d][#%d]EVT: (EPOLLERR|EPOLLHUB), post IOE_ERROR", m_epfd, ctx->fd );
				//refer to https://stackoverflow.com/questions/52976152/tcp-when-is-epollhup-generated
				//@note:
				// 1) FIN_SENT&FIN_RECV result in hub
				// 2) FIN_RECV result in rdhub
				// 3) for EPOLLERR, just notify read|write, if there is a error ,let read|write to handle it
				// 4) EPOLLRDHUP|EPOLLIN would arrive at the same time (but it's not sometimes), keep reading until read() return 0 to avoid a miss
				//		4.1) alternative solution is to ignore read if we get EPOLLRDHUB, in this case, we might miss some pending data in rcvbuf 

				NRP<io_monitor>& iom = ctx->iom;
				//do not check EPOLLERR|EPOLLHUP for read/write, as we has checked before, if they are set, sockerr must not be netp::OK
				if ((ctx->flag&u8_t(io_flag::IO_READ)) && (events&(EPOLLIN|EPOLLRDHUP|EPOLLERR|EPOLLHUP)) ) {
					if (events&EPOLLRDHUP) {
						ctx->flag |= io_flag::IO_READ_HUP;
					}
					iom->io_notify_read(sockerr, ctx);
				}
				//@note io_notify_read might result in io_write be removed
				//read error might result in write act be cancelled, just cancel it 
				if ((ctx->flag&u8_t(io_flag::IO_WRITE)) && (events&(EPOLLOUT|EPOLLERR|EPOLLHUP)) ) {
					iom->io_notify_write(sockerr, ctx);
				}
			}
		}
	};
}
#endif
