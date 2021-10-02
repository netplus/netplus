#ifndef _NETP_CONDITION_HPP_
#define _NETP_CONDITION_HPP_

#include <condition_variable>

#include <netp/core.hpp>
#include <netp/thread_impl/thread_basic.hpp>
#include <netp/thread_impl/mutex.hpp>
#include <netp/thread_impl/condition.hpp>

namespace netp {

	namespace impl {
		template <class mutex_t>
		struct interruption_checker final {
			enum checker_flag {
				F_IS_ANY = 1,
				F_DONE = 1<<1
			};

			NETP_DECLARE_NONCOPYABLE(interruption_checker)

			void __check_for_interrupt__() {
				if (thdata->interrupted) {
					thdata->interrupted = false;
					throw impl::interrupt_exception();
				}
			}
		public:
			impl::thread_data* const thdata;
			mutex_t* m;
			u8_t flag;

			explicit interruption_checker(mutex_t* mtx, condition_variable* cond) :
				thdata(netp::tls_get<thread_data>()),
				m(mtx),
				flag(0)
			{
				if (thdata) {
					lock_guard<spin_mutex> lg(thdata->mtx);
					__check_for_interrupt__();
					thdata->current_cond = cond;
					thdata->current_cond_mutex = mtx;
					m->lock();
				} else {
					m->lock();
				}
			}

			explicit interruption_checker(mutex_t* mtx, condition_variable_any* cond) :
				thdata(netp::tls_get<thread_data>()),
				m(mtx),
				flag(F_IS_ANY)
			{
				if (thdata) {
					lock_guard<spin_mutex> lg(thdata->mtx);
					if (thdata->interrupted) {
						thdata->interrupted = false;
						throw impl::interrupt_exception();
					}
					thdata->current_cond_any = cond;
					thdata->current_cond_any_mutex = mtx;
					m->lock();
				} else {
					m->lock();
				}
			}

			void unlock_if_locked() {
				if(flag&F_DONE) {return;}
				flag |= F_DONE;

				if (thdata) {
					NETP_ASSERT(m != nullptr);
					m->unlock();
					lock_guard<spin_mutex> lg(thdata->mtx);
					if (flag&F_IS_ANY) {
						thdata->current_cond_any_mutex = nullptr;
						thdata->current_cond_any = nullptr;
					} else {
						thdata->current_cond_mutex = nullptr;
						thdata->current_cond = nullptr;
					}
				} else {
					m->unlock();
				}
			}

			~interruption_checker() {
				unlock_if_locked();
			}
		};

		template <typename mutex_t>
		struct lock_on_exit final {
			mutex_t* m;
			lock_on_exit() :
				m(0)
			{}
			inline void activate(mutex_t& m_)
			{
				m_.unlock();
				m = &m_;
			}
			inline void deactive() 
			{
				if (m) {
					m->lock();
					m=0;
				}
			}
			~lock_on_exit() {
				if (m) { m->lock(); }
			}
		};
	}

	template <class _MutexT>
	class unique_lock;

	enum class cv_status {
		no_timeout = (int)std::cv_status::no_timeout,
		timeout = (int)std::cv_status::timeout
	};

	class condition_variable final {
		impl::mutex m_mtx;
		impl::condition_variable m_impl;
		NETP_DECLARE_NONCOPYABLE(condition_variable)
	public:
		condition_variable():m_mtx(),m_impl() {}
		~condition_variable() {}

		inline void notify_one() {
			scoped_lock<impl::mutex> _internal_lock(&m_mtx);
			m_impl.notify_one();
		}
		inline void no_interrupt_notify_one() {
			m_impl.notify_one();
		}
		inline void notify_all() {
			scoped_lock<impl::mutex> _internal_lock(&m_mtx);
			m_impl.notify_all();
		}
		inline void no_interrupt_notify_all() {
			m_impl.notify_all();
		}

		template <class _Rep, class _Period>
		inline cv_status no_interrupt_wait_for(unique_lock<mutex>& ulock, std::chrono::duration<_Rep, _Period> const& duration) {
			impl::unique_lock<impl::cv_mutex>::type ulk(*(ulock.mutex().impl()), impl::adopt_lock_t() );
			impl::cv_status cvs = m_impl.wait_for(ulk, duration);
			ulk.release();
			return cvs == std::cv_status::no_timeout ? cv_status::no_timeout : cv_status::timeout;
		}

		template <class _Clock, class _Duration>
		cv_status no_interrupt_wait_until(unique_lock<mutex>& ulock, std::chrono::time_point<_Clock, _Duration> const& atime) {
			impl::unique_lock<impl::cv_mutex>::type ulk( *(ulock.mutex().impl()), impl::adopt_lock_t() );
			impl::cv_status cvs = m_impl.wait_until(ulk, atime);
			ulk.release();
			return cvs == std::cv_status::no_timeout ? cv_status::no_timeout : cv_status::timeout;
		}

		void no_interrupt_wait(unique_lock<mutex>& ulock) {
			impl::unique_lock<impl::cv_mutex>::type ulk(*(ulock.mutex().impl()), impl::adopt_lock_t() );
			m_impl.wait(ulk);
			ulk.release();
		}
		void wait(unique_lock<mutex>& ulock) {
			{
				impl::lock_on_exit<mutex> guard;
				impl::interruption_checker<impl::mutex> _interruption_checker(&m_mtx, &m_impl);
				guard.activate(ulock.mutex());

				impl::unique_lock<impl::cv_mutex>::type ulk(m_mtx, impl::adopt_lock_t() );

				m_impl.wait(ulk);

				ulk.release();
				_interruption_checker.unlock_if_locked();
				guard.deactive();
			}
			netp::this_thread::__interrupt_check_point();
		}

