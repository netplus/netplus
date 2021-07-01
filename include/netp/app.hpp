#ifndef _NETP_APP_HPP_
#define _NETP_APP_HPP_

#include <netp/core.hpp>
#include <netp/signal_broker.hpp>
#include <netp/test.hpp>

//#define NETP_DEBUG_OBJECT_SIZE

namespace netp {

	typedef std::function<void()> fn_app_hook_t;

	struct app_cfg {

		std::string logfilepathname;
		std::vector<std::string> dnsnses; //dotip
		int poller_max[T_POLLER_MAX];
		int poller_count[T_POLLER_MAX];
		event_loop_cfg event_loop_cfgs[T_POLLER_MAX];

		fn_app_hook_t app_startup_prev;
		fn_app_hook_t app_startup_post;
		fn_app_hook_t app_exit_prev;
		fn_app_hook_t app_exit_post;

		fn_app_hook_t app_event_loop_init_prev;
		fn_app_hook_t app_event_loop_init_post;
		fn_app_hook_t app_event_loop_deinit_prev;
		fn_app_hook_t app_event_loop_deinit_post;

		void __init_from_cfg_json(const char* jsonfile);
		void __parse_cfg(int argc, char** argv);

		void __cfg_default_loop_cfg() {
			const int corecount = std::thread::hardware_concurrency();
			for (size_t i = 0; i < T_POLLER_MAX; ++i) {
				poller_max[i] = (corecount << 1); //0 means default
				if (i == NETP_DEFAULT_POLLER_TYPE) {
					cfg_poller_count(io_poller_type(i), int(std::ceil(corecount) * 1.5f));
				} else {
					cfg_poller_count(io_poller_type(i), 0);
				}
				event_loop_cfgs[i].ch_buf_size = (128 * 1024);
			}
		}
	public:
		app_cfg(int argc, char** argv) :
			logfilepathname(),
			dnsnses(std::vector<std::string>()),
			app_startup_prev(nullptr),
			app_startup_post(nullptr),
			app_exit_prev(nullptr),
			app_exit_post(nullptr),
			app_event_loop_init_prev(nullptr),
			app_event_loop_init_post(nullptr),
			app_event_loop_deinit_prev(nullptr),
			app_event_loop_deinit_post(nullptr)
		{
			NETP_ASSERT( argc>=1 );
			__cfg_default_loop_cfg();
			std::string data=netp::curr_local_data_str();
			std::string data_;
			netp::replace(data, std::string("-"), std::string("_"), data_);
			cfg_log_filepathname(std::string(argv[0]) + std::string(".") + data_ + ".log");
			__parse_cfg(argc, argv);
		}

		app_cfg() :
			logfilepathname(),
			dnsnses(std::vector<std::string>()),
			app_startup_prev(nullptr),
			app_startup_post(nullptr),
			app_exit_prev(nullptr),
			app_exit_post(nullptr),
			app_event_loop_init_prev(nullptr),
			app_event_loop_init_post(nullptr),
			app_event_loop_deinit_prev(nullptr),
			app_event_loop_deinit_post(nullptr)
		{
			__cfg_default_loop_cfg();
			std::string data = netp::curr_local_data_str();
			std::string data_;
			netp::replace(data, std::string("-"), std::string("_"), data_);
			cfg_log_filepathname("netp_" + data_ + ".log");
		}

		void cfg_poller_max(io_poller_type t, int c) {
			if (c > 0) {
				const int max = (std::thread::hardware_concurrency() << 1);
				if (c > max) {
					c = max;
				}
				else if (c < 1) {
					c = 1;
				}
				poller_max[t] = c;
			}
		}

		void cfg_poller_count(io_poller_type t, int c) {
			const int corecount = std::thread::hardware_concurrency();
			if (c > poller_max[t]) {
				c = poller_max[t];
			} else if (c <= 0) {
				c = 0;

				if (t == NETP_DEFAULT_POLLER_TYPE) {
					c = 1;
				}
			}
			poller_count[t] = c ;
		}

		void cfg_channel_buf(io_poller_type t, int buf_in_kbytes) {
			if (buf_in_kbytes > 0) {
				event_loop_cfgs[t].ch_buf_size = buf_in_kbytes * (1024);
			}
		}

		void cfg_add_dns(std::string const& dns_ns) {
			dnsnses.push_back(dns_ns);
		}

		void cfg_log_filepathname(std::string const& logfilepathname_) {
			if (logfilepathname_.length() == 0) { return; }
			logfilepathname = logfilepathname_;
		}
	};

	class app_thread_init :
		public netp::singleton<app_thread_init>
	{
	public:
		app_thread_init();
		~app_thread_init();
	};

	class app {

	private:
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

	public:
		//@warn: if we do need to create app on heap, we should always use new/delete, or std::shared_ptr
		app(app_cfg const& cfg);
		app();

		~app();

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

		int run() {
			netp::unique_lock<netp::mutex> ulk(m_mutex);
			while (!m_should_exit) {
				m_cond.wait(ulk);
			}
			return 0;
		}

		template <class _Rep, class _Period>
		int run_for(std::chrono::duration<_Rep, _Period> const& duration)
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
		int run_until(std::chrono::time_point<_Clock, _Duration> const& tp)
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
	};


	class app_test_unit :
		public netp::test_unit
	{
	public:
		bool run();

		void test_memory();
		void test_packet();
		void test_netp_allocator(size_t loop);
		void test_std_allocator(size_t loop);

		void test_generic_check();

		void benchmark_hash();
	};
}
#endif