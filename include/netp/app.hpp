#ifndef _NETP_APP_HPP_
#define _NETP_APP_HPP_

#include <netp/core.hpp>
#include <netp/signal_broker.hpp>
#include <netp/test.hpp>

#include <netp/logger_broker.hpp>
#include <netp/io_event_loop.hpp>
#include <netp/dns_resolver.hpp>

//#define NETP_DEBUG_OBJECT_SIZE

namespace netp {

	typedef std::function<void()> fn_app_hook_t;

	struct app_cfg {

		std::string logfilepathname;
		std::vector<std::string> dnsnses; //dotip
		u32_t poller_max[T_POLLER_MAX];
		u32_t poller_count[T_POLLER_MAX];
		event_loop_cfg event_loop_cfgs[T_POLLER_MAX];

		fn_app_hook_t app_startup_prev;
		fn_app_hook_t app_startup_post;
		fn_app_hook_t app_exit_prev;
		fn_app_hook_t app_exit_post;

		fn_app_hook_t app_event_loop_init_prev;
		fn_app_hook_t app_event_loop_init_post;
		fn_app_hook_t app_event_loop_deinit_prev;
		fn_app_hook_t app_event_loop_deinit_post;

		bool __cfg_json_checked;
		void __init_from_cfg_json(const char* jsonfile);
		void __parse_cfg(int argc, char** argv);

		void __cfg_default_loop_cfg();
	public:
		app_cfg(int argc, char** argv);
		app_cfg();

		//<0, disable
		//=0, std::thread::hardware_concurrency()
		//<std::thread::hardware_concurrency()<<1, accepted
		//max std::thread::hardware_concurrency()<<1
		void cfg_poller_max(io_poller_type t, int c);
		void cfg_poller_count(io_poller_type t, int c);

		void cfg_channel_buf(io_poller_type t, int buf_in_kbytes);
		void cfg_add_dns(std::string const& dns_ns);

		void cfg_log_filepathname(std::string const& logfilepathname_);
	};

	class app:
		public netp::singleton<app>
	{

	private:

		NRP<io_event_loop_group> m_io_evt_group;
		NRP<dns_resolver> m_dns_resolver;
		NRP<logger_broker> m_logger_broker;

		bool m_should_exit;
		netp::mutex m_mutex;
		netp::condition m_cond;
		std::vector<std::tuple<int,i64_t>> m_signo_tuple_vec;
		app_cfg m_cfg;

		fn_app_hook_t m_app_startup_prev;
		fn_app_hook_t m_app_startup_post;
		fn_app_hook_t m_app_exit_prev;
		fn_app_hook_t m_app_exit_post;

		fn_app_hook_t m_app_event_loop_init_prev;
		fn_app_hook_t m_app_event_loop_init_post;
		fn_app_hook_t m_app_event_loop_deinit_prev;
		fn_app_hook_t m_app_event_loop_deinit_post;

		void app_thread_init();
		void app_thread_deinit();

	public:
		//@warn: if we do need to create app on heap, we should always use new/delete, or std::shared_ptr
		app();
		~app();

		int startup(app_cfg const& cfg);

		void _startup();
		void _exit();

		void _init();
		void _deinit();

		void __log_init();
		void __log_deinit();

#ifndef NETP_DISABLE_INSTRUCTION_SET_DETECTOR
		void dump_arch_info();
#endif

#ifdef NETP_DEBUG_OBJECT_SIZE
		void __dump_sizeof();
#endif

		void __signal_init();
		void __signal_deinit();

		void __net_init();
		void __net_deinit();

		void ___event_loop_init();
		void ___event_loop_wait();

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
		NRP<netp::io_event_loop_group> const& loop_group() const { return m_io_evt_group; }
		
		__NETP_FORCE_INLINE
		NRP<netp::dns_resolver> const& dns() const { return m_dns_resolver; }

		__NETP_FORCE_INLINE
		NRP<netp::logger_broker> const& logger() const { return m_logger_broker; }
	};


#define NETP_LOGGER_BROKER	( netp::app::instance()->logger() )

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