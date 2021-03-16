#include <netp/core/platform.hpp>

#ifdef _NETP_WIN
#include <netp/os/winsock_helper.hpp>

namespace netp { namespace os {
	void __init_winapi_ex(void** api_address) {
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
}}

#endif