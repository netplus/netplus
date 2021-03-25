#include <netp/thread.hpp>
#include <netp/tls.hpp>
#include <netp/logger_broker.hpp>
#include <netp/condition.hpp>

namespace netp {

	thread::thread() :
		m_handler(nullptr),
		m_th_data(nullptr),
		m_run_base_type(nullptr)
	{
	}

	thread::~thread() {
		join();
	}

	void thread::__PRE_RUN_PROXY__() {
		NETP_ASSERT(m_th_data != nullptr);
		tls_set<impl::thread_data>(m_th_data);

#ifdef NETP_MEMORY_USE_TLS_POOL
		tls_create<netp::pool_align_allocator_t>();
#endif

#if defined(_DEBUG_MUTEX) || defined(_DEBUG_SHARED_MUTEX)
		mutex_set_t* mtxset = tls_create<mutex_set_t>();
		NETP_ASSERT(mtxset != nullptr);
		(void)mtxset;
#endif
		NETP_TRACE_THREAD("[thread]__PRE_RUN_PROXY__");
	}

	void thread::__RUN_PROXY__() {
#ifdef NETP_ENABLE_DEBUG_STACK_SIZE
		int __run_proxy_begin = 0;
		NETP_ASSERT(m_th_data !=nullptr);
		m_th_data->record_stack_address(&__run_proxy_begin);
#endif
		netp::random_init_seed();

		__PRE_RUN_PROXY__();
		NETP_ASSERT(m_run_base_type.get() != nullptr);
		try {
			m_run_base_type->_W_run();
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
#ifdef NETP_MEMORY_USE_TLS_POOL
		tls_destroy<netp::pool_align_allocator_t>();
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
		NETP_ASSERT(m_th != nullptr);
		try {
			NETP_TRACE_THREAD("[thread_run_object_abstract::__run__]__on_launching__() begin");
			__on_launching__();
			NETP_TRACE_THREAD("[thread_run_object_abstract::__run__]__on_launching__() end");
		}
		catch (netp::exception & e) {
			NETP_ERR("[thread_run_object_abstract::__run__]__on_launching__() netp::exception:[%d]%s\n%s(%d) %s\n%s",
				e.code(), e.what(), e.file(), e.line(), e.function(), e.callstack());
			throw;
		}
		catch (std::exception & e) {
			NETP_ERR("[thread_run_object_abstract::__run__]__on_launching__() std::exception: %s", e.what());
			throw;
		}
		catch (...) {
			NETP_ERR("[thread_run_object_abstract::__run__]__on_launching__() unknown thread exception");
			throw;
		}

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

	int thread_run_object_abstract::start_thread() {
		NETP_ASSERT(m_th == nullptr);
		try {
			m_th = netp::make_ref<thread>();
		} catch (...) {
			int _eno = netp_last_errno();
			NETP_ERR("[thread_run_object_abstract::start_thread] failed: %d", _eno);
			return _eno;
		}
		m_state = TR_START;
		return m_th->start(&thread_run_object_abstract::__run__, this);
	}
}