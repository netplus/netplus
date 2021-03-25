#ifndef _NETP_POLLER_ABSTRACT_HPP
#define _NETP_POLLER_ABSTRACT_HPP

#include <netp/core.hpp>
#include <netp/smart_ptr.hpp>
#include <netp/address.hpp>
#include <netp/socket_api.hpp>

#ifdef _DEBUG
	#define NETP_DEBUG_TERMINATING
	#define NETP_DEBUG_IO_CTX_
#endif

//in nano
#define NETP_POLLER_WAIT_IGNORE_DUR (27)
namespace netp {

	template<class list_t>
	inline static void list_init(list_t* list) {
		list->next = list;
		list->prev = list;
	}
	template<class list_t>
	inline static void __list_insert(list_t* prev, list_t* next, list_t* item) {
		item->next = next;
		item->prev = prev;
		next->prev = item;
		prev->next = item;
	}
	template<class list_t>
	inline static void list_prepend(list_t* list, list_t* item) {
		__list_insert(list, list->next, item);
	}
	template<class list_t>
	inline static void list_append(list_t* list, list_t* item) {
		__list_insert(list->prev, list, item);
	}
	template<class list_t>
	inline static void list_delete(list_t* item) {
		item->prev->next = item->next;
		item->next->prev = item->prev;
		item->next = 0;
		item->prev = 0;
	}
#define NETP_IO_CTX_LIST_IS_EMPTY(list) ( ((list) == (list)->next) && ((list)==(list)->prev) )

	enum io_poller_type {
		T_SELECT, //win&linux&android
		T_IOCP, //win
		T_EPOLL, //linux,epoll,et
		T_KQUEUE,//bsd
		T_POLLER_CUSTOM_1,
		T_POLLER_CUSTOM_2,
		T_POLLER_MAX,
		T_NONE
	};

	enum io_flag {
		IO_NOTIFY = 0,
		IO_READ = 1,
		IO_WRITE = 1 << 1,
		IO_FLAG_MAX = 3
	};

	enum class io_action {
		READ = 1 << 0, //check read, sys io
		END_READ = 1 << 1,

		WRITE = 1 << 2, //check write, sys io
		END_WRITE = 1 << 3,

		NOTIFY_TERMINATING = 1 << 4,

		READ_WRITE = (READ | WRITE)
	};

	struct io_ctx;
	typedef std::function<void(int status, io_ctx* ctx)> fn_io_event_t;
	class poller_abstract:
		public netp::ref_base
	{
	protected:

	public:
		poller_abstract() {}
		~poller_abstract() {}

		virtual void init() = 0;
		virtual void deinit() = 0;

#define __LOOP_EXIT_WAITING__(_W_) (_W_.store(false, std::memory_order_release))
		virtual void poll(long long wait_in_nano, std::atomic<bool>& waiting) = 0;

		virtual void interrupt_wait() = 0;
		virtual int io_do(io_action, io_ctx*) = 0;
		virtual io_ctx* io_begin(SOCKET) = 0;
		virtual void io_end(io_ctx*) = 0;
		virtual int watch(u8_t,io_ctx*) = 0;
		virtual int unwatch(u8_t, io_ctx*) = 0;
	};

	struct io_ctx
	{
		io_ctx* prev, *next;
		SOCKET fd;
		u8_t flag;
		fn_io_event_t fn_read;
		fn_io_event_t fn_write;
		fn_io_event_t fn_notify;
	};

	inline static io_ctx* io_ctx_allocate(SOCKET fd) {
		io_ctx* ctx = netp::allocator<io_ctx>::malloc(1);
		ctx->fd = fd;
		ctx->flag = 0;
		new ((fn_io_event_t*)&(ctx->fn_read))(fn_io_event_t)();
		new ((fn_io_event_t*)&(ctx->fn_write))(fn_io_event_t)();
		new ((fn_io_event_t*)&(ctx->fn_notify))(fn_io_event_t)();
		return ctx;
	}

