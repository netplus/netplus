#ifndef _NETP_POLLER_ABSTRACT_HPP
#define _NETP_POLLER_ABSTRACT_HPP

#include <netp/core.hpp>
#include <netp/smart_ptr.hpp>
#include <netp/address.hpp>
#include <netp/socket_api.hpp>


#ifdef _DEBUG
#define NETP_DEBUG_TERMINATING
#define NETP_DEBUG_AIO_CTX_
#endif

namespace netp {

	enum io_poller_type {
		T_SELECT, //win&linux&android
		T_IOCP, //win
		T_EPOLL, //linux,epoll,et
		T_KQUEUE,//bsd
		T_POLLER_CUSTOM_1,
		T_POLLER_CUSTOM_2,
		T_POLLER_MAX,
		T_DUMMY,
		T_NONE
	};

	enum aio_flag {
		AIO_NOTIFY = 0,
		AIO_READ = 1,
		AIO_WRITE = 1 << 1,
		AIO_FLAG_MAX = 3
	};

	enum class aio_action {
		READ = 1 << 0, //check read, sys io
		END_READ = 1 << 1,

		WRITE = 1 << 2, //check write, sys io
		END_WRITE = 1 << 3,

		NOTIFY_TERMINATING = 1 << 4,
		READ_WRITE = (READ | WRITE)
	};

	struct aio_ctx;
	typedef std::function<void(int status, aio_ctx* ctx)> fn_aio_event_t;
	struct aio_ctx
	{
		aio_ctx* prev;
		aio_ctx* next;

		SOCKET fd;
		fn_aio_event_t fn_read;
		fn_aio_event_t fn_write;
		fn_aio_event_t fn_notify;

		u8_t flag;

#ifdef NETP_DEBUG_TERMINATING
		bool terminated;
#endif

#ifdef NETP_HAS_POLLER_IOCP
		iocp_overlapped_ctx* ol_ctxs[iocp_ol_type::WRITE + 1];
#endif
	};

	inline static void aio_ctx_list_init(aio_ctx* list) {
		list->next = list;
		list->prev = list;
	}
	inline static void __aio_ctx_list_insert(aio_ctx* prev, aio_ctx* next, aio_ctx* item) {
		item->next = next;
		item->prev = prev;
		next->prev = item;
		prev->next = item;
	}
	inline static void aio_ctx_list_prepend(aio_ctx* list, aio_ctx* item) {
		__aio_ctx_list_insert(list, list->next, item);
	}
	inline static void aio_ctx_list_append(aio_ctx* list, aio_ctx* item) {
		__aio_ctx_list_insert(list->prev, list, item);
	}
	inline static void aio_ctx_list_delete(aio_ctx* item) {
		item->prev->next = item->next;
		item->next->prev = item->prev;
		item->next = 0;
		item->prev = 0;
	}
#define NETP_AIO_CTX_LIST_IS_EMPTY(list) ( ((list) == (list)->next) && ((list)==(list)->prev) )

	inline static aio_ctx* aio_ctx_allocate() {
		aio_ctx* ctx = netp::allocator<aio_ctx>::malloc(1);
		new ((fn_aio_event_t*)&(ctx->fn_read))(fn_aio_event_t)();
		new ((fn_aio_event_t*)&(ctx->fn_write))(fn_aio_event_t)();
		new ((fn_aio_event_t*)&(ctx->fn_notify))(fn_aio_event_t)();
		return ctx;
	}

	inline static void aio_ctx_deallocate(aio_ctx* ctx) {
		netp::allocator<aio_ctx>::free(ctx);
	}

#ifdef NETP_HAS_POLLER_IOCP
	typedef std::function<int(void* ol)> fn_overlapped_io_event;
#endif

