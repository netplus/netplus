#ifndef _NETP_TASK_TASK_HPP_
#define _NETP_TASK_TASK_HPP_

#include <functional>
#include <vector>
#include <queue>

#include <netp/core.hpp>
#include <netp/smart_ptr.hpp>

namespace netp { namespace task {

	// interface for task
	struct task_abstract:
		public netp::ref_base
	{
		task_abstract() {}
		virtual ~task_abstract() {}
		virtual void run() = 0;
	};

	typedef std::function<void(NRP<ref_base> const& cookie)> fn_task_cookie_1;
	class task_with_cookie :
		public task_abstract {
	private:
		fn_task_cookie_1 m_run_func;
		NRP<ref_base> m_cookie ;

	public:
		explicit task_with_cookie(fn_task_cookie_1 const& run, NRP<ref_base> const& cookie ):
			task_abstract(),
			m_run_func(run),
			m_cookie(cookie)
		{
		}

		~task_with_cookie() {}

		//for run
		void run() {
			NETP_ASSERT(m_run_func != nullptr );
			m_run_func( m_cookie ) ;
		}
	};

	typedef std::function<void()> fn_task_void ;

	class task:
		public task_abstract {
		NETP_DECLARE_NONCOPYABLE(task)
			fn_task_void m_run_fn ;
	public:
		explicit task(fn_task_void const& run) :
			task_abstract(),
			m_run_fn(run)
		{
		}

		void run() {
			NETP_ASSERT(m_run_fn != nullptr );
			m_run_fn();
		}
	};


//#define DEBUG_SEQ

#ifdef DEBUG_SEQ
	static std::atomic<u64_t> s_seq_aid(1);
#endif

	class sequencial_task:
		public task_abstract
	{
		NETP_DECLARE_NONCOPYABLE(sequencial_task)
		NRP<task_abstract> m_task;
	public:
#ifdef DEBUG_SEQ
		u64_t m_id;
#endif
		u32_t m_tag;
		explicit sequencial_task( NRP<task_abstract> const& task, u32_t const& tag ):
			m_task(task),
#ifdef DEBUG_SEQ
			m_id( netp::atomic_incre(&s_seq_aid )),
#endif
			m_tag(tag)
		{
#ifdef DEBUG_SEQ
			NETP_INFO("[sequencial_task]new seq task: %u", m_id);
#endif
		}

		~sequencial_task() {}

		void run() {
			NETP_ASSERT( m_task != nullptr );
#ifdef DEBUG_SEQ
			NETP_INFO("[sequencial_task]seq task run: %u", m_id);
#endif
			m_task->run();
		}
	};
}}
#endif