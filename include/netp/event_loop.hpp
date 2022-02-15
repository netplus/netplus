#ifndef _NETP_IO_EVENT_LOOP_HPP
#define _NETP_IO_EVENT_LOOP_HPP

#include <vector>
#include <set>

#include <netp/core.hpp>
#include <netp/string.hpp>
#include <netp/smart_ptr.hpp>
#include <netp/mutex.hpp>

#include <netp/promise.hpp>
#include <netp/packet.hpp>
#include <netp/poller_abstract.hpp>
#include <netp/timer.hpp>
#include <netp/dns_resolver.hpp>

#if defined(NETP_HAS_POLLER_EPOLL)
#define NETP_DEFAULT_POLLER_TYPE netp::io_poller_type::T_EPOLL
#elif defined(NETP_HAS_POLLER_SELECT)
#define NETP_DEFAULT_POLLER_TYPE netp::io_poller_type::T_SELECT
#elif defined(NETP_HAS_POLLER_KQUEUE)
#define NETP_DEFAULT_POLLER_TYPE netp::io_poller_type::T_KQUEUE
#elif defined(NETP_HAS_POLLER_IOCP)
#define NETP_DEFAULT_POLLER_TYPE netp::io_poller_type::T_IOCP
#define NETP_DEFAULT_POLLER_TYPE_IOCP
#else
#error "unknown poller type"
#endif

//#define NETP_DEBUG_LOOP_TIME

namespace netp {

	typedef std::function<void()> fn_task_t;
	typedef std::vector<fn_task_t, netp::allocator<fn_task_t>> io_task_q_t;

	enum event_loop_flag {
		f_th_thread_affinity =1<<0,
		f_th_priority_above_normal =1<<1,
		f_th_priority_time_critical = 1 << 2,
		f_enable_dns_resolver =1<<3
	};

	struct event_loop_cfg {
		explicit event_loop_cfg(u8_t type_, u8_t flag_, u32_t read_buf_) :
			type(type_),
			flag(flag_),
			thread_affinity(0),
			no_wait_us(1),
			channel_read_buf_size(read_buf_)
		{}

		//u16_t no_wait_us wide used construct 
		explicit event_loop_cfg(u8_t type_, u8_t flag_, u32_t read_buf_, u8_t no_wait_us_) :
			type(type_),
			flag(flag_),
			thread_affinity(0),
			no_wait_us(u8_t(no_wait_us_)),
			channel_read_buf_size(read_buf_)
		{}

		u8_t type;
		u8_t flag;
		u8_t thread_affinity;
		u8_t no_wait_us;
		u32_t channel_read_buf_size;
		std::vector<netp::string_t, netp::allocator<netp::string_t>> dns_hosts;
	};

	class event_loop;
	typedef std::function<NRP<event_loop>(event_loop_cfg const& cfg) > fn_event_loop_maker_t;
	extern NRP<event_loop> default_event_loop_maker(event_loop_cfg const& cfg);

	enum class loop_state {
		S_IDLE,
		S_LAUNCHING,
		S_RUNNING,
		S_TERMINATING, //no more watch evt
		S_TERMINATED, //no more new timer, we need this state to make sure all channel have a chance to check terminating flag
		S_EXIT //exit..
	};

//	using loop_rcv_buf_packet_t = netp::cap_fix_packet<netp::non_atomic_ref_base, 0, 0xffff, NETP_DEFAULT_ALIGN>;
	//using loop_rcv_buf_packet_t = packet;

