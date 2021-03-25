
#include <netp/core.hpp>
#include <netp/smart_ptr.hpp>
#include <netp/logger_broker.hpp>


#include <netp/task/scheduler.hpp>

#define NETP_MONITOR_TASK_VECTOR_CAPACITY
//#define NETP_MONITOR_TASK_VECTOR_SIZE
#define NETP_TASK_VECTOR_INIT_SIZE 4096
//#define NETP_TASK_VECTOR_MONITOR_SIZE 128

namespace netp { namespace task {

	//std::atomic<u64_t> task_abstract::s_auto_increment_id(0);

	scheduler::scheduler( u8_t const& max_runner):
		m_mutex(),
		m_condition(),
		m_state( S_IDLE ),
		m_max_concurrency(max_runner),
		m_runner_pool(nullptr)
	{
	}

	scheduler::~scheduler() {
		stop();
		NETP_TRACE_TASK("[scheduler]~scheduler()");
	}

	int scheduler::start() {
		{
			unique_lock<scheduler_mutext_t> _ul(m_mutex);
			if (m_state == S_EXIT) {
				m_state = S_IDLE;
			}
		}
		__on_start();
		return netp::OK;
	}

	void scheduler::stop() {
		{
			NETP_TRACE_TASK("scheduler::stop begin");
			{
				lock_guard<scheduler_mutext_t> _ul(m_mutex);
				if (m_state == S_EXIT || m_state == S_IDLE) { return; }
			}
			NETP_TRACE_TASK("scheduler::stop __block_until_no_new_task()");

			//experiment
			__block_until_no_new_task();
			NETP_TRACE_TASK("scheduler::stop __block_until_no_new_task() exit");

			lock_guard<scheduler_mutext_t> _ul( m_mutex );
			if( m_state == S_EXIT || m_state == S_IDLE ) { return; }

			m_state = S_EXIT;
			m_condition.notify_one();
		}
		NETP_TRACE_TASK("scheduler::stop __block_until_no_new_task() on_stop");
		__on_stop();
		NETP_TRACE_TASK("scheduler::stop __block_until_no_new_task() on_stop exit");
	}

	void scheduler::__on_start() {
		unique_lock<scheduler_mutext_t> _ul( m_mutex );

		m_state = S_RUN;
		m_tasks_assigning = new priority_task_queue();
		m_tasks_runner_wait_count = 0;

		m_runner_pool = new runner_pool( m_max_concurrency ) ;
		NETP_ALLOC_CHECK( m_runner_pool, sizeof(runner_pool) ) ;
		m_runner_pool->init(this);
	}

	void scheduler::__on_stop() {
		NETP_ASSERT( m_state == S_EXIT );
		NETP_ASSERT( m_tasks_assigning->empty() );

		m_runner_pool->deinit();

		NETP_DELETE( m_tasks_assigning );
		NETP_DELETE( m_runner_pool );
	}

	void scheduler::__block_until_no_new_task() {

		u8_t self_flag = 0;
		u8_t runner_flag = 0;
		while (1) {
		begin:

			if (self_flag == 0) {
				while (!m_tasks_assigning->empty()) {
					netp::this_thread::no_interrupt_yield(50);
					runner_flag = 0;
				}
				self_flag = 1;
			}
			else if (self_flag == 1) {
				while (!m_tasks_assigning->empty()) {
					self_flag = 0;
					runner_flag = 0;
					netp::this_thread::no_interrupt_yield(50);
					goto begin;
				}
				self_flag = 2;
			}

			if (runner_flag == 0) {
				while (!m_runner_pool->test_waiting_step1()) {
					netp::this_thread::no_interrupt_yield(50);
					self_flag = 0;
					runner_flag = 0;
					goto begin;
				}
				runner_flag = 1;
			}
			else if (runner_flag == 1) {
				while (!m_runner_pool->test_waiting_step2()	) {
					netp::this_thread::no_interrupt_yield(50);
					self_flag = 0;
					runner_flag = 0;
					goto begin;
				}
				runner_flag = 2;
			}

			if (self_flag == 2 && runner_flag == 2) {
				break;
			}
		}
	}
}}//end of ns
