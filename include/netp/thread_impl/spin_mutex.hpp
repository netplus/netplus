#ifndef NETP_THREAD_IMPL_SPIN_MUTEX_HPP_
#define NETP_THREAD_IMPL_SPIN_MUTEX_HPP_

#include <atomic>
#include <netp/core.hpp>


//pthread_spin_init might fail for several reason, disable it
//https://linux.die.net/man/3/pthread_spin_init
//#if defined(_NETP_GNU_LINUX)
//	#define _NETP_USE_PTHREAD_SPIN_AS_SPIN_MUTEX
//	#include <pthread.h>
//#endif

#include <netp/thread_impl/mutex_basic.hpp>

namespace netp { namespace impl {

		class atomic_spin_mutex final {
			NETP_DECLARE_NONCOPYABLE(atomic_spin_mutex)
	#if defined(_NETP_GNU_LINUX) || (_MSC_VER>=1900) || defined(_NETP_APPLE)
					std::atomic_flag m_flag = ATOMIC_FLAG_INIT; //force for sure
	#else
					std::atomic_flag m_flag;
	#endif
		public:
			atomic_spin_mutex();
			~atomic_spin_mutex();

			void lock();
			void unlock();
			bool try_lock();
		};

#ifdef _NETP_USE_PTHREAD_SPIN_AS_SPIN_MUTEX
		class pthread_spin_mutex {

			NETP_DECLARE_NONCOPYABLE(pthread_spin_mutex)
				pthread_spinlock_t m_spin;
		public:
			pthread_spin_mutex() :
				m_spin()
			{
				pthread_spin_init(&m_spin, 0);
			}
			~pthread_spin_mutex() {
				pthread_spin_destroy(&m_spin);
			}
			void lock() {
				pthread_spin_lock(&m_spin);
			}
			void unlock() {
				pthread_spin_unlock(&m_spin);
			}
			bool try_lock() {
				return pthread_spin_trylock(&m_spin) == 0;
			}
		};
		typedef pthread_spin_mutex spin_mutex;
#else
		typedef atomic_spin_mutex spin_mutex;
#endif

	}

	namespace _mutex_detail {
		class spin_mutex final {
		public:
			impl::spin_mutex m_impl;
			NETP_DECLARE_NONCOPYABLE(spin_mutex)
		public:
			spin_mutex() :
				m_impl()
			{
			}
			~spin_mutex() {
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
			inline impl::spin_mutex* impl() const { return (impl::spin_mutex*)&m_impl; }
		};
}}

namespace netp {
#ifdef _NETP_USE_MUTEX_WRAPPER
		typedef netp::_mutex_detail::spin_mutex spin_mutex;
#else
		typedef netp::impl::spin_mutex spin_mutex;
#endif
}

#endif
