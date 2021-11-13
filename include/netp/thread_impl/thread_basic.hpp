#ifndef _NETP_THREAD_IMPL_THREAD_BASIC_HPP
#define _NETP_THREAD_IMPL_THREAD_BASIC_HPP

#include <thread>
#include <exception>

#include <netp/core.hpp>
#include <netp/smart_ptr.hpp>
#include <netp/thread_impl/mutex.hpp>
#include <netp/thread_impl/condition.hpp>

//#define NETP_ENABLE_TRACE_THREAD
#ifdef NETP_ENABLE_TRACE_THREAD
#define NETP_TRACE_THREAD NETP_INFO
#else
#define NETP_TRACE_THREAD(...)
#endif

//#define NETP_ENABLE_DEBUG_STACK_SIZE

//#define _NETP_STACK_SIZE_CHECK
namespace netp { namespace impl {

		struct _th_run_base :
			netp::non_atomic_ref_base
		{
			virtual void _W_run() = 0;
		};

		template <typename _callable>
		struct _th_run final :
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
				exception(netp::E_THREAD_INTERRUPT, "thread interrupted", __FILE__, __LINE__, __FUNCTION__)
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
				, th_begin_stack_address(0)
#endif
			{}

			~thread_data() {}

			template <class _Rep, class _Period>
			inline void sleep_for(std::chrono::duration<_Rep, _Period> const& duration) {
				netp::unique_lock<spin_mutex> ulk(mtx);
				if (interrupted) {
					interrupted = false;
					throw interrupt_exception();
				}
				static const std::chrono::duration<_Rep, _Period> wait_thresh =
					std::chrono::duration_cast<std::chrono::duration<_Rep, _Period>>(std::chrono::nanoseconds(100000000ULL));

				if (duration <= wait_thresh) {
					std::this_thread::sleep_for(duration);
				}
				else {
					sleep_cond_any.wait_for(ulk, duration);
				}

				if (interrupted) {
					interrupted = false;
					throw interrupt_exception();
				}
			}

			void interrupt();
			inline void check_interrupt() {
				netp::lock_guard<spin_mutex> lg(mtx);
				if (interrupted) {
					interrupted = false;
					throw interrupt_exception();
				}
			}

#ifdef NETP_ENABLE_DEBUG_STACK_SIZE
			void record_stack_address(void* const address);
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
		//https://stackoverflow.com/questions/18071664/stdthis-threadsleep-for-and-nanoseconds
		inline void yield(u64_t k) {
			if (k<100) {
				netp::this_thread::yield();
			} else {
				netp::this_thread::sleep_for<std::chrono::nanoseconds>(std::chrono::nanoseconds(k));
			}
		}
		inline void no_interrupt_yield(u64_t k) {
			if (k<128) {
				std::this_thread::yield();
			} else {
				std::this_thread::sleep_for(std::chrono::nanoseconds(k));
			}
		}

		inline netp::u64_t get_id() {
#ifdef _NETP_WIN
			return static_cast<u64_t>(GetCurrentThreadId());
#else
			return netp::u64_t(pthread_self());
			//std::hash<std::thread::id> hasher;
			//return static_cast<netp::u64_t>(hasher(std::this_thread::get_id()));
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
}

#endif 