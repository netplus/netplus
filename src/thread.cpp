#include <netp/app.hpp>
#include <netp/thread.hpp>
#include <netp/tls.hpp>
#include <netp/condition.hpp>

namespace netp {

//#define _NETP_STACK_SIZE_CHECK
#ifdef _NETP_STACK_SIZE_CHECK
	__NETP_NO_INLINE
	void ___nest_stack_size_check(int i) {
		char __u[10240] = { 0 };
		std::memset(__u, 10240, i);
		___nest_stack_size_check(++i);
	}
#endif

	void thread::__PRE_RUN_PROXY__() {
		netp::random_init_seed();

		impl::thread_data* th_data = m_th_data.load(std::memory_order_relaxed);
		NETP_ASSERT(th_data != nullptr);
		tls_set<impl::thread_data>(th_data);

#ifdef NETP_MEMORY_USE_ALLOCATOR_POOL
		netp::app::instance()->global_allocator()->incre_thread_count();
		tls_create<netp::pool_aligned_allocator_t>();
#endif

#if defined(_DEBUG_MUTEX) || defined(_DEBUG_SHARED_MUTEX)
		mutex_set_t* mtxset = tls_create<mutex_set_t>();
		NETP_ASSERT(mtxset != nullptr);
		(void)mtxset;
#endif
		NETP_TRACE_THREAD("[thread]__PRE_RUN_PROXY__");
	}

	void thread::__RUN_PROXY__() {
		std::atomic_thread_fence(std::memory_order_acquire);
#ifdef _NETP_GNU_LINUX
		//check stack size
		size_t ssize = netp::get_stack_size();
		NETP_VERBOSE("[thread]__RUN_PROXY__, thread stack size: %d", ssize);
		NETP_ASSERT(ssize >= (1024 * 1024 * 4)); //4M
		(void)ssize;
#endif

#ifdef _NETP_STACK_SIZE_CHECK
		___nest_stack_size_check(0);
#endif

#ifdef NETP_ENABLE_DEBUG_STACK_SIZE
		int __run_proxy_begin = 0;
		NETP_ASSERT(m_th_data !=nullptr);
		m_th_data->record_stack_address(&__run_proxy_begin);
#endif

		__PRE_RUN_PROXY__();
		NETP_ASSERT(m_th_run != nullptr);
		try {
			m_th_run->_W_run();
		} catch (impl::interrupt_exception&) {
			NETP_WARN("[thread]__RUN_PROXY__ thread interrupt catch" );
		} catch (netp::exception& e) {
			NETP_ERR("[thread]__RUN_PROXY__, netp::exception: [%d]%s\n%s(%d) %s\ncallstack: \n%s", 
				e.code(), e.what(), e.file(), e.line(), e.function(), e.callstack());
			throw;
		} catch (std::exception& e) {
			NETP_ERR("[thread]__RUN_PROXY__, std::exception: %s", e.what());
			throw;
		} catch (...) {
			NETP_ERR("[thread]__RUN_PROXY__ thread unknown exception");
			__POST_RUN_PROXY__();
			throw;
		}
		__POST_RUN_PROXY__();
	}
	void thread::__POST_RUN_PROXY__() {
#if defined(_DEBUG_MUTEX) || defined(_DEBUG_SHARED_MUTEX)
		tls_destroy<mutex_set_t>();
#endif

		tls_set<impl::thread_data>(nullptr);
		NETP_TRACE_THREAD("[thread]__POST_RUN_PROXY__");
#ifdef NETP_MEMORY_USE_ALLOCATOR_POOL
		tls_destroy<netp::pool_aligned_allocator_t>();
		netp::app::instance()->global_allocator()->decre_thread_count();
#endif
	}

	thread::thread() :
		m_th(nullptr),
		m_th_data(nullptr),
		m_th_run(nullptr)
	{
	}

	thread::~thread() {
		join();
	}

	void thread::set_affinity(int i) {
		std::thread* th = m_th.load(std::memory_order_acquire);
		if (i <= 0) { i = 0; }
		if (th != nullptr) {
#if defined(_NETP_WIN)
			if (!SetThreadAffinityMask(th->native_handle(), (1ULL << (i)))) {
				NETP_WARN("[thread]SetThreadAffinityMask failed: %d", netp_last_errno());
			}
#elif defined(_NETP_GNU_LINUX)
			cpu_set_t cpuset;
			CPU_ZERO(&cpuset);
			CPU_SET(i, &cpuset);
			int rt = pthread_setaffinity_np(th->native_handle(),sizeof(cpu_set_t), &cpuset);
			if (rt != 0) {
				NETP_WARN("[thread]pthread_setaffinity_np failed: %d", rt);
			}
#endif
		}
	}

	//note: https://stackoverflow.com/questions/10876342/equivalent-of-setthreadpriority-on-linux-pthreads
	void thread::set_priority_above_normal() {
#if defined(_NETP_WIN)
		std::thread* th = m_th.load(std::memory_order_acquire);
		if (th == nullptr) { return; }

		if (!SetThreadPriority(th->native_handle(), THREAD_PRIORITY_ABOVE_NORMAL)) {
			NETP_WARN("[thread]SetThreadPriority(THREAD_PRIORITY_ABOVE_NORMAL) failed: %d", netp_last_errno() );
		}
#endif
	}
	
	void thread::set_priority_time_critical() {
#if defined(_NETP_WIN)
		std::thread* th = m_th.load(std::memory_order_acquire);
		if (th == nullptr) { return; }
		if (!SetThreadPriority(th->native_handle(), THREAD_PRIORITY_TIME_CRITICAL)) {
			NETP_WARN("[thread]SetThreadPriority(THREAD_PRIORITY_TIME_CRITICAL) failed: %d", netp_last_errno());
		}
#endif
	}

