#ifndef _NETP_IO_MONITOR_HPP
#define _NETP_IO_MONITOR_HPP

#include <netp/smart_ptr.hpp>

namespace netp {
	struct io_ctx;
	class io_monitor:
		public netp::ref_base
	{
	public:
		virtual void io_notify_terminating(int status, io_ctx*) = 0;
		virtual void io_notify_read(int status, io_ctx* ctx) = 0;
		virtual void io_notify_write(int status, io_ctx* ctx) = 0;
	};
}

#endif