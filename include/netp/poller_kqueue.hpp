#ifndef _NETP_POLLER_KQUEUE_HPP
#define _NETP_POLLER_KQUEUE_HPP

#include <sys/event.h>
#include <netp/core.hpp>
#include <netp/poller_abstract.hpp>

#define NETP_KEVT_COUNT (256)
namespace netp {
	class poller_kqueue final :
		public io_event_loop
	{
		int m_kq;
		struct kevent* m_kevts;
	public:
		poller_kqueue() :
			poller_abstract(),
			m_kq(-1),
			m_kevts(0)
		{}
		void init() override {
			NETP_DEBUG("CREATE KQUEUE");
			m_kq = kqueue();
			if (m_kq == -1) {
				NETP_ERR("[kqueue]create poller failed: %d", netp_last_errno() );
				NETP_THROW("KQUEUE CREATE FAILED");
			}
			NETP_DEBUG("CREATE KQUEUE DONE");
			m_kevts = (struct kevent*)netp::aligned_malloc( sizeof(struct kevent)* NETP_KEVT_COUNT, NETP_DEFAULT_ALIGN);
			NETP_ALLOC_CHECK(m_kevts, sizeof(struct kevent)* NETP_KEVT_COUNT);
			poller_abstract::init();
		}

		void deinit() override {
			poller_abstract::deinit();
			close(m_kq);
			m_kq = -1;
			netp::aligned_free(m_kevts);
			m_kevts=0;
			m_kevt_size = 0;
		}
	
		void poll(long long wait_in_nano, std::atomic<bool>& W) override {
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
			int rt = kevent(m_kq, NULL, 0, m_kevts, NETP_KEVT_COUNT, tspp);
			__LOOP_EXIT_WAITING__(W);
			
			if (NETP_LIKELY(rt > 0)) {
				for (int j = 0; j < rt; ++j) {
					struct kevent* e = (m_kevts + j);
					NETP_ASSERT(e->udata != nullptr);
					aio_ctx* ctx = (static_cast<aio_ctx*> (e->udata));
					if (e->filter==EVFILT_READ && ctx->fn_read != nullptr) {
						ctx->fn_read(ec);
					}
					if (e->filter==EVFILT_WRITE && ctx->fn_write != nullptr ) {
						ctx->fn_write(ec);
					}
				}
			}
		}
		int watch( u8_t flag, aio_ctx* ctx) {
			struct kevent ke;
			if (flag&aio_flag::AIO_READ) {
				EV_SET(&ke, fd, EVFILT_READ, EV_ADD, 0, 0, (void*)(ctx));
				int rt = kevent(m_kq, &ke, 1, NULL, 0, NULL);
				NETP_RETURN_V_IF_MATCH(rt, rt == -1);
			}
			if (flag&aio_flag::AIO_WRITE) {
				EV_SET(&ke, fd, EVFILT_WRITE, EV_ADD, 0, 0, (void*)(ctx));
				int rt = kevent(m_kq, &ke, 1, NULL, 0, NULL);
				NETP_RETURN_V_IF_MATCH(rt, rt == -1);
			}
			return netp::OK;
		}
		int unwatch(u8_t flag, aio_ctx* ctx) {
			struct kevent ke;
			if (flag & aio_flag::AIO_READ) {
				EV_SET(&ke, fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
				int rt = kevent(m_kq, &ke, 1, NULL, 0, NULL);
				NETP_RETURN_V_IF_MATCH(rt, rt == -1);
			}
			if (flag & aio_flag::AIO_WRITE) {
				EV_SET(&ke, fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
				int rt = kevent(m_kq, &ke, 1, NULL, 0, NULL);
				NETP_RETURN_V_IF_MATCH(rt, rt == -1);
			}
			return netp::OK;
		}
	};
}
#endif