	class dns_resolver;
	class timer_broker;
	class thread;
	class event_loop :
		public ref_base
	{
		enum LOOP_WAIT_FLAG {
			F_LOOP_WAIT_NONE_ZERO_TIME_WAIT =1<<0,
			F_LOOP_WAIT_ENTER_WAITING = 1<<1
		};
		friend class event_loop_group;

	protected:
		std::thread::id m_tid;
		std::atomic<bool> m_waiting;
		std::atomic<u8_t> m_state;

		const NRP<poller_abstract> m_poller;
		NRP<timer_broker> m_tb;
		NRP<dns_resolver> m_dns_resolver;
		NRP<netp::packet> m_channel_rcv_buf;
		NRP<netp::thread> m_th;

		int m_io_ctx_count;
		int m_io_ctx_count_before_running;
		std::atomic<long> m_internal_ref_count;
#ifdef NETP_DEBUG_LOOP_TIME
		long long m_loop_last_tp;
		long long m_last_wait;
#endif

		spin_mutex m_tq_mutex;
		io_task_q_t m_tq_standby;
		io_task_q_t m_tq;

		//timer_timepoint_t m_wait_until;
		event_loop_cfg m_cfg;
		std::vector<netp::string_t, netp::allocator<netp::string_t>> m_dns_hosts;
	protected:
		//the ref count is used by main thread usually,
		//but if we run netp on a platform that the enter|exit thread is uncertain, we should pay attention on the start|stop of the grop
		//
		inline long internal_ref_count() { return m_internal_ref_count.load(std::memory_order_relaxed); }
		inline void store_internal_ref_count( long count ) { m_internal_ref_count.store( count, std::memory_order_relaxed); }
		inline void inc_internal_ref_count() { m_internal_ref_count.fetch_add(1, std::memory_order_relaxed); }

		//0,	NO WAIT
		//~0,	INFINITE WAIT
		//>0,	WAIT nanosecond
		i64_t _calc_wait_dur_in_nano() {
			NETP_ASSERT( m_waiting.load(std::memory_order_relaxed) == false, "_calc_wait_dur_in_nano waiting check failed" );
			static_assert(TIMER_TIME_INFINITE == i64_t(-1), "timer infinite check");
			netp::timer_duration_t ndelay;
			m_tb->expire(ndelay);
			i64_t ndelayns = i64_t(ndelay.count());
			//@note: opt for select, epoll_wait
			//@note: select, epoll_wait cost too much time to return (ms level)
			if ( (ndelayns != TIMER_TIME_INFINITE) && (ndelayns <= (i64_t(m_cfg.no_wait_us)*1000LL)) ) {
				//less than 1us
#ifdef NETP_DEBUG_LOOP_TIME
				m_last_wait = 0;
#endif
				return 0;
			}

			{
				lock_guard<spin_mutex> lg(m_tq_mutex);
				if (m_tq_standby.size() != 0) {
#ifdef NETP_DEBUG_LOOP_TIME
					m_last_wait = 0;
#endif
					return 0;
				}
				NETP_POLLER_WAIT_ENTER(m_waiting);
			}
#ifdef NETP_DEBUG_LOOP_TIME
			m_last_wait = ndelayns;
#endif
			return ndelayns;
		}

		virtual void init();
		virtual void deinit();

		void __run();
		void __do_notify_terminating();
		void __notify_terminating();		
		void __do_enter_terminated();

		int __launch();
		void __terminate();

	public:
		event_loop(event_loop_cfg const& cfg, NRP<poller_abstract> const& poller);
		~event_loop();

		__NETP_FORCE_INLINE
		u8_t thread_affinity() const { return m_cfg.thread_affinity; }

		__NETP_FORCE_INLINE
		u8_t poller_type() const { return m_cfg.type; }

		__NETP_FORCE_INLINE
		NRP<netp::packet>& channel_rcv_buf() {
			return m_channel_rcv_buf;
		}

		__NETP_FORCE_INLINE
		u32_t channel_rcv_buf_size() const {
			return m_cfg.channel_read_buf_size;
		}

		__NETP_FORCE_INLINE
		NRP<dns_query_promise> resolve(string_t const& domain) {
			NETP_ASSERT(m_cfg.flag & f_enable_dns_resolver);
			return m_dns_resolver->resolve(domain);
		}

//#define _NETP_DUMP_SCHEDULE_COST
		/*win10 output
		[event_loop]schedule, cost: 122800 ns, interrupted: 1
		[event_loop]schedule, cost: 76400 ns, interrupted: 1
		[event_loop]schedule, cost: 64200 ns, interrupted: 1
		[event_loop]schedule, cost: 300 ns, interrupted: 0
		[event_loop]schedule, cost: 1000 ns, interrupted: 0
		[event_loop]schedule, cost: 100 ns, interrupted: 0
		[event_loop]schedule, cost: 1100 ns, interrupted: 0
		[event_loop]schedule, cost: 47100 ns, interrupted: 1
		[event_loop]schedule, cost: 39800 ns, interrupted: 1
		[event_loop]schedule, cost: 200 ns, interrupted: 0
		[event_loop]schedule, cost: 1400 ns, interrupted: 0
		[event_loop]schedule, cost: 1400 ns, interrupted: 0
		[event_loop]schedule, cost: 500 ns, interrupted: 0
		[event_loop]schedule, cost: 500 ns, interrupted: 0
		[event_loop]schedule, cost: 100 ns, interrupted: 0
		[event_loop]schedule, cost: 47000 ns, interrupted: 1
		[event_loop]schedule, cost: 100 ns, interrupted: 0
		*/
		inline void schedule(fn_task_t&& f) {
			//NOTE: upate on 2021/04/03
			//lock_guard of m_tq_mutex also works as a memory barrier for memory accesses across loops in between task caller and task callee

			//NOTE: update on 2021/03/28
			//lock_guard of m_tq_mutex would pose a load/store memory barrier at least
			//these kinds of barrier could make sure that the compiler would not reorder the instruction around the barrier, so interrupt_poller would always have a false initial value
			//as  a per thread variable, the reorder brought by CPU should not be a issue for _interrupt_poller
#ifdef _NETP_DUMP_SCHEDULE_COST
			long long __begin = netp::now<std::chrono::nanoseconds, netp::steady_clock_t>().time_since_epoch().count();
#endif
			bool _interrupt_poller = false;
			{
				lock_guard<spin_mutex> lg(m_tq_mutex);
				m_tq_standby.emplace_back(std::move(f));
				_interrupt_poller=( m_tq_standby.size() == 1 && !in_event_loop() && m_waiting.load(std::memory_order_relaxed));
			}
			if (_interrupt_poller) {
				m_poller->interrupt_wait();
			}
#ifdef _NETP_DUMP_SCHEDULE_COST
			long long __end = netp::now<std::chrono::nanoseconds, netp::steady_clock_t>().time_since_epoch().count();
			printf("[event_loop]schedule, cost: %llu ns, interrupted: %d\n", __end - __begin, _interrupt_poller);
#endif
		}

		inline void schedule(fn_task_t const& f) {
			bool _interrupt_poller = false;
			{
				lock_guard<spin_mutex> lg(m_tq_mutex);
				m_tq_standby.push_back(f);
				_interrupt_poller=( m_tq_standby.size() == 1 && !in_event_loop() && m_waiting.load(std::memory_order_relaxed));
			}
			if (NETP_UNLIKELY(_interrupt_poller)) {
				m_poller->interrupt_wait();
			}
		}

		inline void execute(fn_task_t&& f) {
			if (in_event_loop()) {
				f();
				return;
			}
			schedule(std::move(f));
		}

		inline void execute(fn_task_t const& f) {
			if (in_event_loop()) {
				f();
				return;
			}
			schedule(f);
		}

		__NETP_FORCE_INLINE
		const bool in_event_loop() const {
			/*
			* @note: 
			* linux: The thread ID returned by pthread_self() is not the same thing as the kernel thread ID returned by a call to gettid(2). pthread_self just return the relative address to thre process address
			* win: The KPCR of the current CPU is always accessible at fs:[0] on the x86 systems and gs:[0] on x64 systems. Commonly used kernel functions like PsGetCurrentProcess() and KeGetCurrentThread() retrieve information from the KPCR using the FS/GS relative accesses.
			*/

			return std::this_thread::get_id() == m_tid;
		}

		void launch(NRP<netp::timer> const& t , NRP<netp::promise<int>> const& lf = nullptr ) {
			if(!in_event_loop()) {
				netp::timer_clock_t::time_point outer_loop_tp = netp::timer_clock_t::now();
				schedule([L = NRP<event_loop>(this), t, lf, outer_loop_tp]() {
					netp::timer_clock_t::time_point inner_loop_tp = netp::timer_clock_t::now();
					timer_duration_t tdur = t->get_delay();
					tdur = tdur - (inner_loop_tp - outer_loop_tp);
					if (tdur < timer_duration_t(0)) { tdur = timer_duration_t(0); }
					t->set_delay(tdur);
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
				schedule([L = NRP<event_loop>(this), t_=std::move(t) , lf]() {
					L->launch(t_, lf);
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

		inline int io_do(io_action act, io_ctx* ctx) {
#ifdef _NETP_DEBUG
			NETP_ASSERT(in_event_loop());
#endif
			if (((u8_t(act) & u8_t(io_action::READ_WRITE)) == 0) || m_state.load(std::memory_order_acquire) < u8_t(loop_state::S_TERMINATING)) {
				return m_poller->io_do(act, ctx);
			} else {
				return netp::E_IO_EVENT_LOOP_TERMINATED;
			}
		}
		inline io_ctx* io_begin(SOCKET fd, NRP<io_monitor> const& iom) {
#ifdef _NETP_DEBUG
			NETP_ASSERT(in_event_loop());
#endif
			if (m_state.load(std::memory_order_acquire) < u8_t(loop_state::S_TERMINATING)) {
				io_ctx* _ctx= m_poller->io_begin(fd, iom);
				if (NETP_LIKELY(_ctx != nullptr)) {
					++m_io_ctx_count;
				}
				return _ctx;
			}
			return 0;
		}
		inline void io_end(io_ctx* ctx) {
#ifdef _NETP_DEBUG
			NETP_ASSERT(in_event_loop());
#endif
			m_poller->io_end(ctx);

			if ( (--m_io_ctx_count == m_io_ctx_count_before_running) && m_state.load(std::memory_order_acquire) == u8_t(loop_state::S_TERMINATING)) {
				__do_enter_terminated();
			}
		}
	};

	class app;
	typedef std::vector<NRP<event_loop>, netp::allocator<NRP<event_loop>>> event_loop_vector_t;
	class event_loop_group:
		public non_atomic_ref_base
	{
		friend class netp::app;
		enum class bye_event_loop_state {
			S_IDLE,
			S_PREPARING,
			S_RUNNING,
			S_EXIT
		};

	private:
		netp::shared_mutex m_loop_mtx;
		std::atomic<u32_t> m_curr_loop_idx;
		event_loop_vector_t m_loop;

		std::atomic<bye_event_loop_state> m_bye_state;
		NRP<event_loop> m_bye_event_loop;
		long m_bye_ref_count;
		event_loop_cfg m_cfg;
		fn_event_loop_maker_t m_fn_loop_maker;

		void _wait_loop();
	public:
		event_loop_group(event_loop_cfg const& cfg, fn_event_loop_maker_t const& fn_maker);
		~event_loop_group();

		void wait();
		void notify_terminating();

		void start(u32_t count);
		void stop();

		netp::size_t size();

		NRP<event_loop> next(std::set<NRP<event_loop>> const& exclude_this_set_if_have_more);
		NRP<event_loop> next();

		void execute(fn_task_t&& f);
		void schedule(fn_task_t&& f);
		void launch(NRP<netp::timer> const& t, NRP<netp::promise<int>> const& lf = nullptr);
	};
}
#endif
