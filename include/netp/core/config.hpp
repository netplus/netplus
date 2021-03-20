#ifndef _NETP_CORE_CONFIG_H_
#define _NETP_CORE_CONFIG_H_

#ifndef __NETP_VER
	#define __NETP_VER 100
	#define __NETP_VER_STR "netplus v1.00"
#endif

#include <netp/core/compiler.hpp>
#include <netp/core/platform.hpp>
#include <netp/core/macros.hpp>

//split SYSLOG message into piece that less than 1024
#ifndef NETP_SPLIT_SYSLOG
	#define NETP_SPLIT_SYSLOG 1
#endif

#ifndef NETP_SYSLOG_SPLIT_LENGTH
	#define NETP_SYSLOG_SPLIT_LENGTH 1024
#endif

//log level
/** PLASE REFER TO netp::log::log_level  */
#ifndef NETP_DEFAULT_LOG_LEVEL
	#define NETP_DEFAULT_LOG_LEVEL 3
#endif

#ifndef NETP_USE_FILE_LOGGER
	#ifdef _NETP_WIN
		#define NETP_USE_FILE_LOGGER	1
	#else
		#define NETP_USE_FILE_LOGGER	0
	#endif
#endif

#ifndef NETP_USE_SYS_LOGGER
	#ifdef _NETP_GNU_LINUX
		#define NETP_USE_SYS_LOGGER 1
	#else
		#define NETP_USE_SYS_LOGGER 0
	#endif
#endif

#ifdef __ANDROID__
	#define NETP_ENABLE_ANDROID_LOG 1
#else
	#define NETP_ENABLE_CONSOLE_LOGGER 1
#endif

#ifdef _DEBUG
	#define __DEFAULT_CONSOLE_LOG_LEVEL 3
	#define __DEFAULT_FILE_LOG_LEVEL 3
	#define __DEFAULT_SYS_LOG_LEVEL 3
#else
	#define __DEFAULT_CONSOLE_LOG_LEVEL 2
	#define __DEFAULT_SYS_LOG_LEVEL 2
	#define __DEFAULT_FILE_LOG_LEVEL 2
#endif


#ifndef	NETP_FILE_LOGGER_LEVEL
	#define NETP_FILE_LOGGER_LEVEL	__DEFAULT_FILE_LOG_LEVEL
#endif

#ifndef NETP_SYS_LOGGER_LEVEL
	#define NETP_SYS_LOGGER_LEVEL	__DEFAULT_SYS_LOG_LEVEL
#endif

#ifndef NETP_CONSOLE_LOGGER_LEVEL
	#define NETP_CONSOLE_LOGGER_LEVEL	__DEFAULT_CONSOLE_LOG_LEVEL
#endif

#define NETP_ENABLE_EPOLL
#define NETP_ENABLE_KQUEUE

#define NETP_ENABLE_IOCP

//FOR IO MODE
#if defined(NETP_ENABLE_IOCP) && defined(_NETP_WIN)
	#define NETP_HAS_POLLER_IOCP
#elif defined(NETP_ENABLE_EPOLL) && (defined(_NETP_GNU_LINUX) || defined(_NETP_ANDROID))
	#define NETP_HAS_POLLER_EPOLL
#elif defined(NETP_ENABLE_KQUEUE) && defined(_NETP_APPLE)
	#define NETP_HAS_POLLER_KQUEUE
#else
	#define NETP_HAS_POLLER_SELECT
#endif

// for epoll using
#ifdef NETP_ENABLE_EPOLL
	#define NETP_EPOLL_CREATE_HINT_SIZE			(1024)	///< max size of epoll control
	#define NETP_EPOLL_PER_HANDLE_SIZE			(128)	///< max size of per epoll_wait
	#define NETP_IO_POLLER_EPOLL_USE_ET
#endif


//#define NETP_ENABLE_TASK_TRACK
#ifdef NETP_ENABLE_TRACK_TASK
	#define NETP_TRACE_TASK NETP_DEBUG
#else
	#define NETP_TRACE_TASK(...)
#endif

//in seconds
#define NETP_DEFAULT_TCP_KEEPALIVE_IDLETIME	(30)
//in seconds
#define NETP_DEFAULT_TCP_KEEPALIVE_INTERVAL	(30)
#define NETP_DEFAULT_TCP_KEEPALIVE_PROBES		(6)

#define NETP_RPC_QUEUE_SIZE (200)


//#define NETP_ENABLE_WEBSOCKET

#ifdef DEBUG
//	#define NETP_ENABLE_TRACE_DH
//	#define NETP_ENABLE_TRACE_SOCKET_API
//	#define NETP_ENABLE_TRACE_IOE
//	#define NETP_ENABLE_TRACE_SOCKET
//	#define NETP_ENABLE_TRACE_CHANNEL

//	#define NETP_ENABLE_DEBUG_STREAM
//	#define NETP_ENABLE_TRACE_TIMER
//#define NETP_ENABLE_DEBUG_STACK_SIZE

//	#define NETP_ENABLE_TRACE_HTTP
//	#define NETP_ENABLE_TRACE_RPC
// #define __NETP_DEBUG_BROKER_INVOKER_

//#define NETP_DEBUG_WATCH_CTX_FLAG

//#define NETP_ENABLE_TRACE_HTTP_MESSAGE
#endif

#ifdef NETP_HAS_POLLER_IOCP
//	#define NETP_ENABLE_TRACE_SOCKET
//	#define NETP_ENABLE_TRACE_IOE
#endif

#ifdef NETP_ENABLE_TRACE_SOCKET_API
	#define NETP_TRACE_SOCKET_API NETP_INFO
#else
	#define NETP_TRACE_SOCKET_API(...)
#endif

#ifdef NETP_ENABLE_TRACE_CHANNEL
	#define NETP_TRACE_CHANNEL NETP_INFO
#else
	#define NETP_TRACE_CHANNEL(...)
#endif

#ifdef NETP_ENABLE_TRACE_IOE
	#define NETP_TRACE_IOE NETP_INFO
#else
	#define NETP_TRACE_IOE(...)
#endif

#ifdef NETP_ENABLE_TRACE_SOCKET
	#define NETP_TRACE_SOCKET NETP_INFO
#else
	#define NETP_TRACE_SOCKET(...)
#endif

#ifdef NETP_ENABLE_DEBUG_STREAM
	#define NETP_TRACE_STREAM NETP_INFO
#else
	#define NETP_TRACE_STREAM(...)
#endif


#ifdef NETP_ENABLE_TRACE_HTTP
	#define TRACE_HTTP NETP_INFO
#else
	#define TRACE_HTTP(...)
#endif

#ifdef NETP_ENABLE_TRACE_RPC
	#define TRACE_RPC NETP_INFO
#else
	#define TRACE_RPC(...)
#endif


#endif // end for _CONFIG_NETP_CONFIG_H_
