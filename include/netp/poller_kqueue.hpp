#ifndef _NETP_POLLER_KQUEUE_HPP
#define _NETP_POLLER_KQUEUE_HPP

#include <sys/event.h>
#include <netp/core.hpp>
#include <netp/io_event_loop.hpp>

namespace netp {
	class poller_kqueue final :
		public io_event_loop
	{
		int m_kq;
		struct kevent* m_kevts;
		int m_kevt_size;
	public:
		poller_kqueue(poller_cfg const& cfg) :
			io_event_loop(T_KQUEUE, cfg),
			m_kq(-1),
			m_kevts(0),
			m_kevt_size(0)
		{}
		void _do_poller_init() override {
			NETP_DEBUG("CREATE KQUEUE");
			m_kq = kqueue();
			if (m_kq == -1) {
				NETP_ERR("[kqueue]create poller failed: %d", netp_last_errno() );
				NETP_THROW("KQUEUE CREATE FAILED");
			}
			NETP_DEBUG("CREATE KQUEUE DONE");
			m_kevts = (struct kevent*)netp::aligned_malloc( sizeof(struct kevent) * 256, NETP_DEFAULT_ALIGN);
			NETP_ALLOC_CHECK(m_kevts, sizeof(struct kevent) * 256);
			m_kevt_size = 256;
			io_event_loop::_do_poller_init();
		}

		void _do_poller_deinit() override {
			io_event_loop::_do_poller_deinit();
			close(m_kq);
			m_kq = -1;
			netp::aligned_free(m_kevts);
			m_kevts=0;
			m_kevt_size = 0;
		}
	
		void _do_poll(long long wait_in_nano) override {
			struct timespec tsp = {0,0};
			struct timespec* tspp = 0;
			if (wait_in_nano != ~0) {
				if (wait_in_nano > 0) {
					tsp.tv_sec = static_cast<long>(wait_in_nano / 1000000000ULL);
					tsp.tv_nsec = static_cast<long>(wait_in_nano % 1000000000ULL);
				}
				tspp = &tsp;
			}

			int ec=netp::OK;
			int rt = kevent(m_kq, NULL, 0, m_kevts, m_kevt_size, tspp);
                        __LOOP_EXIT_WAITING__();
			if (NETP_LIKELY(rt > 0)) {
				for (int j = 0; j < rt; ++j) {
					struct kevent* e = (m_kevts + j);
					NETP_ASSERT(e->udata != nullptr);
					NRP<watch_ctx> ctx(static_cast<watch_ctx*> (e->udata));
					if (e->filter==EVFILT_READ) {
#ifdef NETP_DEBUG_WATCH_CTX_FLAG
						NETP_ASSERT(((ctx->flag & (AIO_READ)) && ctx->iofn[AIO_READ] != nullptr), "fd: %d, flag: %d", ctx->fd, ctx->flag);
#endif
						ctx->iofn[AIO_READ](ec);
					}
					if (e->filter==EVFILT_WRITE) {
#ifdef NETP_DEBUG_WATCH_CTX_FLAG
						NETP_ASSERT(((ctx->flag & (AIO_WRITE)) && ctx->iofn[AIO_WRITE] != nullptr), "fd: %d, flag: %d", ctx->fd, ctx->flag);
#endif	
						ctx->iofn[AIO_WRITE](ec);
					}
				}
			}
		}
		int _do_watch(SOCKET fd, u8_t flag, NRP<watch_ctx> const& ctx) {
			struct kevent ke;
			if (flag&AIO_READ) {
				EV_SET(&ke, fd, EVFILT_READ, EV_ADD, 0, 0, (void*)(ctx.get()));
				int rt = kevent(m_kq, &ke, 1, NULL, 0, NULL);
				NETP_RETURN_V_IF_MATCH(rt, rt == -1);
			}
			if (flag & AIO_WRITE) {
				EV_SET(&ke, fd, EVFILT_WRITE, EV_ADD, 0, 0, (void*)(ctx.get()));
				int rt = kevent(m_kq, &ke, 1, NULL, 0, NULL);
				NETP_RETURN_V_IF_MATCH(rt, rt == -1);
			}
			return netp::OK;
		}
		int _do_unwatch(SOCKET fd, u8_t flag, NRP<watch_ctx> const& ctx) {
			struct kevent ke;
			if (flag & AIO_READ) {
				EV_SET(&ke, fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
				int rt = kevent(m_kq, &ke, 1, NULL, 0, NULL);
				NETP_RETURN_V_IF_MATCH(rt, rt == -1);
			}
			if (flag & AIO_WRITE) {
				EV_SET(&ke, fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
				int rt = kevent(m_kq, &ke, 1, NULL, 0, NULL);
				NETP_RETURN_V_IF_MATCH(rt, rt == -1);
			}
			return netp::OK;
		}
	};
}

#endif
