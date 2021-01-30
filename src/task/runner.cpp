#include <netp/logger_broker.hpp>
#include <netp/task/scheduler.hpp>
#include <netp/task/runner.hpp>

namespace netp { namespace task {

	runner::runner( u8_t id, scheduler* s ) :
		m_id( id ),
		m_wait_flag(0),
		m_state(TR_S_IDLE),
		m_scheduler(s)
	{
		NETP_TRACE_TASK( "[TRunner][-%u-]construct new runner", m_id );
	}

	runner::~runner() {
		stop() ;
		NETP_TRACE_TASK( "[TRunner][-%u-]destructTaskRunner", m_id );
	}

	void runner::on_start() {
		m_state = TR_S_IDLE ;
	}

	void runner::on_stop() {
		NETP_TRACE_TASK("[TRunner][-%u-]runner exit...", m_id ) ;
	}

	void runner::stop() {
		{
			while (m_state != TR_S_WAITING) {
				netp::this_thread::no_interrupt_yield(1);
			}

			m_state = TR_S_ENDING;
			NETP_TRACE_TASK("[TRunner][-%u-]runner enter into state [%d]", m_id, m_state ) ;
		}

		{
			lock_guard<scheduler_mutext_t> lg_schd(m_scheduler->m_mutex);
			m_scheduler->m_condition.no_interrupt_notify_all();
		}
		thread_run_object_abstract::stop();
	}

	void runner::run() {

		NETP_ASSERT(m_scheduler != nullptr);
		while (1)
		{
			NRP<netp::task::task_abstract> task;
			{
			unique_lock<scheduler_mutext_t> _ulk(m_scheduler->m_mutex);
			check_begin:
				{
					if (m_state == TR_S_ENDING) {
						break;
					}
				}

				m_scheduler->m_tasks_assigning->front_and_pop(task);
				if (task == nullptr) {
					m_state = TR_S_WAITING;
					++(m_scheduler->m_tasks_runner_wait_count);
					m_scheduler->m_condition.no_interrupt_wait(_ulk);
					--(m_scheduler->m_tasks_runner_wait_count);
					goto check_begin;
				}
			}
			NETP_ASSERT(task != nullptr);

			m_state = TR_S_RUNNING;
			m_wait_flag = 0;
			try {
				NETP_TRACE_TASK("[TRunner][-%d-]task run begin, TID: %llu", m_id, reinterpret_cast<u64_t>(task.get()));
				task->run();
				NETP_TRACE_TASK("[TRunner][-%d-]task run end, TID: %llu", m_id, reinterpret_cast<u64_t>(task.get()));
			}
			catch (netp::exception& e) {
				NETP_ERR("[TRunner][-%d-]runner netp::exception: [%d]%s\n%s(%d) %s\n%s",
					m_id, e.code(), e.what(), e.file(), e.line(), e.function(), e.callstack());
				throw;
			}
			catch (std::exception& e) {
				NETP_ERR("[TRunner][-%d-]runner exception: %s", m_id, e.what());
				throw;
			}
			catch (...) {
				NETP_ERR("[TRunner][-%d-]runner, unknown exception", m_id);
				throw;
			}
		}
	}


#define NETP_SQT_VECTOR_SIZE 4000

	sequencial_runner::sequencial_runner(u8_t const& id):
		m_mutex(),
		m_condition(),
		m_wait_flag(0),
		m_state(S_IDLE),
		m_id(id),
		m_standby(nullptr),
		m_assigning(nullptr)
	{
	}

	sequencial_runner::~sequencial_runner() {
		stop();
	}

	void sequencial_runner::on_start() {
			lock_guard<task_runner_mutex_t> lg(m_mutex);

			m_state = S_RUN;

			m_standby = new sequencial_task_vector() ;
			NETP_ALLOC_CHECK( m_standby, sizeof(sequencial_task_vector) );

			m_assigning = new sequencial_task_vector();
			NETP_ALLOC_CHECK( m_assigning, sizeof(sequencial_task_vector) ) ;

			m_standby->reserve( NETP_SQT_VECTOR_SIZE );
			m_assigning->reserve( NETP_SQT_VECTOR_SIZE );
	}

	void sequencial_runner::stop() {
		{
			while (m_standby->size() || m_assigning->size() ) {
				netp::this_thread::yield(1);
			}

			lock_guard<task_runner_mutex_t> lg(m_mutex);

			if( m_state == S_EXIT ) return ;
			m_state = S_EXIT ;
			m_condition.no_interrupt_notify_one();
		}

		thread_run_object_abstract::stop();
	}

