#ifndef _NETP_IO_EVENT_HPP
#define _NETP_IO_EVENT_HPP

#include <functional>
#include <netp/core.hpp>
#include <netp/packet.hpp>
#include <netp/address.hpp>

//in nano seconds
#define NETP_POLLER_WAIT_IGNORE_DUR ((50LL)) 

namespace netp {
	
	typedef std::function<void(const int )> fn_aio_event_t;
	typedef std::function<void(const int, NRP<netp::packet> const&)> fn_aio_read_event_t;
	typedef std::function<void(const int, NRP<netp::packet> const&, address const&)> fn_aio_read_from_event_t;

#ifdef NETP_HAS_POLLER_IOCP
	//class packet;
	//give iocp a different struct, such as ,,iocp_result
	struct iocp_result {
		SOCKET fd;
		union {
			int len;
			int code;
		}intv;
		byte_t* data;
	};

	typedef std::function<int(const iocp_result&)> fn_iocp_event_t;
	typedef std::function<int(void* ol)> fn_overlapped_io_event;
#endif
}

#endif