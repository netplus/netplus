#ifndef _NETP_IO_EVENT_LOOP_HPP
#define _NETP_IO_EVENT_LOOP_HPP

#include <vector>
#include <unordered_map>
#include <set>

#include <netp/singleton.hpp>
#include <netp/mutex.hpp>
#include <netp/thread.hpp>

#include <netp/timer.hpp>

#include <netp/promise.hpp>
#include <netp/packet.hpp>
#include <netp/poller_abstract.hpp>

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

	typedef std::function<void()> fn_io_event_task_t;
	typedef std::vector<fn_io_event_task_t, netp::allocator<fn_io_event_task_t>> io_task_q_t;

	struct event_loop_cfg {
		u32_t ch_buf_size;
	};

	class io_event_loop;
	typedef std::function< NRP<io_event_loop>(event_loop_cfg const& cfg) > fn_event_loop_maker_t;

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
		std::atomic<bool> m_waiting;
		std::atomic<u8_t> m_state;
		io_poller_type m_type;

		NRP<poller_abstract> m_poller;
		NRP<timer_broker> m_tb;

		spin_mutex m_tq_mutex;
		io_task_q_t m_tq_standby;
		io_task_q_t m_tq;
		std::thread::id m_tid;

		NRP<netp::packet> m_channel_rcv_buf;
		NRP<netp::thread> m_th;

		//timer_timepoint_t m_wait_until;
		std::atomic<u16_t> m_internal_ref_count;
		event_loop_cfg m_cfg;

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

			NETP_ASSERT( m_waiting.load(std::memory_order_acquire) == false, "_calc_wait_dur_in_nano waiting check failed" );
			netp::timer_duration_t ndelay;
			m_tb->expire(ndelay);
			long long ndelayns = ndelay.count();
			if (ndelayns == 0) {
				return 0;
			}

			lock_guard<spin_mutex> lg(m_tq_mutex);
			if (m_tq_standby.size() != 0) {
				return 0;
			}
			NETP_ASSERT( u64_t(TIMER_TIME_INFINITE) > (NETP_POLLER_WAIT_IGNORE_DUR));
			if ( (u64_t(ndelayns)>(NETP_POLLER_WAIT_IGNORE_DUR)) ) {
				m_waiting.store(true, std::memory_order_release);
			}
			return ndelayns;
		}

		virtual void init() {

			m_channel_rcv_buf = netp::make_ref<netp::packet>(m_cfg.ch_buf_size);
			m_tid = std::this_thread::get_id();
			m_tb = netp::make_ref<timer_broker>();
			
			m_poller->init();

#ifdef NETP_DEBUG_TERMINATING
			m_terminated = false;
#endif
		}

		virtual void deinit() {
			NETP_DEBUG("[io_event_loop]deinit begin");
			NETP_ASSERT(in_event_loop());
			NETP_ASSERT(m_state.load(std::memory_order_acquire) == u8_t(loop_state::S_EXIT), "event loop deinit state check failed");

			{
				lock_guard<spin_mutex> lg(m_tq_mutex);
				NETP_ASSERT(m_tq_standby.empty());
			}

			NETP_ASSERT(m_tq.empty());
			NETP_ASSERT(m_tb->size() == 0);
			m_tb = nullptr;

			m_poller->deinit();
			NETP_DEBUG("[io_event_loop]deinit done");
		}

		void __run();
		void __do_notify_terminating();
		void __notify_terminating();
		int __launch();
		void __terminate();

	public:
		io_event_loop(io_poller_type t, NRP<poller_abstract> const& poller, event_loop_cfg const& cfg) :
			m_waiting(false),
			m_state(u8_t(loop_state::S_IDLE)),
			m_type(t),
			m_poller(poller),
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
				m_poller->interrupt_wait();
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
				m_poller->interrupt_wait();
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

		inline io_poller_type poller_type() const { return m_type; }

		__NETP_FORCE_INLINE NRP<netp::packet> const& channel_rcv_buf() const {
			return m_channel_rcv_buf;
		}

		inline int aio_do(aio_action act, aio_ctx* ctx) {
			NETP_ASSERT(in_event_loop());
			if (((u8_t(act) & u8_t(aio_action::READ_WRITE)) == 0) || m_state.load(std::memory_order_acquire) < u8_t(loop_state::S_TERMINATING)) {
				m_poller->aio_do(act, ctx);
				return netp::OK;
			} else {
				return netp::E_IO_EVENT_LOOP_TERMINATED;
			}
		}
		inline aio_ctx* aio_begin(SOCKET fd) {
			NETP_ASSERT(in_event_loop());
			if (m_state.load(std::memory_order_acquire) < u8_t(loop_state::S_TERMINATING)) {
				return m_poller->aio_begin(fd);
			}
			return 0;
		}
		inline void aio_end(aio_ctx* ctx) {
			NETP_ASSERT(in_event_loop());
			m_poller->aio_end(ctx);
		}
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
		netp::shared_mutex m_loop_mtx[T_POLLER_MAX];
		std::atomic<u32_t> m_curr_loop_idx[T_POLLER_MAX];
		io_event_loop_vector m_loop[T_POLLER_MAX];

		int m_bye_ref_count;
		std::atomic<bye_event_loop_state> m_bye_state;
		NRP<io_event_loop> m_bye_event_loop;


		void init( int count[io_poller_type::T_POLLER_MAX], event_loop_cfg cfgs[io_poller_type::T_POLLER_MAX]);
		void deinit();

	public:
		io_event_loop_group();
		~io_event_loop_group();

		void notify_terminating(io_poller_type t);
		void dealloc_remove_event_loop(io_poller_type t);
		void alloc_add_event_loop(io_poller_type t, int count, event_loop_cfg const& cfg, fn_event_loop_maker_t const& fn_maker = nullptr);
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
