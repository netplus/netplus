#ifndef _NETP_THREAD_HPP_
#define _NETP_THREAD_HPP_

#if defined( _DEBUG) || defined(DEBUG)
	#ifndef _DEBUG_THREAD
		#define _DEBUG_THREAD
	#endif
#endif

#ifdef _DEBUG_THREAD
#endif

#include <thread>
#include <exception>

#include <netp/core.hpp>

#include <netp/exception.hpp>
#include <netp/logger_broker.hpp>

#include <netp/thread_impl/spin_mutex.hpp>
#include <netp/thread_impl/mutex.hpp>
#include <netp/thread_impl/condition.hpp>
#include <netp/tls.hpp>


//#define NETP_ENABLE_TRACE_THREAD
#ifdef NETP_ENABLE_TRACE_THREAD
	#define NETP_TRACE_THREAD NETP_INFO
#else
	#define NETP_TRACE_THREAD(...)
#endif

//#define NETP_ENABLE_DEBUG_STACK_SIZE

namespace netp {

	namespace impl {
		struct _th_run_base :
			netp::non_atomic_ref_base
		{
			virtual void _W_run() = 0;
		};

		template <typename _callable>
		struct _th_run final: 
			public _th_run_base
		{
			_callable _W_func;
			_th_run(_callable&& __f) :_W_func(std::forward<_callable>(__f))
			{}
			void _W_run() { _W_func(); }
		};

		template <typename _callable>
		inline netp::ref_ptr<_th_run<_callable>> _M_make_routine(_callable&& __f)
		{
			return netp::make_ref<_th_run<_callable>>(std::forward<_callable>(__f));
		}

		struct interrupt_exception : public netp::exception {
			interrupt_exception() :
				exception(netp::E_THREAD_INTERRUPT, "thread interrupted", __FILE__, __LINE__,__FUNCTION__ )
			{
			}
		};

		struct thread_data final {
			NETP_DECLARE_NONCOPYABLE(thread_data)
			
			public:
			spin_mutex mtx;
			mutex* current_cond_mutex;
			condition_variable* current_cond;

			spin_mutex* current_cond_any_mutex;
			condition_variable_any* current_cond_any;
			condition_variable_any sleep_cond_any;

			bool interrupted;
#ifdef NETP_ENABLE_DEBUG_STACK_SIZE
			void* th_begin_stack_address;
#endif
		public:
			thread_data() :
				mtx(),
				current_cond_mutex(nullptr),
				current_cond(nullptr),
				current_cond_any_mutex(nullptr),
				current_cond_any(nullptr),
				interrupted(false)
#ifdef NETP_ENABLE_DEBUG_STACK_SIZE
				,th_begin_stack_address(0)
#endif
			{}

			~thread_data() {}

			template <class _Rep, class _Period>
			inline void sleep_for(std::chrono::duration<_Rep, _Period> const& duration) {
				netp::unique_lock<spin_mutex> ulk(mtx);
				if (interrupted) {
					interrupted = false;
					NETP_TRACE_THREAD("interrupt_exception triggered in sleep action");
					throw interrupt_exception();
				}
				static const std::chrono::duration<_Rep, _Period> wait_thresh =
					std::chrono::duration_cast<std::chrono::duration<_Rep, _Period>>(std::chrono::nanoseconds(100000000ULL));

				if (duration <= wait_thresh) {
					std::this_thread::sleep_for(duration);
				} else {
					sleep_cond_any.wait_for(ulk, duration);
				}

				if (interrupted) {
					interrupted = false;
					NETP_TRACE_THREAD("interrupt_exception triggered in sleep action");
					throw interrupt_exception();
				}
			}

			void interrupt();
			inline void check_interrupt() {
				netp::lock_guard<spin_mutex> lg(mtx);
				if (interrupted) {
					interrupted = false;
					NETP_TRACE_THREAD("interrupt_exception triggered");
					throw interrupt_exception();
				}
			}

#ifdef NETP_ENABLE_DEBUG_STACK_SIZE
			void record_stack_address(void* const address) ;
#endif
		};
	}//end of impl

	namespace this_thread {
		inline void __interrupt_check_point() {
			netp::impl::thread_data*& th_data = netp::tls_get<netp::impl::thread_data>();
			NETP_ASSERT(th_data != nullptr);
			th_data->check_interrupt();
		}

		template <class _Duration>
		void sleep_for(_Duration const& dur) {
			netp::impl::thread_data*& th_data = netp::tls_get<netp::impl::thread_data>();
			NETP_ASSERT(th_data != nullptr);
			th_data->sleep_for(dur);
		}

