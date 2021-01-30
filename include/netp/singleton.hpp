#ifndef _NETP_SINGLETON_HPP_
#define _NETP_SINGLETON_HPP_

#include <mutex>
#include <atomic>
#include <netp/core/compiler.hpp>

namespace netp {

	template <class T>
	class singleton {
	public:
		__NETP_FORCE_INLINE static T* instance() {
			T* ins = s_instance.load(std::memory_order_acquire);
			if ( NETP_LIKELY(nullptr != ins)) {
				return ins;
			}

			static std::mutex __s_instance_mutex;
			std::lock_guard<std::mutex> _lg(__s_instance_mutex);
			ins = s_instance.load(std::memory_order_acquire);
			if ( NETP_LIKELY(nullptr == ins)) {
				//@note, try ..catch would prevent inline
				//if there is a exception , we just let it bubbling up
				//try {
					ins = new T();
					singleton<T>::schedule_for_destroy(singleton<T>::destroy_instance);
					s_instance.store(ins, std::memory_order_release);
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
				static std::mutex __s_instance_for_destroy_mutex;
				std::lock_guard<std::mutex> lg(__s_instance_for_destroy_mutex);
				if (ins != nullptr) {
					delete ins;
					s_instance.store(nullptr, std::memory_order_release);
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
	};

	template<class T>
	std::atomic<T*> singleton<T>::s_instance(nullptr);

#define DECLARE_SINGLETON_FRIEND(T) \
	friend class netp::singleton<T>
}

#endif//endof _NETP_SINGLETION_HPP_