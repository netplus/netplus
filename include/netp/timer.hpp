#ifndef _NETP_TIMER_HPP_
#define _NETP_TIMER_HPP_

#include <functional>
#include <chrono>
#include <deque>

#include <netp/core.hpp>
#include <netp/smart_ptr.hpp>
#include <netp/singleton.hpp>
#include <netp/heap.hpp>

#include <netp/mutex.hpp>
#include <netp/thread.hpp>

#include <netp/logger_broker.hpp>

#ifdef NETP_ENABLE_TRACE_TIMER
	#define NETP_TRACE_TIMER NETP_INFO
#else
	#define NETP_TRACE_TIMER(...) 
#endif

namespace netp {

	/*
	 * @note
	 * it is a pure impl for stopwatch
	 * do not make your business depend on timer's life cycle management
	 *
	 * expire == zero() mean stoped

	 * @design consideration
	 * self managed thread update
	 * non self managed thread update

	 * @impl consideration
	 * 1,start new timer on timer thread is available [circle lock check and etc]
	 * 2,timer thread waken up notification on the following situation
	 *		a), new timer wait exceed current wait time
	 *		b), new timer wait less than current wait time
	 * 3,current timer thread should be terminated if no new timer started
	 * 4,must launch new thread when current thread is not running at the moment when new timer have been planed
	 * 5,timer thread could be interrupted at any given time [ie, interrupt wait state when process exit]
	 *
	 */

	typedef std::chrono::nanoseconds timer_duration_t;
	typedef std::chrono::steady_clock timer_clock_t;
	typedef std::chrono::time_point<timer_clock_t, timer_duration_t> timer_timepoint_t;

	const long long TIMER_TIME_INFINITE(~0);
	const timer_duration_t _TIMER_DURATION_INFINITE = timer_duration_t(TIMER_TIME_INFINITE);
	const timer_timepoint_t _TIMER_TP_INFINITE = timer_timepoint_t() + _TIMER_DURATION_INFINITE;

	class timer final:
		public netp::ref_base
	{
		typedef std::function<void(NRP<timer> const&)> _fn_timer_t;
		_fn_timer_t callee;
		timer_duration_t delay;
		timer_timepoint_t expiration;
		timer_timepoint_t invocation;
		NRP<netp::ref_base> m_ctx;
		u32_t invoke_cnt;

		friend bool operator < (NRP<timer> const& l, NRP<timer> const& r);
		friend bool operator > (NRP<timer> const& l, NRP<timer> const& r);
		friend bool operator == (NRP<timer> const& l, NRP<timer> const& r);

		friend struct timer_less;
		friend struct timer_greater;
		friend class timer_broker;
		friend class timer_broker_ts;
	public:
		template <class dur, class _Fx, class... _Args>
		inline timer(dur&& delay_, _Fx&& func, _Args&&... args):
			callee(std::forward<_fn_timer_t>(std::bind(std::forward<_Fx>(func), std::forward<_Args>(args)...))),
			delay(delay_),
			expiration(timer_timepoint_t()),
			invocation(timer_timepoint_t()),
			invoke_cnt(0)
		{
		}

		template <class dur, class _callable
			, class=typename std::enable_if<std::is_convertible<_callable, _fn_timer_t>::value>::type>
		inline timer(dur&& delay_, _callable&& callee_ ):
			callee(std::forward<std::remove_reference<_fn_timer_t>::type>(callee_)),
			delay(delay_),
			expiration(timer_timepoint_t()),
			invocation(timer_timepoint_t()),
			invoke_cnt(0)
		{
			static_assert(std::is_class<std::remove_reference<_callable>>::value, "_callable must be lambda or std::function type");
		}
		//return expire - now
		inline timer_duration_t invoke(bool force_expire = false) {
			NETP_ASSERT(expiration != timer_timepoint_t() && expiration != _TIMER_TP_INFINITE);
			const timer_timepoint_t now = timer_clock_t::now();
			const timer_duration_t left = expiration - now;
			if (left.count() <= 0LL || force_expire) {
				invocation = now;
				callee(NRP<timer>(this));
				++invoke_cnt;
			}
			return left;
		}

		__NETP_FORCE_INLINE bool is_expired_invocation() {
			return invocation >= expiration;
		}
		__NETP_FORCE_INLINE u32_t invoke_count() { return invoke_cnt; }

		template <class dur>
        inline void set_delay(dur&& delay_) {
            delay = delay_;
        }

		inline timer_duration_t get_delay() const { return delay; }

		template <class ctx_t>
		inline NRP<ctx_t> get_ctx() {
			return netp::static_pointer_cast<ctx_t>(m_ctx);
		}

		inline void set_ctx(NRP<netp::ref_base> const& ctx) {
			m_ctx = ctx;
		}
	};

	inline bool operator < (NRP<timer> const& l, NRP<timer> const& r) {
		return l->expiration < r->expiration;
	}

