#ifndef _NETP_THREAD_HPP_
#define _NETP_THREAD_HPP_

#include <netp/thread_impl/thread_basic.hpp>
#include <netp/core.hpp>
#include <netp/exception.hpp>

#include <netp/thread_impl/spin_mutex.hpp>
#include <netp/thread_impl/mutex.hpp>
#include <netp/thread_impl/condition.hpp>
#include <netp/tls.hpp>

#include <netp/app.hpp>

namespace netp {

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
				//if it has been moved from(either constructing another thread object, or assigning to it).
				//if either of its members join or detach has been called.
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