	inline static void io_ctx_deallocate(io_ctx* ctx) {
		NETP_ASSERT(ctx->fn_read == nullptr);
		NETP_ASSERT(ctx->fn_write == nullptr);
		NETP_ASSERT(ctx->fn_notify == nullptr);
		netp::allocator<io_ctx>::free(ctx);
	}

	class poller_interruptable_by_fd : public poller_abstract {
	public:
		io_ctx m_io_ctx_list;
		SOCKET m_signalfds[2];
		io_ctx* m_signalfds_io_ctx;

#ifdef NETP_DEBUG_IO_CTX_
		long m_io_ctx_count_alloc;
		long m_io_ctx_count_free;
#endif

		poller_interruptable_by_fd():
			poller_abstract(),
			m_signalfds{ (SOCKET)NETP_INVALID_SOCKET, (SOCKET)NETP_INVALID_SOCKET },
			m_signalfds_io_ctx(0)
#ifdef NETP_DEBUG_IO_CTX_
			,m_io_ctx_count_alloc(0),
			m_io_ctx_count_free(0)
#endif
		{}
		~poller_interruptable_by_fd() {}

		void __init_interrupt_fd() {
			int rt = netp::socketpair(int(NETP_AF_INET), int(NETP_SOCK_STREAM), int(NETP_PROTOCOL_TCP), m_signalfds);
			NETP_ASSERT(rt == netp::OK, "rt: %d", rt);

			rt = netp::turnon_nonblocking(netp::default_socket_api, m_signalfds[0]);
			NETP_ASSERT(rt == netp::OK, "rt: %d", rt);

			rt = netp::turnon_nonblocking(netp::default_socket_api, m_signalfds[1]);
			NETP_ASSERT(rt == netp::OK, "rt: %d", rt);

			rt = netp::turnon_nodelay(netp::default_socket_api, m_signalfds[1]);
			NETP_ASSERT(rt == netp::OK, "rt: %d", rt);

			NETP_ASSERT(rt == netp::OK);

			m_signalfds_io_ctx = io_begin(m_signalfds[0]);
			NETP_ASSERT(m_signalfds_io_ctx != 0);
			m_signalfds_io_ctx->fn_read = [](int status, io_ctx* ctx) {
				if (status == netp::OK) {
					byte_t tmp[1];
					int ec = netp::OK;
					do {
						u32_t c = netp::recv(netp::default_socket_api, ctx->fd, tmp, 1, ec, 0);
						if (c == 1) {
							NETP_ASSERT(ec == netp::OK);
							NETP_ASSERT(tmp[0] == 'i', "c: %d", tmp[0]);
						}
					} while (ec == netp::OK);
				}
				return netp::OK;
			};
			rt = io_do(io_action::READ, m_signalfds_io_ctx);
			NETP_ASSERT(rt == netp::OK);
	}
		void __deinit_interrupt_fd() {
			io_do(io_action::END_READ, m_signalfds_io_ctx);
			m_signalfds_io_ctx->fn_read = nullptr;
			io_end(m_signalfds_io_ctx);

			NETP_CLOSE_SOCKET(m_signalfds[0]);
			NETP_CLOSE_SOCKET(m_signalfds[1]);
			m_signalfds[0] = (SOCKET)NETP_INVALID_SOCKET;
			m_signalfds[1] = (SOCKET)NETP_INVALID_SOCKET;

			NETP_TRACE_IOE("[io_event_loop][default]deinit done");

			//NETP_ASSERT(m_ctxs.size() == 0);
			NETP_ASSERT(NETP_IO_CTX_LIST_IS_EMPTY(&m_io_ctx_list), "m_io_ctx_list not empty");
		}

		void init() {
			netp::list_init(&m_io_ctx_list);
#ifdef NETP_DEBUG_IO_CTX_
			m_io_ctx_count_alloc = 0;
			m_io_ctx_count_free = 0;
#endif
			__init_interrupt_fd();
		}

