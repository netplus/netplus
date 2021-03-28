#ifndef _NETP_LOG_NET_LOGGER_HPP_
#define _NETP_LOG_NET_LOGGER_HPP_

#include <list>

#include <netp/promise.hpp>
#include <netp/packet.hpp>
#include <netp/timer.hpp>

#include <netp/logger/logger_abstract.hpp>

namespace netp {
	class rpc;
	class io_event_loop;
}

namespace netp { namespace logger {

		class net_logger :
			public logger_abstract {

			enum class flag {
				f_connecting = 1,
				f_connected = 1<<1,
				f_close_called = 1<<2,
				f_writing = 1<<3
			};

		private:
			NRP<io_event_loop> m_loop;
			u8_t m_flag;
			string_t m_server;
			NRP<rpc> m_rpc;
			std::list<NRP<netp::packet>> m_loglist;

			void _tm_redial(NRP<timer> const& tm);

			void _do_dial(NRP<promise<int>> const& p);
			void _do_close(NRP<promise<int>> const& p);

			void _do_push_done(int rt);
			void _do_push();
		public:
			net_logger( std::string const& server );
			~net_logger();

			void write(log_mask mask, char const* log, netp::u32_t len);

			NRP<promise<int>> dial();
			NRP<promise<int>> close();
		};
	}
}
#endif