		inline void sleep(netp::u64_t milliseconds) {
			netp::this_thread::sleep_for<std::chrono::milliseconds>(std::chrono::milliseconds(milliseconds));
		}
		inline void no_interrupt_sleep(netp::u64_t milliseconds) {
			std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
		}
		inline void usleep(netp::u64_t microseconds) {
			netp::this_thread::sleep_for<std::chrono::microseconds>(std::chrono::microseconds(microseconds));
		}
		inline void no_interrupt_usleep(netp::u64_t microseconds) {
			std::this_thread::sleep_for(std::chrono::microseconds(microseconds));
		}
		inline void nsleep(netp::u64_t nano) {
			netp::this_thread::sleep_for<std::chrono::nanoseconds>(std::chrono::nanoseconds(nano));
		}
		inline void no_interrupt_nsleep(netp::u64_t nano) {
			std::this_thread::sleep_for(std::chrono::nanoseconds(nano));
		}
		inline void yield() {
			netp::this_thread::__interrupt_check_point();
			std::this_thread::yield();
		}
		inline void no_interrupt_yield() {
			std::this_thread::yield();
		}
		inline void yield(u64_t k) {
			if (k < 4) {
			}
			else if (k < 28) {
				netp::this_thread::yield();
			}
			else {
				netp::this_thread::sleep_for<std::chrono::nanoseconds>(std::chrono::nanoseconds(k));
			}
		}
		inline void no_interrupt_yield(u64_t k) {
			if (k < 4) {
			}
			else if (k < 28) {
				std::this_thread::yield();
			}
			else {
				std::this_thread::sleep_for(std::chrono::nanoseconds(k));
			}
		}

		inline netp::u64_t get_id() {
#ifdef _NETP_WIN
			return static_cast<u64_t>(GetCurrentThreadId());
#else
			std::hash<std::thread::id> hasher;
			return static_cast<netp::u64_t>(hasher(std::this_thread::get_id()));
#endif
		}
	}//end of this_thread

#ifdef NETP_ENABLE_DEBUG_STACK_SIZE
	#define NETP_DEBUG_STACK_SIZE() \
	do { \
		netp::impl::thread_data* ____tls_thdata =  netp::tls_get<netp::impl::thread_data>(); \
		if(NETP_LIKELY(____tls_thdata != nullptr)) { \
			____tls_thdata->record_stack_address(&____tls_thdata) ; \
		} \
	} while(false); 
#else
	#define NETP_DEBUG_STACK_SIZE()
#endif

	class thread final :
		public netp::ref_base
	{
		//use atomic type to sure that the address tearing of load/store operation not happen, cuz we have to access these two variable in other threads
		std::atomic<std::thread*> m_th;
		std::atomic<impl::thread_data*> m_th_data;

		NRP<impl::_th_run_base> m_th_run;

		void __PRE_RUN_PROXY__();
		void __RUN_PROXY__();
		void __POST_RUN_PROXY__();
	public:
		thread();
		~thread();

#ifdef _NETP_NO_CXX11_TEMPLATE_VARIADIC_ARGS
#define _THREAD_CONS( \
	TEMPLATE_LIST, PADDING_LIST, LIST, COMMA, X1, X2, X3, X4) \
	template<class _Fn COMMA LIST(_CLASS_TYPE)> \
		int start(_Fn _Fx COMMA LIST(_TYPE_REFREF_ARG)) \
		{ \
			NETP_ASSERT(m_th_data.load(std::memory_order_relaxed) == nullptr); \
			NETP_ASSERT(m_th.load(std::memory_order_relaxed) == nullptr); \
			m_th_data.store(netp::allocator<impl::thread_data>::make(), std::memory_order_relaxed); \
			NETP_ASSERT(m_th_data.load(std::memory_order_relaxed) != nullptr); \
			try { \
				m_th_run = impl::_M_make_routine(std::bind(std::forward<_Fn>(_Fx) COMMA LIST(_FORWARD_ARG))); \
				m_th = new std::thread(&thread::__RUN_PROXY__, this); \
			} catch (...) { \
				netp::allocator<impl::thread_data>::trash(m_th_data.load(std::memory_order_relaxed));\
				m_th_data.store(nullptr, std::memory_order_relaxed);\
				::delete (m_th.load(std::memory_order_relaxed));\
				m_th.store(nullptr, std::memory_order_relaxed);\
				int _eno = netp_last_errno();\
				NETP_ERR("[thread]new std::thread(&thread::__RUN_PROXY__, _THH_) failed: %d", _eno); \
				return NETP_NEGATIVE(_eno); \
			} \
			return netp::OK; \
		}

_VARIADIC_EXPAND_0X(_THREAD_CONS, , , , )
#else
		template <typename _Fun_t, typename... _Args>
		int start(_Fun_t&& __fun, _Args&&... __args) {

			NETP_ASSERT(m_th_data.load(std::memory_order_relaxed) == nullptr);
			NETP_ASSERT(m_th.load(std::memory_order_relaxed) == nullptr);
			//th_data should be prior to m_th, in case we call interrupt immediately after calling start
			m_th_data.store(netp::allocator<impl::thread_data>::make(), std::memory_order_relaxed);
			NETP_ASSERT(m_th_data.load(std::memory_order_relaxed) != nullptr);

			try {
				m_th_run = impl::_M_make_routine(std::bind(std::forward<typename std::remove_reference<_Fun_t>::type>(__fun), std::forward<_Args>(__args)...));

				//force a release for th related data
				std::atomic_thread_fence(std::memory_order_release);

				//@NOTE: A thread object is joinable if it represents a thread of execution.
				//A thread object is not joinable in any of these cases :
				//if it was default - constructed.
				//	if it has been moved from(either constructing another thread object, or assigning to it).
				//	if either of its members join or detach has been called.
				m_th = ::new std::thread(&thread::__RUN_PROXY__, this);
			} catch (...) {
				netp::allocator<impl::thread_data>::trash(m_th_data.load(std::memory_order_relaxed));
				m_th_data.store(nullptr, std::memory_order_relaxed);

				::delete m_th.load(std::memory_order_relaxed);
				m_th.store(nullptr, std::memory_order_relaxed);

				int _eno = netp_last_errno();
				NETP_ERR("[thread]new std::thread(&thread::__RUN_PROXY__, _THH_) failed: %d", _eno);
				return (_eno);
			}
			return netp::OK;
		}
#endif