		template <class _Rep, class _Period>
		inline cv_status wait_for(unique_lock<mutex>& ulock, std::chrono::duration<_Rep, _Period> const& duration) {
			impl::cv_status cvs;
			{
				impl::lock_on_exit<mutex> guard;
				impl::interruption_checker<impl::mutex> _interruption_checker(&m_mtx, &m_impl);
				guard.activate(ulock.mutex());

				impl::unique_lock<impl::cv_mutex>::type ulk(m_mtx, impl::adopt_lock_t() );
				cvs = m_impl.wait_for(ulk, duration);
				ulk.release();
				_interruption_checker.unlock_if_locked();
				guard.deactive();
			}
			netp::this_thread::__interrupt_check_point();
			return cvs == std::cv_status::no_timeout ? cv_status::no_timeout : cv_status::timeout;
		}

		template <class _Clock, class _Duration>
		cv_status wait_until(unique_lock<mutex>& ulock, std::chrono::time_point<_Clock, _Duration> const& atime ) {
			impl::cv_status cvs;
			{
				impl::lock_on_exit<mutex> guard;
				impl::interruption_checker<impl::mutex> _interruption_checker(&m_mtx, &m_impl);
				guard.activate(ulock.mutex());

				impl::unique_lock<impl::cv_mutex>::type ulk(m_mtx, impl::adopt_lock_t() );
				cvs = m_impl.wait_until(ulk, atime);
				ulk.release();
				_interruption_checker.unlock_if_locked();
				guard.deactive();
			}
			netp::this_thread::__interrupt_check_point();
			return cvs == std::cv_status::no_timeout ? cv_status::no_timeout : cv_status::timeout;
		}
	};

	class condition_variable_any final
	{
		impl::spin_mutex m_mtx;
		impl::condition_variable_any m_impl;
		NETP_DECLARE_NONCOPYABLE(condition_variable_any)
	public:
		condition_variable_any() {}
		~condition_variable_any() {}

		inline void notify_one() {
			scoped_lock<impl::spin_mutex> sl(&m_mtx);
			m_impl.notify_one();
		}
		inline void no_interrupt_notify_one() {
			m_impl.notify_one();
		}
		inline void notify_all() {
			scoped_lock<impl::spin_mutex> sl(&m_mtx);
			m_impl.notify_all();
		}
		inline void no_interrupt_notify_all() {
			m_impl.notify_all();
		}

		template <class _MutexT>
		inline void no_interrupt_wait(_MutexT& mutex) {
			m_impl.wait(mutex);
		}

		template <class _MutexT, class _Rep, class _Period>
		inline cv_status no_interrupt_wait_for(_MutexT& mutex, std::chrono::duration<_Rep, _Period> const& duration) {
			impl::cv_status cvs = m_impl.wait_for(mutex, duration);
			return cvs == std::cv_status::no_timeout ? cv_status::no_timeout : cv_status::timeout;
		}

		template <class _MutexT, class _Clock, class _Duration>
		inline cv_status no_interrupt_wait_until(_MutexT& mutex, std::chrono::time_point<_Clock, _Duration> const& atime) {
			impl::cv_status cvs = m_impl.wait_until(mutex, atime);
			return cvs == std::cv_status::no_timeout ? cv_status::no_timeout : cv_status::timeout;
		}

		template <class _MutexT>
		inline void wait( _MutexT& mtx ) {
			{
				impl::lock_on_exit<_MutexT> guard;
				impl::interruption_checker<impl::spin_mutex> _interruption_checker(&m_mtx, &m_impl);
				guard.activate(mtx);

				m_impl.wait(m_mtx);
			}
			netp::this_thread::__interrupt_check_point();
		}

		template <class _MutexT, class _Rep, class _Period>
		inline cv_status wait_for(_MutexT& mtx, std::chrono::duration<_Rep, _Period> const& duration) {
			impl::cv_status cvs;
			{
				impl::lock_on_exit<_MutexT> guard;
				impl::interruption_checker<impl::spin_mutex> _interruption_checker(&m_mtx, &m_impl);
				guard.activate(mtx);

				cvs = m_impl.wait_for(m_mtx, duration);
			}
			netp::this_thread::__interrupt_check_point();
			return cvs == std::cv_status::no_timeout ? cv_status::no_timeout : cv_status::timeout;
		}

		template <class _MutexT, class _Clock, class _Duration>
		cv_status wait_until(_MutexT& mtx, std::chrono::time_point<_Clock, _Duration> const& atime) {
			impl::cv_status cvs;
			{
				impl::lock_on_exit< _MutexT > guard;
				impl::interruption_checker<impl::spin_mutex> _interruption_checker(&m_mtx, &m_impl);
				guard.activate(mtx);

				cvs = m_impl.wait_until(m_mtx, atime);
			}
			netp::this_thread::__interrupt_check_point();
			return cvs == std::cv_status::no_timeout ? cv_status::no_timeout : cv_status::timeout;
		}
	};

	typedef condition_variable condition;
	typedef condition_variable_any condition_any;
}

#endif //end _NETP_THREAD_CONDITION_H_