	class poller_abstract :
		public netp::ref_base
	{

	protected:
		aio_ctx m_aio_ctx_list;

		SOCKET m_signalfds[2];
		aio_ctx* m_signalfds_aio_ctx;

#ifdef NETP_DEBUG_AIO_CTX_
		long m_aio_ctx_count_alloc;
		long m_aio_ctx_count_free;
#endif

	public:
		poller_abstract() : 
			m_signalfds {
			(SOCKET)NETP_INVALID_SOCKET, (SOCKET)NETP_INVALID_SOCKET}
		{
			netp::aio_ctx_list_init(&m_aio_ctx_list);
		}
		~poller_abstract() {}

		virtual void init() {
#ifdef NETP_DEBUG_AIO_CTX_
			m_aio_ctx_count_alloc = 0;
			m_aio_ctx_count_free = 0;
#endif

			int rt = netp::socketpair(int(NETP_AF_INET), int(NETP_SOCK_STREAM), int(NETP_PROTOCOL_TCP), m_signalfds);
			NETP_ASSERT(rt == netp::OK, "rt: %d", rt);

			rt = netp::turnon_nonblocking(netp::NETP_DEFAULT_SOCKAPI, m_signalfds[0]);
			NETP_ASSERT(rt == netp::OK, "rt: %d", rt);

			rt = netp::turnon_nonblocking(netp::NETP_DEFAULT_SOCKAPI, m_signalfds[1]);
			NETP_ASSERT(rt == netp::OK, "rt: %d", rt);

			rt = netp::turnon_nodelay(netp::NETP_DEFAULT_SOCKAPI, m_signalfds[1]);
			NETP_ASSERT(rt == netp::OK, "rt: %d", rt);

			NETP_ASSERT(rt == netp::OK);

			m_signalfds_aio_ctx = aio_begin(m_signalfds[0]);
			NETP_ASSERT(m_signalfds_aio_ctx != 0);
			m_signalfds_aio_ctx->fn_read = [](int status, aio_ctx* ctx) {
				if (status == netp::OK) {
					byte_t tmp[1];
					int ec = netp::OK;
					do {
						u32_t c = netp::recv(netp::NETP_DEFAULT_SOCKAPI, ctx->fd, tmp, 1, ec, 0);
						if (c == 1) {
							NETP_ASSERT(ec == netp::OK);
							NETP_ASSERT(tmp[0] == 'i', "c: %d", tmp[0]);
						}
					} while (ec == netp::OK);
				}
				return netp::OK;
			};
			rt = __do_execute_act(aio_action::READ, m_signalfds_aio_ctx);
			NETP_ASSERT(rt == netp::OK);
		}
		virtual void deinit() {
			__do_execute_act(aio_action::END_READ, m_signalfds_aio_ctx);
			m_signalfds_aio_ctx->fn_read = nullptr;
			aio_end(m_signalfds_aio_ctx);

			NETP_CLOSE_SOCKET(m_signalfds[0]);
			NETP_CLOSE_SOCKET(m_signalfds[1]);
			m_signalfds[0] = (SOCKET)NETP_INVALID_SOCKET;
			m_signalfds[1] = (SOCKET)NETP_INVALID_SOCKET;

			NETP_TRACE_IOE("[io_event_loop][default]deinit done");

			//NETP_ASSERT(m_ctxs.size() == 0);
			NETP_ASSERT(NETP_AIO_CTX_LIST_IS_EMPTY(&m_aio_ctx_list), "m_aio_ctx_list not empty");

#ifdef NETP_DEBUG_AIO_CTX_
			NETP_ASSERT(m_aio_ctx_count_alloc == m_aio_ctx_count_free);
#endif
		}

		virtual void interrupt_wait() {
			NETP_ASSERT(m_signalfds[0] > 0);
			NETP_ASSERT(m_signalfds[1] > 0);
			int ec;
			const byte_t interrutp_a[1] = { (byte_t)'i' };
			u32_t c = netp::send(netp::NETP_DEFAULT_SOCKAPI, m_signalfds[1], interrutp_a, 1, ec, 0);
			if (NETP_UNLIKELY(ec != netp::OK)) {
				NETP_WARN("[io_event_loop]interrupt send failed: %d", ec);
			}
			(void)c;
		}

		virtual int __do_execute_act(aio_action act, aio_ctx* ctx) {
			switch (act) {
			case aio_action::READ:
			{
				NETP_TRACE_IOE("[io_event_loop][type:%d][#%d]aio_action::READ", m_type, ctx->fd);
				NETP_ASSERT((ctx->flag & aio_flag::AIO_READ) == 0);
				int rt = watch(aio_flag::AIO_READ, ctx);
				if (netp::OK == rt) {
					ctx->flag |= aio_flag::AIO_READ;
				}
				return rt;
			}
			break;
			case aio_action::END_READ:
			{
				NETP_TRACE_IOE("[io_event_loop][type:%d][#%d]aio_action::END_READ", m_type, ctx->fd);
				if (ctx->flag & aio_flag::AIO_READ) {
					ctx->flag &= ~aio_flag::AIO_READ;
					//we need this condition check ,cuz epoll might fail to watch
					return unwatch(aio_flag::AIO_READ, ctx);
				}
				return netp::OK;
			}
			break;
			case aio_action::WRITE:
			{
				NETP_TRACE_IOE("[io_event_loop][type:%d][#%d]aio_action::WRITE", m_type, ctx->fd);
				NETP_ASSERT((ctx->flag & aio_flag::AIO_WRITE) == 0);
				int rt = watch(aio_flag::AIO_WRITE, ctx);
				if (netp::OK == rt) {
					ctx->flag |= aio_flag::AIO_WRITE;
				}
				return rt;
			}
			break;
			case aio_action::END_WRITE:
			{
				NETP_TRACE_IOE("[io_event_loop][type:%d][#%d]aio_action::END_WRITE", m_type, ctx->fd);
				if (ctx->flag & aio_flag::AIO_WRITE) {
					ctx->flag &= ~aio_flag::AIO_WRITE;
					//we need this condition check ,cuz epoll might fail to watch
					return unwatch(aio_flag::AIO_WRITE, ctx);
				}
				return netp::OK;
			}
			break;
			case aio_action::NOTIFY_TERMINATING:
			{
				NETP_DEBUG("[io_event_loop]notify terminating...");
				aio_ctx* _ctx, * _ctx_n;
				for (_ctx = (m_aio_ctx_list.next), _ctx_n = _ctx->next; _ctx != &(m_aio_ctx_list); _ctx = _ctx_n, _ctx_n = _ctx->next) {
					if (_ctx->fd == m_signalfds[0]) {
						continue;
					}

					NETP_ASSERT(_ctx->fd > 0);
					NETP_ASSERT(_ctx->fn_notify != nullptr);

					if (_ctx->fn_read != nullptr) {
						_ctx->fn_read(E_IO_EVENT_LOOP_NOTIFY_TERMINATING, _ctx);
					}
					if (_ctx->fn_write != nullptr) {
						_ctx->fn_write(E_IO_EVENT_LOOP_NOTIFY_TERMINATING, _ctx);
					}

					//in case , close would result in _ctx->fn_notify be nullptr
					if (_ctx->fn_notify != nullptr) {
						_ctx->fn_notify(E_IO_EVENT_LOOP_NOTIFY_TERMINATING, _ctx);
					}
				}
				NETP_DEBUG("[io_event_loop]notify terminating done");
			}
			break;
			case aio_action::READ_WRITE:
			{//for compiler warning...
			}
			break;
			}
			return netp::OK;
		}

		inline aio_ctx* aio_begin(SOCKET fd) {
				NETP_TRACE_IOE("[io_event_loop][type:%d][#%d]aio_action::BEGIN", m_type, fd);
				aio_ctx* ctx = netp::aio_ctx_allocate();
				ctx->fd = fd;
				ctx->flag = 0;
				netp::aio_ctx_list_append(&m_aio_ctx_list, ctx);
#ifdef NETP_DEBUG_AIO_CTX_
				++m_aio_ctx_count_alloc;
#endif
				return ctx;
		}

		inline void aio_end(aio_ctx* ctx) {
			NETP_TRACE_IOE("[io_event_loop][type:%d][#%d]aio_action::END", m_type, ctx->fd);
			NETP_ASSERT((ctx->fn_read == nullptr));
			NETP_ASSERT((ctx->fn_write == nullptr));
			NETP_ASSERT(ctx->fn_notify == nullptr);
			netp::aio_ctx_list_delete(ctx);
			netp::aio_ctx_deallocate(ctx);
#ifdef NETP_DEBUG_AIO_CTX_
			++m_aio_ctx_count_free;
#endif
		}

#define __LOOP_EXIT_WAITING__(_W_) (_W_.store(false, std::memory_order_release))
		virtual void poll(long long wait_in_nano, std::atomic<bool>& waiting) = 0;
		virtual int watch(u8_t,aio_ctx*) = 0;
		virtual int unwatch(u8_t, aio_ctx*) = 0;
	};
}
#endif