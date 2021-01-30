#ifndef _NETP_SIGNAL_BROKER_HPP
#define _NETP_SIGNAL_BROKER_HPP

#include <signal.h>
#include <vector>

#include <netp/core.hpp>
#include <netp/smart_ptr.hpp>
#include <netp/event_broker.hpp>
#include <netp/mutex.hpp>

namespace netp {
	typedef std::function<void(int signo)> fn_signal_handler_t;

	class signal_broker:
		public netp::event_broker_any,
		public netp::ref_base {

	public:
		signal_broker();
		~signal_broker();

	public:
		void signal_raise( int signo );//simulate signo trigger

#ifdef _NETP_WIN
		//WINDOWS ONLY
		static BOOL WINAPI WIN_CtrlEvent_Handler(DWORD CtrlType);
		static void WIN_RegisterCtrlEventHandler();
		static void WIN_UnRegisterCtrlEventHandler();
#endif
		DECLARE_SINGLETON_FRIEND(signal_broker);
	};
}

#endif