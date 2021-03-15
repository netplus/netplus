#ifndef _NETP_WINSOCK_HPP
#define _NETP_WINSOCK_HPP

#include <netp/core.hpp>

#ifdef _NETP_WIN
#include <mswsock.h>

namespace netp { namespace os {
	enum winsock_api_ex {
		API_CONNECT_EX = 0,
		API_ACCEPT_EX,
		API_GET_ACCEPT_EX_SOCKADDRS,
		API_RECVMSG,
		API_MAX
	};

	extern void __init_winapi_ex(void** api_address);

	inline void* load_api_ex_address(winsock_api_ex id) {
		static void* __s_api_address[API_MAX] = {0};
		if (0 == __s_api_address[id] ) {
			__init_winapi_ex(__s_api_address);
		}
		return __s_api_address[id];
	}

	inline void winsock_init() {
		WSADATA wsaData;
		int result = ::WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (result != 0) {
			NETP_THROW("INIT WINSOCK FAILED");
		}
	}

	inline void winsock_deinit() {
		::WSACleanup();
	}
}}
#endif //iswin

#endif