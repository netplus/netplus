#ifndef _NETP_IO_EVENT_LOOP_HPP
#define _NETP_IO_EVENT_LOOP_HPP

#include <vector>
#include <unordered_map>
#include <set>

#include <netp/singleton.hpp>
#include <netp/mutex.hpp>
#include <netp/thread.hpp>

#include <netp/io_event.hpp>
#include <netp/timer.hpp>

#include <netp/promise.hpp>
#include <netp/packet.hpp>
#include <netp/list.hpp>

#if defined(NETP_HAS_POLLER_EPOLL)
	#define NETP_DEFAULT_POLLER_TYPE netp::io_poller_type::T_EPOLL
#elif defined(NETP_HAS_POLLER_SELECT)
	#define NETP_DEFAULT_POLLER_TYPE netp::io_poller_type::T_SELECT
#elif defined(NETP_HAS_POLLER_KQUEUE)
	#define NETP_DEFAULT_POLLER_TYPE netp::io_poller_type::T_KQUEUE
#elif defined(NETP_HAS_POLLER_IOCP)
	#define NETP_DEFAULT_POLLER_TYPE netp::io_poller_type::T_IOCP
#else
	#error "unknown poller type"
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
		T_BYE,
		T_NONE
	};

	class io_event_loop;
	struct poller_cfg {
		u32_t ch_buf_size;
		u32_t maxiumctx;
	};
	typedef std::function< NRP<io_event_loop>(io_poller_type t, poller_cfg const& cfg) > fn_poller_maker_t;

	enum aio_flag {
		AIO_NOTIFY = 0,
		AIO_READ	=1,
		AIO_WRITE	= 1<<1,
		AIO_FLAG_MAX =3
	};

	enum class aio_action {
		READ = 1<<0, //check read, sys io
		END_READ=1<<1,

		WRITE = 1<<2, //check write, sys io
		END_WRITE =1<<3,

		NOTIFY_TERMINATING=1<<4,
		READ_WRITE = (READ | WRITE)
	};

#ifdef NETP_HAS_POLLER_IOCP
	enum class iocp_action {
		READ=1<<0,
		END_READ = 1 << 1,
		WRITE = 1 << 2,
		END_WRITE = 1 << 3,
		ACCEPT = 1 << 4,
		END_ACCEPT = 1 << 5,
		CONNECT = 1 << 6,
		END_CONNECT = 1 << 7,
		READFROM = 1<<8,
		SENDTO = 1<<9,
		BEGIN = 1 << 10,
		NOTIFY_TERMINATING = 1 << 11,
		END = 1 << 12,
		BEGIN_READ_WRITE_ACCEPT_CONNECT = (BEGIN | READ | WRITE| ACCEPT| CONNECT)
	};
#endif

#ifdef _DEBUG
	#define NETP_DEBUG_TERMINATING
	#define NETP_DEBUG_AIO_CTX_
#endif

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

	typedef std::function<void()> fn_io_event_task_t;
	typedef std::vector<fn_io_event_task_t, netp::allocator<fn_io_event_task_t>> io_task_q_t;

#ifdef NETP_HAS_POLLER_IOCP
	struct iocp_act_op {
		iocp_action act;
		SOCKET fd;
		fn_overlapped_io_event fn_overlapped;
		fn_iocp_event_t fn_iocp;
	};
	typedef std::vector<iocp_act_op, netp::allocator<act_op>> iocp_act_op_queue_t;
	typedef std::function<int(const iocp_result&)> fn_iocp_event_t;
	typedef std::function<int(void* ol)> fn_overlapped_io_event;