		void join() {

			//only one join call is allowed
			std::thread* th = m_th.load(std::memory_order_acquire);
			while (th) {
				if (m_th.compare_exchange_weak(th, nullptr, std::memory_order_acq_rel, std::memory_order_acquire)) {
					goto __join_begin;
				}
				th = m_th.load(std::memory_order_acquire);
			}

			//the failed thread shhoul be blocked until the other thread's join done
			NETP_ASSERT(m_th.load(std::memory_order_acquire) == nullptr);
			while (m_th_data.load(std::memory_order_acquire) != nullptr) {
				netp::this_thread::no_interrupt_usleep(8);
			}
			return;
		__join_begin:
			NETP_ASSERT( m_th.load(std::memory_order_acquire) == nullptr );

			NETP_ASSERT(th->joinable());
			th->join();
			::delete th;

			impl::thread_data* th_data = m_th_data.load( std::memory_order_relaxed );
			m_th_data.store(nullptr, std::memory_order_relaxed);
			netp::allocator<impl::thread_data>::trash(th_data);
		}

		void interrupt() {
			impl::thread_data* th_data = m_th_data.load(std::memory_order_relaxed);
			if (NETP_UNLIKELY(th_data != nullptr)) {
				th_data->interrupt();
			}
		}
	};

	//will run until you call stop
	class thread_run_object_abstract:
		public netp::ref_base
	{
		enum thread_run_object_state {
			TR_IDLE,
			TR_LAUNCHING,
			TR_RUNNING,
			TR_EXITING,
		};

	private:
		NRP<thread> m_th;
		std::atomic<u8_t> m_state;
		void __run__();

		int _start_thread();
		void _join_thread() {
			if (m_th != nullptr) {
				m_th->join();
				m_th = nullptr;
			}
		}
		void _interrupt_thread() {
			NETP_ASSERT(m_th != nullptr);
			m_th->interrupt();
		}

		void __on_start__() {
			NETP_ASSERT(m_state.load(std::memory_order_acquire) == TR_LAUNCHING);
			on_start();
			m_state.store(TR_RUNNING, std::memory_order_release); //if launch failed, we'll not reach here, and it will not in TR_RUNNING ..., so don't worry
		}
		void __on_stop__() {
			NETP_ASSERT(m_state.load(std::memory_order_acquire) == TR_EXITING);
			on_stop();
		}

		void operator()() {
			while (m_state.load(std::memory_order_acquire) == TR_RUNNING) {
				run();
			}
		}
		void __on_exit__() {}

	public:
		thread_run_object_abstract() :
			m_th(nullptr),
			m_state(TR_IDLE)
		{
		}

		virtual ~thread_run_object_abstract() { stop(); }

		inline bool is_launching() const {
			return m_state.load(std::memory_order_acquire) == TR_LAUNCHING;
		}
		inline bool is_running() const {
			return m_state.load(std::memory_order_acquire) == TR_RUNNING;
		}
		inline bool is_idle() const {
			return m_state.load(std::memory_order_acquire) == TR_IDLE;
		}

		//@NOTE, all thread_run_object must call stop explicitly if start return netp::OK
		virtual int start(bool block_start = true) {
			NETP_ASSERT(m_state.load(std::memory_order_acquire) == TR_IDLE);
			int start_rt = _start_thread();
			if (start_rt != netp::OK || block_start == false) {
				return start_rt;
			}
			for (int k = 0; is_launching(); ++k) {
				netp::this_thread::yield(k);
			}
			return netp::OK;
		}

		virtual void stop() {
			u8_t s = m_state.load(std::memory_order_acquire);
			while (s != TR_IDLE) {
				if (s == TR_LAUNCHING) {
					int k = 0;
					while (is_launching()) netp::this_thread::yield(++k);
				} else if (s == TR_RUNNING) {
					if (m_state.compare_exchange_weak(s, TR_EXITING, std::memory_order_acq_rel, std::memory_order_release)) {
						_interrupt_thread();
						goto __exit_thread_run;
					}
				}

				s = m_state.load(std::memory_order_acquire);
				return;
			}

			return;
__exit_thread_run:
			_join_thread();
			m_state.store(TR_IDLE, std::memory_order_release);
		}

		virtual void on_start() = 0;
		virtual void on_stop() = 0;
		virtual void run() = 0;
	};
}

#endif