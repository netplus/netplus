#ifndef _NETP_APP_HPP_
#define _NETP_APP_HPP_

#include <netp/core.hpp>
#include <netp/signal_broker.hpp>
#include <netp/test.hpp>

#include <netp/logger_broker.hpp>
#include <netp/event_loop.hpp>

#define NETP_DEBUG_OBJECT_SIZE
#define NETP_DEBUG_ARCH_INFO
namespace netp {

	//typedef std::function<void()> fn_app_hook_t;
#if defined(_NETP_DEBUG) || 1
	#define	NETP_TRACE_APP(...)				(netp::logger_broker::instance()->write( netp::logger::LOG_MASK_INFO, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__ ))
#else
	#define	NETP_TRACE_APP(...)
#endif

	enum class app_state {
		s_idle,
		s_init_begin,
		s_init_done,
		s_loop_run,
		s_loop_exit_notified,
		s_loop_wait_done
	};

	enum parse_cfg_mode {
		mode_do,
		mode_fetch
	};

	class app:
		public netp::singleton<app>
	{
	private:

		NRP<event_loop_group> m_def_loop_group;

		//absolute path of the execution app
		std::string m_app_name;
		std::string m_app_path;
		netp::mutex m_mutex;
		netp::condition m_cond;

		u32_t m_loop_count;
		u32_t m_channel_read_buf_size; //in bytes
		u32_t m_channel_tx_limit_clock; //in millis
		bool m_is_cfg_json_loaded;
		bool m_should_exit;

		std::atomic<app_state> m_app_state;
		std::vector<std::tuple<int,i64_t>> m_signo_tuple_vec;
		std::vector<std::string> m_dns_hosts; //dotip
		std::string m_logfilepathname;

		void _app_thread_init();
		void _app_thread_deinit();

		int _init_from_cfg_json(const char* jsonfile);

		int __parse_cfg(parse_cfg_mode mode, int argc, char** argv, std::string const& param, std::string& value);
		int _parse_cfg_fetch(int argc, char** argv, std::string const& param, std::string& value);
		void _parse_cfg(int argc, char** argv);

		void _init();
		void _deinit();

		void _log_init();
		void _log_deinit();

#ifdef NETP_DEBUG_ARCH_INFO
		void _dump_arch_info();
#endif

#ifdef NETP_DEBUG_OBJECT_SIZE
		void _dump_sizeof();
#endif

		void _signal_init();
		void _signal_deinit();

		void _net_init();
		void _net_deinit();

		void _event_loop_init();
		void _event_loop_deinit();
	public:
		//@warn: if we do need to create app on heap, we should always use new/delete, or std::shared_ptr
		app();
		~app();

		void cfg_loop_count(u32_t c);
		void cfg_channel_read_buf(u32_t buf_in_kbytes);

		__NETP_FORCE_INLINE
		u32_t channel_tx_limit_clock() const { return m_channel_tx_limit_clock; }

		void cfg_channel_tx_limit_clock(u32_t clock_in_millis);
		void cfg_add_dns(std::string const& dns_ns);
		void cfg_log_filepathname(std::string const& logfilepathname_);
		void dns_hosts(std::vector<netp::string_t, netp::allocator<netp::string_t>>&) const ;

		std::string app_name() const;
		std::string app_path() const;

		void init(int argc, char** argv);
		
		//startup loop & dns
		void start_loop();
		void stop_loop();

		void terminate_loop();	
		//for any botan related app, we should call this line explicitly when we exit main
		//or call Botan::initialize_allocator() explicitly before netp::app::instance()
		void wait_loop();

		//ISSUE: if the waken thread is main thread, we would get stuck here
		void handle_signal(int signo);
		bool should_exit() const;

		int wait() {
			netp::unique_lock<netp::mutex> ulk(m_mutex);
			while (!m_should_exit) {
				m_cond.wait(ulk);
			}
			return 0;
		}

		template <class _Rep, class _Period>
		int wait_for(std::chrono::duration<_Rep, _Period> const& duration)
		{
			netp::unique_lock<netp::mutex> ulk(m_mutex);
			const std::chrono::time_point<std::chrono::steady_clock> exit_tp = std::chrono::steady_clock::now() + duration;
			while (!m_should_exit) {
				const std::chrono::time_point<std::chrono::steady_clock> now = std::chrono::steady_clock::now();
				if (now>=exit_tp) {
					break;
				}
				m_cond.wait_for(ulk, exit_tp-now );
			}
			return 0;
		}

		template <class _Clock, class _Duration>
		int wait_until(std::chrono::time_point<_Clock, _Duration> const& tp)
		{
			netp::unique_lock<netp::mutex> ulk(m_mutex);
			while (!m_should_exit) {
				if (_Clock::now() >= tp) {
					break;
				}
				//NETP_INFO("to: %lld", tp.time_since_epoch().count() );
				m_cond.wait_until(ulk, tp);
			}
			return 0;
		}

		__NETP_FORCE_INLINE
		NRP<netp::event_loop_group> const& def_loop_group() const { return m_def_loop_group; }
		//__NETP_FORCE_INLINE
		//NRP<netp::logger_broker> const& logger() const { return m_logger_broker; }
	};

#define NETP_LOGGER_BROKER	( netp::logger_broker::instance())

#define	NETP_VERBOSE(...)		(NETP_LOGGER_BROKER->write( netp::logger::LOG_MASK_VERBOSE, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__ ))
#define	NETP_INFO(...)				(NETP_LOGGER_BROKER->write( netp::logger::LOG_MASK_INFO, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__ ))
#define	NETP_WARN(...)			(NETP_LOGGER_BROKER->write( netp::logger::LOG_MASK_WARN, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__ ))
#define	NETP_ERR(...)				(NETP_LOGGER_BROKER->write( netp::logger::LOG_MASK_ERR, __FILE__, __LINE__, __FUNCTION__,__VA_ARGS__ ))

#ifndef _NETP_DEBUG
#undef NETP_VERBOSE
#define NETP_VERBOSE(...)
#endif

	class app_test_unit :
		public netp::test_unit
	{
	public:
		bool run();
		void test_generic_check();
		void benchmark_hash();
	};
}
#endif