	inline bool operator > (NRP<timer> const& l, NRP<timer> const& r) {
		return l->expiration > r->expiration;
	}

	inline bool operator == (NRP<timer> const& l, NRP<timer> const& r) {
		return l->expiration == r->expiration;
	}

	struct timer_less
	{
		inline bool operator()(NRP<timer> const& l, NRP<timer> const& r)
		{
			return l->expiration < r->expiration;
		}
	};

	struct timer_greater
	{
		inline bool operator()(NRP<timer> const& l, NRP<timer> const& r)
		{
			return l->expiration > r->expiration;
		}
	};

	#define NETP_TM_INIT_CAPACITY (1000)
	typedef std::deque<NRP<timer>> _timer_queue;
	typedef netp::binary_heap< NRP<timer>, netp::timer_less, NETP_TM_INIT_CAPACITY > _timer_heap_t;
	
	class timer_broker final:
		public netp::ref_base
	{
		_timer_heap_t m_heap;
		_timer_queue m_tq;

	public:
		timer_broker()
		{
		}

		virtual ~timer_broker()
		{
			NETP_ASSERT(m_tq.size() + m_heap.size() == 0);
			//NETP_INFO("[timer_broker]cancel timer: %d", m_tq.size() + m_heap.size() );
		}

		inline void launch(NRP<timer> const& t) {
			NETP_ASSERT(t != nullptr);
			NETP_ASSERT(t->delay >= timer_duration_t(0) && (t->delay != timer_duration_t(~0)));
			t->expiration = timer_clock_t::now() + t->delay;
			m_tq.push_back(t);
		}

		inline void launch(NRP<timer>&& t) {
			NETP_ASSERT(t != nullptr);
			NETP_ASSERT(t->delay >= timer_duration_t(0) && (t->delay != timer_duration_t(~0)));
			t->expiration = timer_clock_t::now() + t->delay;
			m_tq.push_back(std::move(t));
		}

		void expire_all();
		void expire(timer_duration_t& ndelay);
		const netp::size_t size() { return m_tq.size() + m_heap.size(); }
	};

	/*
	class timer_broker_ts final
	{
		netp::mutex m_mutex;
		netp::condition_variable m_cond;
		NRP<netp::thread> m_th;
		_timer_heap_t m_heap;
		_timer_queue m_tq;

		timer_duration_t m_ndelay;
		bool m_th_break;
		bool m_in_wait;
		bool m_in_exit;

		inline void __check_th_and_wait() {
			//interrupt wait state
			if (m_in_wait) {
				NETP_ASSERT(m_th_break == false);
				m_cond.notify_one();
				return;
			}

			if (m_th_break == true) {
				NETP_ASSERT(m_in_wait == false);
				m_th = netp::make_ref<netp::thread>();
				NETP_ALLOC_CHECK(m_th, sizeof(netp::thread));
				int rt = m_th->start(&timer_broker_ts::_run, this);
				NETP_CONDITION_CHECK(rt == netp::OK);
				m_th_break = false;
			}
		}

	public:
		timer_broker_ts():
			m_th_break(true),
			m_in_wait(false),
			m_in_exit(false)
		{
		}

		~timer_broker_ts();

		inline void launch(NRP<timer> const& t) {
			lock_guard<mutex> lg(m_mutex);
			if (m_in_exit == true) {
				return;
				//_TIMER_TP_INFINITE;
			}

			NETP_ASSERT(t != nullptr);
			NETP_ASSERT(t->delay >= timer_duration_t(0) && (t->delay != timer_duration_t(~0)));
			t->expiration = timer_clock_t::now() + t->delay;
			m_tq.push_back(t);
			__check_th_and_wait();
		}

		inline void launch(NRP<timer>&& t) {
			lock_guard<mutex> lg(m_mutex);
			if (m_in_exit == true) {
				return;
					//; _TIMER_TP_INFINITE;
			}
			NETP_ASSERT(t != nullptr);
			NETP_ASSERT(t->delay >= timer_duration_t(0) && (t->delay != timer_duration_t(~0)) );
			t->expiration = timer_clock_t::now() + t->delay;
			m_tq.push_back(std::forward<NRP<timer>>(t));
			__check_th_and_wait();
		}

		void _run();

		 //
		 // @note
		 // 1, if m_tq is not empty, check one by one ,
		 //		a) tm update expire and state, push heap, pop from tq
		 // 2, check the first heap element and see whether it is expired
		 //		a) if expire == timepoint::zero(), ignore and pop, continue
		 //		b) if expire time reach, call callee, check REPEAT and push to tq if necessary, pop
		 // @param nexpire, next expire timepoint
		 //		a) updated on every frame when heap get updated (on both pop and push operation)
		 //
		void update(timer_duration_t& ndelay);
	};

	class timer_manager:
		public singleton<timer_broker_ts>
	{};
	*/
}
#endif