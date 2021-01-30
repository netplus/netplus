#ifndef NETP_THREAD_IMPL_CONDITION_HPP
#define NETP_THREAD_IMPL_CONDITION_HPP

#include <mutex>
#include <condition_variable>

namespace netp { namespace impl {

	typedef std::mutex cv_mutex;

	template <class cv_mutex_t>
	struct unique_lock final {
		typedef std::unique_lock<cv_mutex_t> type;
	};

	typedef std::adopt_lock_t adopt_lock_t;
	typedef std::defer_lock_t defer_lock_t;

	typedef std::condition_variable condition_variable;
	typedef std::condition_variable_any condition_variable_any;

	typedef std::cv_status cv_status;
}}
#endif