#endif

	enum class loop_state {
		S_IDLE,
		S_LAUNCHING,
		S_RUNNING,
		S_TERMINATING, //no more watch evt
		S_TERMINATED, //no more new timer, we need this state to make sure all channel have a chance to check terminating flag
		S_EXIT //exit..
	};

	class io_event_loop :
		public ref_base
	{
		enum LOOP_WAIT_FLAG {
			F_LOOP_WAIT_NONE_ZERO_TIME_WAIT =1,
			F_LOOP_WAIT_ENTER_WAITING = 1<<1
		};
		friend class io_event_loop_group;

	protected:
		std::thread::id m_tid;

		aio_ctx m_aio_ctx_list;

#ifdef NETP_DEBUG_AIO_CTX_
		long m_aio_ctx_count_alloc;
		long m_aio_ctx_count_free;
#endif

#ifdef NETP_HAS_POLLER_IOCP
		iocp_act_op_queue_t m_iocp_acts;
#endif

		spin_mutex m_tq_mutex;
		io_task_q_t m_tq_standby;
		io_task_q_t m_tq;
		NRP<timer_broker> m_tb;

		u8_t m_type;
		std::atomic<bool> m_waiting;
		std::atomic<u8_t> m_state;

		SOCKET m_signalfds[2];
		aio_ctx* m_signalfds_aio_ctx;

		NRP<netp::packet> m_channel_rcv_buf;
		NRP<netp::thread> m_th;

		//timer_timepoint_t m_wait_until;
		std::atomic<u16_t> m_internal_ref_count;
		poller_cfg m_cfg;

#ifdef NETP_DEBUG_TERMINATING
		bool m_terminated;
#endif
	protected:
		inline u16_t internal_ref_count() { return m_internal_ref_count.load(std::memory_order_acquire); }
		inline void __internal_ref_count_inc() { netp::atomic_incre(&m_internal_ref_count); }
		//0,	NO WAIT
		//~0,	INFINITE WAIT
		//>0,	WAIT nanosecond
		__NETP_FORCE_INLINE long long _calc_wait_dur_in_nano() {

			NETP_ASSERT( m_waiting.load(std::memory_order_acquire) == false );
			netp::timer_duration_t ndelay;
			m_tb->expire(ndelay);
			long long ndelayns = ndelay.count();
			if (ndelayns == 0
#ifdef NETP_HAS_POLLER_IOCP
//				|| m_iocp_acts.size() != 0
#else
//				|| m_acts.size() != 0 
#endif
				) {
				return 0;
			}

			lock_guard<spin_mutex> lg(m_tq_mutex);
			if (m_tq_standby.size() != 0) {
				return 0;
			}
			NETP_ASSERT( u64_t(TIMER_TIME_INFINITE) > NETP_POLLER_WAIT_IGNORE_DUR);
			if ( (u64_t(ndelayns)>NETP_POLLER_WAIT_IGNORE_DUR) ) {
				m_waiting.store(true, std::memory_order_release);
			}
			return ndelayns;
		}

		virtual void init() {
			netp::aio_ctx_list_init(&m_aio_ctx_list);

#ifdef NETP_DEBUG_AIO_CTX_
			m_aio_ctx_count_alloc = 0;
			m_aio_ctx_count_free = 0;
#endif
			m_channel_rcv_buf = netp::make_ref<netp::packet>(m_cfg.ch_buf_size);
			m_tid = std::this_thread::get_id();
			m_tb = netp::make_ref<timer_broker>();
			_do_poller_init();

#ifdef NETP_DEBUG_TERMINATING
			m_terminated = false;
#endif
		}

		virtual void deinit() {
			NETP_ASSERT(in_event_loop());
			NETP_ASSERT(m_state.load(std::memory_order_acquire) == u8_t(loop_state::S_EXIT));

			{
				lock_guard<spin_mutex> lg(m_tq_mutex);
				NETP_ASSERT(m_tq_standby.empty());
			}

			//NETP_ASSERT(m_acts.size() == 0);

			NETP_ASSERT(m_tq.empty());
			NETP_ASSERT(m_tb->size() == 0);
			m_tb = nullptr;
			_do_poller_deinit();
			
			//NETP_ASSERT(m_ctxs.size() == 0);
			NETP_ASSERT(NETP_LIST_IS_EMPTY(&m_aio_ctx_list));

#ifdef NETP_DEBUG_AIO_CTX_
			NETP_ASSERT( m_aio_ctx_count_alloc == m_aio_ctx_count_free );
#endif
		}

		virtual int __do_execute_act(aio_action act, aio_ctx* ctx) {
				switch (act) {
				case aio_action::READ:
				{ 
					NETP_TRACE_IOE("[io_event_loop][type:%d][#%d]aio_action::READ", m_type, ctx->fd);
					NETP_ASSERT((ctx->flag & aio_flag::AIO_READ) == 0);
					int rt = _do_watch( aio_flag::AIO_READ, ctx);
					if (netp::OK == rt) {
						ctx->flag |= aio_flag::AIO_READ;
					}
					return rt;
				}
				break;
				case aio_action::END_READ:
				{
					NETP_TRACE_IOE("[io_event_loop][type:%d][#%d]aio_action::END_READ", m_type, ctx->fd);
					if (ctx->flag&aio_flag::AIO_READ) {
						ctx->flag &= ~aio_flag::AIO_READ;
						//we need this condition check ,cuz epoll might fail to watch
						return _do_unwatch(aio_flag::AIO_READ, ctx);
					}
					return netp::OK;
				}
				break;
				case aio_action::WRITE:
				{
					NETP_TRACE_IOE("[io_event_loop][type:%d][#%d]aio_action::WRITE", m_type, ctx->fd);
					NETP_ASSERT((ctx->flag & aio_flag::AIO_WRITE) == 0);
					int rt = _do_watch(aio_flag::AIO_WRITE, ctx);
					if (netp::OK == rt) {
						ctx->flag |= aio_flag::AIO_WRITE;
					}
					return rt;
				}
				break;
				case aio_action::END_WRITE:
				{
					NETP_TRACE_IOE("[io_event_loop][type:%d][#%d]aio_action::END_WRITE", m_type, ctx->fd);
					if (ctx->flag&aio_flag::AIO_WRITE) {
						ctx->flag &= ~aio_flag::AIO_WRITE;
						//we need this condition check ,cuz epoll might fail to watch
						return _do_unwatch( aio_flag::AIO_WRITE, ctx);
					}
					return netp::OK;
				}
				break;
				case aio_action::NOTIFY_TERMINATING:
				{
					aio_ctx* _ctx,*_ctx_n;
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
							_ctx->fn_write(E_IO_EVENT_LOOP_NOTIFY_TERMINATING,_ctx);
						}

						//in case , close would result in _ctx->fn_notify be nullptr
						if (_ctx->fn_notify != nullptr ) {
							_ctx->fn_notify(E_IO_EVENT_LOOP_NOTIFY_TERMINATING, _ctx);
						}
					}

					//no competitor here, store directly
					NETP_ASSERT(m_state.load(std::memory_order_acquire) == u8_t(loop_state::S_TERMINATING));
					m_state.store(u8_t(loop_state::S_TERMINATED), std::memory_order_release);

					NETP_ASSERT(m_tb != nullptr);
					m_tb->expire_all();
				}
				break;
				case aio_action::READ_WRITE:
				{//for compiler warning...
				}
				break;
				}
				return netp::OK;
		}

		void __run();
		void __notify_terminating();
		int __launch();
		void __terminate();

	public:
		io_event_loop( io_poller_type t, poller_cfg const& cfg) :
			m_type(u8_t(t)),
			m_waiting(false),
			m_state(u8_t(loop_state::S_IDLE)),
			m_signalfds{ (SOCKET)NETP_INVALID_SOCKET, (SOCKET)NETP_INVALID_SOCKET },
			m_internal_ref_count(1),
			m_cfg(cfg)
		{}

		~io_event_loop() {
			NETP_ASSERT(m_tb == nullptr);
			NETP_ASSERT(m_th == nullptr);
		}

		inline void schedule(fn_io_event_task_t&& f) {
			//disable compiler order opt by barrier
			std::atomic<bool> _interrupt_poller(false);
			{
				lock_guard<spin_mutex> lg(m_tq_mutex);
				m_tq_standby.push_back(std::move(f));
				_interrupt_poller.store( m_tq_standby.size() == 1 && !in_event_loop() && m_waiting.load(std::memory_order_acquire), std::memory_order_release);
			}
			if (NETP_UNLIKELY(_interrupt_poller.load(std::memory_order_acquire))) {
				_do_poller_interrupt_wait();
			}
		}

		inline void schedule(fn_io_event_task_t const& f) {
			std::atomic<bool> _interrupt_poller(false);
			{
				lock_guard<spin_mutex> lg(m_tq_mutex);
				m_tq_standby.push_back(f);
				_interrupt_poller.store( m_tq_standby.size() == 1 && !in_event_loop() && m_waiting.load(std::memory_order_acquire), std::memory_order_release);
			}
			if (NETP_UNLIKELY(_interrupt_poller.load(std::memory_order_acquire))) {
				_do_poller_interrupt_wait();
			}
		}

		inline void execute(fn_io_event_task_t&& f) {
			if (in_event_loop()) {
				f();
				return;
			}
			schedule(std::move(f));
		}

		inline void execute(fn_io_event_task_t const& f) {
			if (in_event_loop()) {
				f();
				return;
			}
			schedule(f);
		}

		__NETP_FORCE_INLINE bool in_event_loop() const {
			return std::this_thread::get_id() == m_tid;
		}

		void launch(NRP<netp::timer> const& t , NRP<netp::promise<int>> const& lf = nullptr ) {
			if(!in_event_loop()) {
				schedule([L = NRP<io_event_loop>(this), t, lf]() {
					L->launch(t, lf);
				});
				return;
			}
			if (NETP_LIKELY(m_state.load(std::memory_order_acquire) < u8_t(loop_state::S_TERMINATED))) {
				m_tb->launch(t);
				(lf != nullptr)? lf->set(netp::OK):(void)0;
			} else {
				(lf != nullptr) ? lf->set(netp::E_IO_EVENT_LOOP_TERMINATED):NETP_THROW("DO NOT LAUNCH AFTER TERMINATED, OR PASS A PROMISE TO OVERRIDE THIS ERRO");
			}
		}

		void launch(NRP<netp::timer>&& t, NRP<netp::promise<int>> const& lf = nullptr) {
			if (!in_event_loop()) {
				schedule([L = NRP<io_event_loop>(this), t=std::move(t) , lf]() {
					L->launch(std::move(t), lf);
				});
				return;
			}
			if (NETP_LIKELY(m_state.load(std::memory_order_acquire) < u8_t(loop_state::S_TERMINATED))) {
				m_tb->launch(std::move(t));
				(lf != nullptr) ? lf->set(netp::OK) : (void)0;
			} else {
				(lf != nullptr) ? lf->set(netp::E_IO_EVENT_LOOP_TERMINATED) : NETP_THROW("DO NOT LAUNCH AFTER TERMINATED, OR PASS A PROMISE TO OVERRIDE THIS ERRO");
			}
		}

		inline io_poller_type type() const { return (io_poller_type)m_type; }
		inline int aio_do(aio_action act, aio_ctx* ctx) {
			NETP_ASSERT(in_event_loop());
			if ( ((u8_t(act)&u8_t(aio_action::READ_WRITE)) == 0) || m_state.load(std::memory_order_acquire) < u8_t(loop_state::S_TERMINATING) ) {
				__do_execute_act(act, ctx);
				return netp::OK;
			} else {
				return netp::E_IO_EVENT_LOOP_TERMINATED;
			}
		}
		inline aio_ctx* aio_begin(SOCKET fd) {
			NETP_ASSERT(in_event_loop());
			if ( m_state.load(std::memory_order_acquire) < u8_t(loop_state::S_TERMINATING)) {
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
			return 0;
		}
		inline void aio_end(aio_ctx* ctx) {
			NETP_ASSERT(in_event_loop());
			NETP_TRACE_IOE("[io_event_loop][type:%d][#%d]aio_action::END", m_type, ctx->fd);
			NETP_ASSERT((ctx->fn_read == nullptr));
			NETP_ASSERT((ctx->fn_write == nullptr));
			NETP_ASSERT( ctx->fn_notify == nullptr);
			netp::aio_ctx_list_delete(ctx);
			netp::aio_ctx_deallocate(ctx);
#ifdef NETP_DEBUG_AIO_CTX_
			++m_aio_ctx_count_free;
#endif
		}

		__NETP_FORCE_INLINE NRP<netp::packet> const& channel_rcv_buf() const {
			return m_channel_rcv_buf;
		}

#ifdef NETP_HAS_POLLER_IOCP
		inline void iocp_do( iocp_action act, SOCKET fd, fn_overlapped_io_event const& fn_overlapped, fn_iocp_event_t const& fn) {
			NETP_ASSERT(fd != NETP_INVALID_SOCKET);
			NETP_ASSERT(in_event_loop());
			if (((u16_t(act) & u16_t(iocp_action::BEGIN_READ_WRITE_ACCEPT_CONNECT)) == 0) || m_state.load(std::memory_order_acquire) < u8_t(loop_state::S_TERMINATING)) {
				m_iocp_acts.push_back({act,fd, (fn_overlapped), (fn) });
			} else {
				fn({ fd, netp::E_IO_EVENT_LOOP_TERMINATED,0 });
			}
		}
		inline void iocp_do(iocp_action act, SOCKET fd, fn_overlapped_io_event&& fn_overlapped, fn_iocp_event_t&& fn) {
			NETP_ASSERT(fd != NETP_INVALID_SOCKET);
			NETP_ASSERT(in_event_loop());
			if (((u16_t(act) & u16_t(iocp_action::BEGIN_READ_WRITE_ACCEPT_CONNECT)) == 0) || m_state.load(std::memory_order_acquire) < u8_t(loop_state::S_TERMINATING)) {
				m_iocp_acts.push_back({ act,fd, std::move(fn_overlapped), std::move(fn) });
			}
			else {
				fn({ fd, netp::E_IO_EVENT_LOOP_TERMINATED,0 });
			}
		}
#endif

	protected:
	#define __LOOP_EXIT_WAITING__() (m_waiting.store(false, std::memory_order_release))

		virtual void _do_poller_init();
		virtual void _do_poller_deinit() ;
		virtual void _do_poller_interrupt_wait() ;

		virtual void _do_poll(long long wait_in_nano ) = 0;
		virtual int _do_watch(u8_t, aio_ctx* ) = 0;
		virtual int _do_unwatch(u8_t, aio_ctx* ) = 0;
	};

	class bye_event_loop :
		public netp::io_event_loop
	{
		public:
			bye_event_loop(io_poller_type t, poller_cfg const& cfg):
				io_event_loop(t,cfg)
			{}

		protected:
			void _do_poller_init() override {}
			void _do_poller_deinit() override {}
			void _do_poller_interrupt_wait() override { NETP_ASSERT(!in_event_loop());}

			void _do_poll(long long wait_in_nano)  override;
			int _do_watch(u8_t, aio_ctx*)  override;
			int _do_unwatch(u8_t, aio_ctx*) override;
	};

	class app;
	typedef std::vector<NRP<io_event_loop>> io_event_loop_vector;
	class io_event_loop_group:
		public netp::singleton<io_event_loop_group>
	{
		friend class netp::app;
		enum class bye_event_loop_state {
			S_IDLE,
			S_PREPARING,
			S_RUNNING,
			S_EXIT
		};

	private:
		netp::shared_mutex m_pollers_mtx[T_POLLER_MAX];
		std::atomic<u32_t> m_curr_poller_idx[T_POLLER_MAX];
		io_event_loop_vector m_pollers[T_POLLER_MAX];

		int m_bye_ref_count;
		std::atomic<bye_event_loop_state> m_bye_state;
		NRP<io_event_loop> m_bye_event_loop;


		void init( int count[io_poller_type::T_POLLER_MAX], poller_cfg cfgs[io_poller_type::T_POLLER_MAX]);
		void deinit();

	public:
		io_event_loop_group();
		~io_event_loop_group();

		void notify_terminating(io_poller_type t);
		void dealloc_remove_poller(io_poller_type t);
		void alloc_add_poller(io_poller_type t, int count, poller_cfg const& cfg, fn_poller_maker_t const& fn_maker = nullptr);
		io_poller_type query_available_custom_poller_type();
		netp::size_t size(io_poller_type t);
		NRP<io_event_loop> next(io_poller_type t, std::set<NRP<io_event_loop>> const& exclude_this_set_if_have_more);

		NRP<io_event_loop> next(io_poller_type t = NETP_DEFAULT_POLLER_TYPE);
		NRP<io_event_loop> internal_next(io_poller_type t = NETP_DEFAULT_POLLER_TYPE);

		void execute(fn_io_event_task_t&& f, io_poller_type = NETP_DEFAULT_POLLER_TYPE);
		void schedule(fn_io_event_task_t&& f, io_poller_type = NETP_DEFAULT_POLLER_TYPE);
		void launch(NRP<netp::timer> const& t, NRP<netp::promise<int>> const& lf = nullptr, io_poller_type = NETP_DEFAULT_POLLER_TYPE);
	};
}
#endif
