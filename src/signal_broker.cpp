
#include <signal.h>

#include <netp/core.hpp>
#include <netp/signal_broker.hpp>
#include <netp/app.hpp>

namespace netp {

	signal_broker::signal_broker() {
#ifdef _NETP_WIN
		WIN_RegisterCtrlEventHandler();
#endif
	}

	signal_broker::~signal_broker() {
#ifdef _NETP_WIN
		WIN_UnRegisterCtrlEventHandler();
#endif
	}

	void signal_broker::signal_raise( int signo ) {
		event_broker_any::invoke<fn_signal_handler_t>(signo, signo);
	}

#ifdef _NETP_WIN
	void signal_broker::WIN_RegisterCtrlEventHandler() {
		if(SetConsoleCtrlHandler( (PHANDLER_ROUTINE) signal_broker::WIN_CtrlEvent_Handler , TRUE ) == FALSE ) {
			NETP_ERR( "[signal_broker]SetConsoleCtrlHandler failed, errno: %d", GetLastError() );
		}
	}
	void signal_broker::WIN_UnRegisterCtrlEventHandler() {
		if (SetConsoleCtrlHandler((PHANDLER_ROUTINE)signal_broker::WIN_CtrlEvent_Handler, false) == FALSE) {
			NETP_ERR("[signal_broker]SetConsoleCtrlHandler failed, errno: %d", GetLastError());
		}
	}

	BOOL WINAPI signal_broker::WIN_CtrlEvent_Handler( DWORD CtrlType ) {

		//windows ctrl event
		switch( CtrlType ) {
			case CTRL_C_EVENT:
				{
					NETP_WARN( "[signal_broker]CTRL_C_EVENT received" );
					::raise(SIGINT);
				}
				break;
			case CTRL_BREAK_EVENT:
				{
					NETP_WARN( "[signal_broker]CTRL_BREAK_EVENT received" );
					::raise(SIGINT);
				}
				break;

			case CTRL_SHUTDOWN_EVENT:
				{
					NETP_WARN( "[signal_broker]CTRL_SHUTDOWN_EVENT received" );
					::raise(SIGTERM);
				}
				break;
			case CTRL_CLOSE_EVENT:
				{
					NETP_WARN( "[signal_broker]CTRL_CLOSE_EVENT received" );
					::raise(SIGTERM);
				}
				break;
			case CTRL_LOGOFF_EVENT:
				{
   					NETP_WARN( "[signal_broker]CTRL_LOGOFF_EVENT received" );
					::raise(SIGTERM);
				}
				break;
			default:
				{
					NETP_WARN("[signal_broker]unknown windows event received");
					return false;
				}
		}
		return true;
	}
#endif
}