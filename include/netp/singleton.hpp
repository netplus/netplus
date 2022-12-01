#ifndef _NETP_SINGLETON_HPP_
#define _NETP_SINGLETON_HPP_

#include <mutex>
#include <atomic>
#include <netp/core/compiler.hpp>

namespace netp {

	//@note:
	//the impl promise thread-safe for instance creation
	//but non-thread-safe for destroy and creation case
	//the programmer should be sure that there is no place to run xx::instance right after xx::destroy_instance()
	template <class T>
	class singleton {
	public:
		__NETP_FORCE_INLINE static T* instance() {
			//we have a mutex guard below,,, 
			T* ins = s_instance.load(std::memory_order_relaxed);
			if ( NETP_LIKELY(nullptr != ins)) {
				return ins;
			}

			static std::mutex __s_instance_mutex;
			std::lock_guard<std::mutex> _lg(__s_instance_mutex);
			//we have a mutex guard above,,, 
			ins = s_instance.load(std::memory_order_relaxed);
			if ( NETP_LIKELY(nullptr == ins)) {
				//@note, try ..catch would prevent inline
				//if there is a exception , we just let it bubbling up
				//try {
					ins = ::new T();
					s_instance.store(ins, std::memory_order_release);

					//preventing dup-register from the following case
					//1, for debug purpose, manually call destroy_instance, then call instance() again
					bool expected = false;
					if (s_exit_registered.compare_exchange_strong(expected, true, std::memory_order_acq_rel, std::memory_order_acquire)) {
						singleton<T>::schedule_for_destroy(singleton<T>::destroy_instance);
					}
				//} catch (...) {
					//issue might be
				//	delete ins;
				//	NETP_THROW("new T() | schedule_for_destroy failed!!!");
				//}
			}
			return ins;
		}

		static void destroy_instance() {
			T* ins = s_instance.load(std::memory_order_acquire);
			if (nullptr != ins) {
				//static std::mutex __s_instance_for_destroy_mutex;
				//std::lock_guard<std::mutex> lg(__s_instance_for_destroy_mutex);				
				T* expected = ins;
				if (s_instance.compare_exchange_strong(expected, nullptr, std::memory_order_acq_rel, std::memory_order_acquire)) {
					::delete ins;
				}
			}
		}

	protected:
		static void schedule_for_destroy(void(*func)()) {
			std::atexit(func);
		}
	protected:
		singleton() {}
		virtual ~singleton() {}

		singleton(const singleton<T>&);
		singleton<T>& operator=(const singleton<T>&);

		static std::atomic<T*> s_instance;
		static std::atomic<bool> s_exit_registered;
	};

	template<class T>
	std::atomic<T*> singleton<T>::s_instance(nullptr);

	template<class T>
	std::atomic<bool> singleton<T>::s_exit_registered(false);

#define DECLARE_SINGLETON_FRIEND(T) \
	friend class netp::singleton<T>
}

#endif//endof _NETP_SINGLETION_HPP_