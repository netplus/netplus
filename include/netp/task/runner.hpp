#ifndef _NETP_TASK_TASKRUNNER_HPP_
#define _NETP_TASK_TASKRUNNER_HPP_

#include <queue>

#include <netp/core.hpp>
#include <netp/smart_ptr.hpp>
#include <netp/mutex.hpp>
#include <netp/condition.hpp>
#include <netp/task/task.hpp>

//#define NETP_TASK_RUNNER_USE_SPIN_MUTEX

//#define NETP_PULL_TASK
//#define NETP_PUSH_TASK

namespace netp { namespace task {
	 
	static const u8_t		P_HIGH = 0;
	static const u8_t		P_NORMAL = 1;
	static const u8_t		P_MAX = 2;

	typedef std::deque< NRP<task_abstract>,netp::allocator<NRP<task_abstract>> > task_queue;
	struct priority_task_queue {
		task_queue tasks[P_MAX];
		inline void push(NRP<task_abstract> const& t, u8_t const& p) {
			tasks[p].push_back(t);
		}
		inline void front_and_pop(NRP<task_abstract>& t) {
			for (u8_t i = 0; i < P_MAX; ++i) {
				if (tasks[i].size()) {
					t = tasks[i].front();
					tasks[i].pop_front();
					return ;
				}
			}
		}
		inline bool empty() const {
			return	tasks[P_NORMAL].empty() && tasks[P_HIGH].empty();
		}
	};
}}

namespace netp { namespace task {
	
	typedef std::vector< NRP<task_abstract>,netp::allocator<NRP<task_abstract>> > task_vector;
	typedef std::vector< NRP<sequencial_task>, netp::allocator<NRP<sequencial_task>> > sequencial_task_vector;

	enum task_runner_state {
		TR_S_IDLE,
		TR_S_WAITING,
		TR_S_ASSIGNED,
		TR_S_RUNNING,
		TR_S_ENDING
	};

#ifdef NETP_TASK_RUNNER_USE_SPIN_MUTEX
	typedef spin_mutex task_runner_mutex_t;
#else
	typedef mutex task_runner_mutex_t;
#endif

	class scheduler;
	class task;
	class runner final:
		public thread_run_object_abstract
	{
		NETP_DECLARE_NONCOPYABLE(runner)

		u8_t m_id;
		u8_t m_wait_flag;
		volatile task_runner_state m_state;

		scheduler* m_scheduler;
	public:
		runner( u8_t runner_id, scheduler* s );
		~runner();

		void stop();
		void on_start() ;
		void on_stop() ;
		void run();

		inline bool is_waiting() const { return m_state == TR_S_WAITING ;}
		inline bool is_running() const { return m_state == TR_S_RUNNING ;}
		inline bool is_idle() const { return m_state == TR_S_IDLE ;}
		inline bool is_ending() const { return m_state == TR_S_ENDING ;}

		inline bool test_waiting_step1() {
			if (m_state == TR_S_WAITING) {
				m_wait_flag = 1;
				return true;
			}
			return false;
		}

		inline bool test_waiting_step2() {
			return (m_wait_flag == 1) && (m_state == TR_S_WAITING);
		}
	};

	class sequencial_task;
	class sequencial_runner final:
		public thread_run_object_abstract
	{
		NETP_DECLARE_NONCOPYABLE(sequencial_runner)
		enum State {
			S_IDLE,
			S_RUN,
			S_WAITING,
			S_EXIT
		};

	public:
		sequencial_runner( u8_t const& id) ;
		~sequencial_runner() ;

		inline void assign_task( NRP<sequencial_task> const& task ) {
			lock_guard<task_runner_mutex_t> _lg(m_mutex);
			NETP_ASSERT(m_state == S_RUN || m_state == S_WAITING);
			m_standby->push_back(task);

			if(m_state == S_WAITING) {
				m_condition.notify_one();
			}
		}

		void stop();
		void on_start() ;
		void on_stop() ;
		void run();

		inline bool test_waiting_step1() {
			if (m_state == S_WAITING) {
				m_wait_flag = 1;
				return true;
			}
			return false;
		}

		inline bool test_waiting_step2() {
			return (m_wait_flag == 1) && (m_state == S_WAITING);
		}

	private:
		task_runner_mutex_t m_mutex;

#ifdef NETP_TASK_RUNNER_USE_SPIN_MUTEX
		condition_any m_condition;
#else
		condition m_condition;
#endif
		u8_t m_wait_flag;
		volatile State m_state;
		u32_t m_id;

		sequencial_task_vector* m_standby ;
		sequencial_task_vector* m_assigning;
	};


	typedef std::vector< NRP<runner> > TRV;

	class runner_pool {
	public:
		runner_pool(u8_t const& max_runner = 4);
		~runner_pool();

		void init( scheduler* s );
		void deinit();

	public:

		inline void set_max_task_runner(u8_t const& count);
		inline u8_t const& get_max_task_runner() const { return m_max_concurrency; }

		bool test_waiting_step1() {
			for (u32_t i = 0; i < m_runners.size(); ++i) {
				if (!m_runners[i]->test_waiting_step1()) {
					return false;
				}
			}
			return true;
		}

		bool test_waiting_step2() {
			for (u32_t i = 0; i < m_runners.size(); ++i) {
				if (!m_runners[i]->test_waiting_step2()) {
					return false;
				}
			}
			return true;
		}

	private:
		mutex m_mutex;
		TRV m_runners;
		bool m_is_running : 1;
		u8_t m_max_concurrency;
		u8_t m_last_runner_idx;
	};

	class sequencial_task;
	typedef std::vector< NRP<sequencial_runner> > sequence_task_runner_vector;

	class sequencial_runner_pool {
		NETP_DECLARE_NONCOPYABLE(sequencial_runner_pool)
	public:
		sequencial_runner_pool(u8_t const& concurrency = 2);
		~sequencial_runner_pool();

		inline void assign_task(NRP<sequencial_task> const& ta ) const {
			NETP_ASSERT(m_is_running == true);
			NETP_ASSERT(ta != nullptr);

			u32_t ridx = (ta->m_tag%m_concurrency);
			NETP_ASSERT(m_runners[ridx] != nullptr);
			m_runners[ridx]->assign_task(ta);
		}

		void init();
		void deinit();

		bool test_waiting_step1() {
			for (u32_t i = 0; i < m_runners.size(); ++i) {
				if (!m_runners[i]->test_waiting_step1()) {
					return false;
				}
			}
			return true;
		}

		bool test_waiting_step2() {
			for (u32_t i = 0; i < m_runners.size(); ++i) {
				if (!m_runners[i]->test_waiting_step2()) {
					return false;
				}
			}
			return true;
		}

	private:
		mutex m_mutex;
		bool m_is_running : 1;
		u8_t m_concurrency;

		sequence_task_runner_vector m_runners;
	};
}}
#endif