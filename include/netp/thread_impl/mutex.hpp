#ifndef _NETP_THREAD_IMPL_MUTEX_HPP_
#define _NETP_THREAD_IMPL_MUTEX_HPP_

#include <mutex>
#include <netp/core.hpp>

#include <netp/thread_impl/mutex_basic.hpp>

namespace netp {

	namespace impl { typedef std::mutex mutex; }

	namespace _mutex_detail {
		class mutex final
		{
			impl::mutex m_impl;

			NETP_DECLARE_NONCOPYABLE(mutex)
		public:
			mutex() :
				m_impl()
			{
			}
			~mutex() {
				_MUTEX_DEBUG_CHECK_FOR_LOCK_
			}

			inline void lock() {
				_MUTEX_IMPL_LOCK(m_impl);
			}

			inline void unlock() {
				_MUTEX_IMPL_UNLOCK(m_impl);
			}

			inline bool try_lock() {
				_MUTEX_IMPL_TRY_LOCK(m_impl);
			}
			inline impl::mutex* impl() const { return (impl::mutex*)(&m_impl); }
		};
	}
}

namespace netp {
#ifdef _NETP_USE_MUTEX_WRAPPER
	typedef netp::_mutex_detail::mutex mutex;
#else
	typedef netp::impl::mutex mutex;
#endif
}

#endif //_NETP_THREAD_MUTEX_H_