		void deinit() {
			__deinit_interrupt_fd();
#ifdef NETP_DEBUG_IO_CTX_
			NETP_ASSERT(m_io_ctx_count_alloc == m_io_ctx_count_free);
#endif
		}

		virtual void interrupt_wait() override {
			NETP_ASSERT(m_signalfds[0] > 0);
			NETP_ASSERT(m_signalfds[1] > 0);
			int ec;
			const byte_t interrutp_a[1] = { (byte_t)'i' };
			u32_t c = netp::send(netp::default_socket_api, m_signalfds[1], interrutp_a, 1, ec, 0);
			if (NETP_UNLIKELY(ec != netp::OK)) {
				NETP_WARN("[io_event_loop]interrupt send failed: %d", ec);
			}
			(void)c;
		}

		virtual io_ctx* io_begin(SOCKET fd) override {
			io_ctx* ctx = netp::io_ctx_allocate(fd);
			netp::list_append(&m_io_ctx_list, ctx);

#ifdef NETP_DEBUG_IO_CTX_
			++m_io_ctx_count_alloc;
#endif
			return ctx;
		}

		virtual void io_end(io_ctx* ctx) override {
			netp::list_delete(ctx);
			netp::io_ctx_deallocate(ctx);

#ifdef NETP_DEBUG_IO_CTX_
			++m_io_ctx_count_free;
#endif
		}

		virtual int io_do(io_action act, io_ctx* ctx) override {
			switch (act) {
			case io_action::READ:
			{
				NETP_TRACE_IOE("[io_event_loop][#%d]io_action::READ", ctx->fd);
				NETP_ASSERT((ctx->flag & io_flag::IO_READ) == 0);
				int rt = watch(io_flag::IO_READ, ctx);
				if (netp::OK == rt) {
					ctx->flag |= io_flag::IO_READ;
				}
				return rt;
			}
			break;
			case io_action::END_READ:
			{
				NETP_TRACE_IOE("[io_event_loop][type:%d][#%d]io_action::END_READ", ctx->fd);
				if (ctx->flag & io_flag::IO_READ) {
					ctx->flag &= ~io_flag::IO_READ;
					//we need this condition check ,cuz epoll might fail to watch
					return unwatch(io_flag::IO_READ, ctx);
				}
				return netp::OK;
			}
			break;
			case io_action::WRITE:
			{
				NETP_TRACE_IOE("[io_event_loop][type:%d][#%d]io_action::WRITE", ctx->fd);
				NETP_ASSERT((ctx->flag & io_flag::IO_WRITE) == 0);
				int rt = watch(io_flag::IO_WRITE, ctx);
				if (netp::OK == rt) {
					ctx->flag |= io_flag::IO_WRITE;
				}
				return rt;
			}
			break;
			case io_action::END_WRITE:
			{
				NETP_TRACE_IOE("[io_event_loop][type:%d][#%d]io_action::END_WRITE", ctx->fd);
				if (ctx->flag & io_flag::IO_WRITE) {
					ctx->flag &= ~io_flag::IO_WRITE;
					//we need this condition check ,cuz epoll might fail to watch
					return unwatch(io_flag::IO_WRITE, ctx);
				}
				return netp::OK;
			}
			break;
			case io_action::NOTIFY_TERMINATING:
			{
				NETP_DEBUG("[io_event_loop]notify terminating...");
				io_ctx* _ctx, * _ctx_n;
				for (_ctx = (m_io_ctx_list.next), _ctx_n = _ctx->next; _ctx != &(m_io_ctx_list); _ctx = _ctx_n, _ctx_n = _ctx->next) {
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
			case io_action::READ_WRITE:
			{//for compiler warning...
			}
			break;
			}
			return netp::OK;
		}
	};
}
#endif