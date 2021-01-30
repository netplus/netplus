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

	inline void __init_winapi_ex(void** api_address) {
		GUID guid = WSAID_CONNECTEX;
		LPFN_CONNECTEX fn_connectEx = nullptr;
		DWORD dwBytes;
		SOCKET fd = ::WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, 0);
		if (fd == NETP_SOCKET_ERROR) {
			NETP_THROW("CREATE FD FAILED");
		}

		int loadrt = ::WSAIoctl(fd, SIO_GET_EXTENSION_FUNCTION_POINTER,
			&guid, sizeof(guid),
			&fn_connectEx, sizeof(fn_connectEx),
			&dwBytes, nullptr, nullptr);

		if (loadrt != 0) {
			NETP_THROW("load address: WSAID_CONNECTEX failed");
		}
		NETP_ASSERT(fn_connectEx != 0);
		api_address[API_CONNECT_EX] = (void*)fn_connectEx;

		guid = WSAID_ACCEPTEX;
		LPFN_ACCEPTEX fn_acceptEx = nullptr;
		loadrt = ::WSAIoctl(fd, SIO_GET_EXTENSION_FUNCTION_POINTER,
			&guid, sizeof(guid),
			&fn_acceptEx, sizeof(fn_acceptEx),
			&dwBytes, nullptr, nullptr);

		if (loadrt != 0) {
			NETP_THROW("load address: WSAID_ACCEPTEX failed");
		}
		NETP_ASSERT(fn_acceptEx != 0);
		api_address[API_ACCEPT_EX] = (void*)fn_acceptEx;

		guid = WSAID_GETACCEPTEXSOCKADDRS;
		LPFN_GETACCEPTEXSOCKADDRS fn_getacceptexsockaddrs = nullptr;
		loadrt = ::WSAIoctl(fd, SIO_GET_EXTENSION_FUNCTION_POINTER,
			&guid, sizeof(guid),
			&fn_getacceptexsockaddrs, sizeof(fn_getacceptexsockaddrs),
			&dwBytes, nullptr, nullptr);

		if (loadrt != 0) {
			NETP_THROW("load address: WSAID_GETACCEPTEXSOCKADDRS failed");
		}
		NETP_ASSERT(fn_getacceptexsockaddrs != 0);
		api_address[API_GET_ACCEPT_EX_SOCKADDRS] = (void*)fn_getacceptexsockaddrs;

		guid = WSAID_WSARECVMSG;
		LPFN_WSARECVMSG fn_wsarecvmsg = nullptr;
		loadrt = ::WSAIoctl(fd, SIO_GET_EXTENSION_FUNCTION_POINTER,
			&guid, sizeof(guid),
			&fn_wsarecvmsg, sizeof(fn_wsarecvmsg),
			&dwBytes, nullptr, nullptr);
		if (loadrt != 0) {
			NETP_THROW("load address: WSAID_WSARECVMSG failed");
		}
		api_address[API_RECVMSG] = (void*)fn_wsarecvmsg;

		NETP_CLOSE_SOCKET(fd);
	}

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