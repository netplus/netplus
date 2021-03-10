#ifndef _NETP_IO_EVENT_HPP
#define _NETP_IO_EVENT_HPP

#include <functional>
#include <netp/core.hpp>
#include <netp/packet.hpp>

//in nano seconds
#define NETP_POLLER_WAIT_IGNORE_DUR ((50LL)) 

namespace netp {
	
	//class packet;
	//give iocp a different struct, such as ,,iocp_result
	struct iocp_result {
		SOCKET fd;
		union {
			int len;
			int code;
		};
		NRP<netp::packet> data;
	};

	typedef std::function<void(const int )> fn_aio_event_t;

#ifdef NETP_IO_POLLER_IOCP
#ifdef _NETP_AM64
	typedef i64_t overlapped_return_t;
#else
	typedef i32_t overlapped_return_t;
#endif
	typedef std::function<overlapped_return_t(void* ol)> fn_overlapped_io_event;
#endif
}

#endif