	namespace impl {
		void thread_data::interrupt() {
			lock_guard<spin_mutex> lg(mtx);
			interrupted = true;
			{
				if (current_cond != nullptr)
				{
					lock_guard<mutex> lg_current_cond_mtx(*current_cond_mutex);
					current_cond->notify_all();
				}
				if (current_cond_any != nullptr)
				{
					lock_guard<spin_mutex> lg_current_cond_mtx(*current_cond_any_mutex);
					current_cond_any->notify_all();
				}
			}
		}

		#ifdef NETP_ENABLE_DEBUG_STACK_SIZE
		void thread_data::record_stack_address( void* const address) {
			if (th_begin_stack_address == 0) {
				th_begin_stack_address = address;
				return ;
			}
			i64_t current_size = NETP_ABS((i64_t)address - (i64_t)th_begin_stack_address);
			if(current_size >= 100*1024) {
				//stack_trace
				NETP_WARN("[stack_check]start: %p, current : %p, size: %llu bytes", th_begin_stack_address, address , current_size);
			}
		}
		#endif
	}


	void thread_run_object_abstract::__run__() {
		//sync m_th
		std::atomic_thread_fence(std::memory_order_acquire);
		try {
			NETP_TRACE_THREAD("[thread_run_object_abstract::__run__]__on_start__() begin");
			__on_start__();
			NETP_TRACE_THREAD("[thread_run_object_abstract::__run__]__on_start__() end");
		}
		catch (netp::exception & e) {
			NETP_ERR("[thread_run_object_abstract::__run__]__on_start__() netp::exception: [%d]%s\n%s(%d) %s\n%s",
				e.code(), e.what(), e.file(), e.line(), e.function(), e.callstack());
			throw;
		}
		catch (std::exception & e) {
			NETP_ERR("[thread_run_object_abstract::__run__]__on_start__() std::exception: %s", e.what());
			throw;
		}
		catch (...) {
			NETP_ERR("[thread_run_object_abstract::__run__]__on_start__() unknown thread exception");
			throw;
		}

		try {
			NETP_TRACE_THREAD("[thread_run_object_abstract::__run__]thread operator() begin");
			operator()();
			NETP_TRACE_THREAD("[thread_run_object_abstract::__run__]thread operator() end");
		}

		catch (netp::impl::interrupt_exception&) {
			NETP_TRACE_THREAD("[thread_run_object_abstract::__run__] interrupt catch");
		}
		catch (netp::exception & e) {
			NETP_ERR("[thread_run_object_abstract::__run__]operator() netp::exception: [%d]%s\n%s(%d) %s\n%s",
				e.code(), e.what(), e.file(), e.line(), e.function(), e.callstack());
			throw;
		}
		catch (std::exception & e) {
			NETP_ERR("[thread_run_object_abstract::__run__]operator() std::exception: %s", e.what());
			throw;
		}
		catch (...) {
			NETP_ERR("[thread_run_object_abstract::__run__]operator() unknown thread exception");
			throw;
		}

		//clear skiped interrupt execetion if it has [edge event]
		try { this_thread::__interrupt_check_point(); }
		catch (netp::impl::interrupt_exception&) {}

		try {
			NETP_TRACE_THREAD("[thread_run_object_abstract::__run__]__on_stop__() begin");
			__on_stop__();
			NETP_TRACE_THREAD("[thread_run_object_abstract::__run__]__on_stop__() end");
		}
		catch (netp::exception & e) {
			NETP_ERR("[thread_run_object_abstract::__run__]__on_stop__()  netp::exception: [%d]%s\n%s(%d) %s\n%s",
				e.code(), e.what(), e.file(), e.line(), e.function(), e.callstack());
			throw;
		}
		catch (std::exception & e) {
			NETP_ERR("[thread_run_object_abstract::__run__]__on_stop__() std::exception : %s", e.what());
			throw;
		}
		catch (...) {
			NETP_ERR("[thread_run_object_abstract::__run__]__on_stop__() unknown thread exception");
			throw;
		}

		try {
			NETP_TRACE_THREAD("[thread_run_object_abstract::__run__]__on_exit__() begin");
			__on_exit__();
			NETP_TRACE_THREAD("[thread_run_object_abstract::__run__]__on_exit__() end");
		}
		catch (netp::exception & e) {
			NETP_ERR("[thread_run_object_abstract::__run__]__on_exit__ netp::exception: [%d]%s\n%s(%d) %s\n%s",
				e.code(), e.what(), e.file(), e.line(), e.function(), e.callstack());
			throw;
		}
		catch (std::exception & e) {
			NETP_ERR("[thread_run_object_abstract::__run__]__on_exit__ exception : %s", e.what());
			throw;
		}
		catch (...) {
			NETP_ERR("[thread_run_object_abstract::__run__]__on_exit__ unknown thread exception");
			throw;
		}
	}

	int thread_run_object_abstract::_start_thread() {
		NETP_ASSERT(m_th == nullptr);
		try {
			m_th = netp::make_ref<thread>();
		} catch (...) {
			int _eno = netp_last_errno();
			NETP_ERR("[thread_run_object_abstract::start_thread] failed: %d", _eno);
			return _eno;
		}
		m_state.store(TR_LAUNCHING, std::memory_order_release);
		return m_th->start(&thread_run_object_abstract::__run__, this);
	}
}