	void sequencial_runner::on_stop() {
		NETP_ASSERT( m_state == S_EXIT );
		NETP_ASSERT( m_standby->empty() );
		NETP_ASSERT( m_assigning->empty() );
		NETP_DELETE( m_standby );
		NETP_DELETE( m_assigning );
	}

	void sequencial_runner::run() {
		{
			NETP_ASSERT(m_assigning->empty());
			unique_lock<task_runner_mutex_t> _ulk( m_mutex );

			if( m_state == S_EXIT ) {
				return ;
			}

			while( m_standby->empty() ) {
				m_state = S_WAITING;
				m_condition.no_interrupt_wait( _ulk );

				if( m_state == S_EXIT ) {
					return ;
				}
				NETP_ASSERT( m_state == S_WAITING);
			}

			NETP_ASSERT( m_assigning->empty() );
			NETP_ASSERT( !m_standby->empty());
			std::swap( m_standby, m_assigning );
			NETP_ASSERT( !m_assigning->empty() );
			NETP_ASSERT( m_standby->empty() );
			m_state = S_RUN;
			m_wait_flag = 0;
		}

		::size_t i = 0;
		::size_t size = m_assigning->size();
		while( m_state == S_RUN && i != size ) {
			NRP<sequencial_task>& task = (*m_assigning)[i++] ;
			NETP_ASSERT( task != nullptr );
			try {
				task->run();
			} catch ( netp::exception& e ) {
				NETP_ERR("[SQTRunner][-%d-]netp::exception: [%d]%s\n%s(%d) %s\n%s", 
					e.code(), e.what(), e.file(), e.line(), e.function(), e.callstack());
				throw;
			} catch( std::exception& e) {
				NETP_ERR("[SQTRunner][-%d-]std::exception: %s", m_id, e.what());
				throw;
			} catch( ... ) {
				NETP_ERR("[SQTRunner][-%d-]unknown exception: %s", m_id );
				throw;
			}
		}
		NETP_ASSERT( i == size );

		m_assigning->clear();
		if( (m_state == S_RUN) && (size>NETP_SQT_VECTOR_SIZE) ) {
			m_assigning->reserve(NETP_SQT_VECTOR_SIZE);
		}
	}


	runner_pool::runner_pool(u8_t const& max_runner) :
		m_mutex(),
		m_runners(),
		m_is_running(false),
		m_max_concurrency(max_runner),
		m_last_runner_idx(0)

	{
	}

	runner_pool::~runner_pool() {}

	void runner_pool::init( scheduler* s ) {
		netp::lock_guard<mutex> _lg(m_mutex);
		m_is_running = true;
		m_last_runner_idx = 0;

		u8_t i = 0;
		while (m_runners.size() < m_max_concurrency) {
			NRP<runner> tr = netp::make_ref<runner>(i,s);
			NETP_ALLOC_CHECK(tr, sizeof(runner));
			int rt = tr->start();
			if (rt != netp::OK) {
				tr->stop();
				NETP_THROW("create thread failed");
			}
			++i;
			m_runners.push_back(tr);
		}
	}

	void runner_pool::deinit() {
		netp::lock_guard<mutex> _lg(m_mutex);
		if (m_is_running == false) { return; }

		m_is_running = false;
		while (!m_runners.empty()) {
			m_runners.pop_back();
		}
	}

	void runner_pool::set_max_task_runner(u8_t const& count) {
		unique_lock<mutex> _lg(m_mutex);
		m_max_concurrency = count;
		//dynamic clear ,,if ...
		while (m_runners.size() > m_max_concurrency) {
			m_runners.pop_back();
		}
	}

	//for sequence task
	sequencial_runner_pool::sequencial_runner_pool(u8_t const& concurrency) :
		m_mutex(),
		m_is_running(false),
		m_concurrency(concurrency),
		m_runners()
	{
	}

	sequencial_runner_pool::~sequencial_runner_pool() {
		deinit();
	}

	void sequencial_runner_pool::init() {
		m_is_running = true;
		NETP_ASSERT(m_runners.size() == 0);
		u8_t i = 0;
		while (i++ < m_concurrency) {
			NRP<sequencial_runner> r = netp::make_ref<sequencial_runner>(i);
			NETP_ALLOC_CHECK(r, sizeof(sequencial_runner));
			int launch_rt = r->start();
			NETP_CONDITION_CHECK(launch_rt == netp::OK);
			m_runners.push_back(r);
		}
		NETP_ASSERT(m_runners.size() == m_concurrency);
	}

	void sequencial_runner_pool::deinit() {
		m_is_running = false;
		while (m_runners.size()) {
			m_runners.pop_back();
		}
		NETP_ASSERT(m_runners.size() == 0);
	}

}}//END OF NS
