#ifndef _CORE_PLATFORM_NETP_PLATFORM_WIN_HPP_
#define _CORE_PLATFORM_NETP_PLATFORM_WIN_HPP_

//note
//we can set this value very high to enable maxmium fd check for each select op
//2048 has been tested on win10
//if you add more fd to check sets, select will always return 10038
#ifdef FD_SETSIZE //disable warning
	#undef FD_SETSIZE
#endif
#define FD_SETSIZE 1000 //will only affact MODE_SELECT

//#ifndef SOCKET
//typedef int SOCKET ;

#ifndef WIN32_LEAN_AND_MEAN
	#define WIN32_LEAN_AND_MEAN
#endif

//generic
#include <SDKDDKVer.h>
#include <malloc.h>
#include <winsock2.h> //WIN32_LEAN_AND_MEAN will exclude this from Windows.h
#include <WS2tcpip.h> //for inet_ntop, etc
#include <mstcpip.h>
#include <Windows.h> //will include winsock.h
#include <tchar.h>
#include <io.h>

#include "../../../../3rd/getopt/getopt.h"

//there is no ws2_64
#pragma comment(lib,"ws2_32.lib")
#pragma comment(lib,"mswsock.lib")


//for shutdown
#define SHUT_RD		SD_RECEIVE
#define SHUT_WR		SD_SEND
#define SHUT_RDWR	SD_BOTH

#define NETP_CLOSE_SOCKET	closesocket
#define NETP_DUP				_dup
#define NETP_DUP2			_dup2

#if _MSC_VER<1900
	#define snprintf	sprintf_s
#endif

#ifdef max
	#undef max
#endif

#ifdef min
	#undef min
#endif

//remark: WSAGetLastError() == GetLastError(), but there is no gurantee for future change.
#define netp_last_errno() NETP_NEGATIVE((long)::GetLastError())
#define netp_set_last_errno(e) (::SetLastError( NETP_ABS(e)))
#define netp_socket_set_last_errno(e) (::WSASetLastError(NETP_ABS(e)))
#define netp_socket_get_last_errno() NETP_NEGATIVE((long)::WSAGetLastError())

namespace netp {
	typedef int socklen_t;
}
#endif