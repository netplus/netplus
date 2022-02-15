#ifndef _NETP_LOG_LOGGER_MANAGER_HPP_
#define _NETP_LOG_LOGGER_MANAGER_HPP_

#include <vector>

#include <netp/singleton.hpp>
#include <netp/smart_ptr.hpp>
#include <netp/logger/logger_abstract.hpp>

namespace netp {

	class logger_broker:
		public netp::non_atomic_ref_base
	{
	public:
		logger_broker();
		virtual ~logger_broker();

		void init(); //ident for linux
		void deinit();

		void add( NRP<logger::logger_abstract> const& logger);
		void remove(NRP<logger::logger_abstract> const& logger);

		void write(logger::log_mask mask,char const* const file, int line, char const* const func, ... );
	private:
		std::vector<NRP<logger::logger_abstract>, netp::allocator<NRP<logger::logger_abstract>> > m_loggers;
		bool m_isInited;
	};
}

#endif