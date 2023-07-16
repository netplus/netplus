#include <fstream>

#include "../3rd/nlohmann/include/nlohmann/json.hpp"

#include <signal.h>

#include <netp/CPUID.hpp>
#include <netp/helper.hpp>
#include <netp/socket.hpp>
#include <netp/logger/console_logger.hpp>
#include <netp/logger/file_logger.hpp>
#include <netp/os/api_wrapper.hpp>
#include <netp/signal_broker.hpp>

#include <netp/security/crc.hpp>
#include <netp/security/fletcher.hpp>

#include <netp/benchmark.hpp>

#ifdef _NETP_WIN
	#include <netp/os/winsock_helper.hpp>
#endif

#include <netp/app.hpp>
#include <netp/memory_unit_test.hpp>

namespace netp {
	NRP<netp::signal_broker> g_sigbroker;

	static void signal_broker_init() {
		NETP_ASSERT(g_sigbroker == nullptr);
		g_sigbroker = netp::make_ref<netp::signal_broker>();
	}

	static void signal_broker_deinit() {
		NETP_ASSERT(g_sigbroker != nullptr);
		g_sigbroker = nullptr;
	}

	static void signal_void(int) {}

	static void signal_from_system(int signo) {
		NETP_ASSERT(g_sigbroker != nullptr);
		g_sigbroker->signal_raise(signo);
	}

	i64_t signal_register(int signo, netp::fn_signal_handler_t&& H) {
		if (H == nullptr) {
			//NETP_WARN("[signal_broker]register_signal, ignore: %d", signo);
			::signal(signo, SIG_IGN);
			return -1;
		}

		::signal(signo, &signal_from_system);
		return g_sigbroker->bind(signo, std::forward<netp::fn_signal_handler_t>(H));
	}

	void signal_unregister(int signo, i64_t handle_id) {
		::signal(signo, &signal_void);
		g_sigbroker->unbind_by_handle_id(handle_id);
	}

	void app::cfg_loop_count( u32_t c) {
		u32_t hcore = std::thread::hardware_concurrency();
		if (c == 0) {
			m_loop_count = hcore;
		} else if (u32_t(c) > (hcore<<1) ) {
			m_loop_count = (hcore << 1);
		} else {
			m_loop_count = c;
		}
	}

	void app::cfg_channel_read_buf(u32_t buf_in_kbytes) {
		if (buf_in_kbytes == 0) {
			buf_in_kbytes = 128;
		} else if (buf_in_kbytes > 256) {
			buf_in_kbytes = 256;
		} else if (buf_in_kbytes<8) {
			buf_in_kbytes = 8;
		}
		m_channel_read_buf_size = buf_in_kbytes * (1024);
	}

	void app::cfg_channel_tx_limit_clock(u32_t clock) {
		if (clock < 1) {
			clock = 1;
		} else if (clock > 200) {
			clock = 200;
		}
		m_channel_tx_limit_clock = clock;
	}

	void app::cfg_add_dns(std::string const& dns_ns) {
		m_dns_hosts.push_back(dns_ns);
	}

	void app::cfg_log_filepathname(std::string const& logfilepathname_) {
		if (logfilepathname_.length() == 0) { return; }
		m_logfilepathname = logfilepathname_;
	}

	void app::dns_hosts(std::vector<netp::string_t, netp::allocator<netp::string_t>>& dns_hosts) const {
		for (auto dns : m_dns_hosts) {
			dns_hosts.push_back( netp::string_t(dns.c_str()));
		}
	}

	void app::_app_thread_init() 
	{
		static_assert(sizeof(netp::i8_t) == 1, "assert sizeof(i8_t) failed");
		static_assert(sizeof(netp::u8_t) == 1, "assert sizeof(u8_t) failed");
		static_assert(sizeof(netp::i16_t) == 2, "assert sizeof(i16_t) failed");
		static_assert(sizeof(netp::u16_t) == 2, "assert sizeof(u16_t) failed");
		static_assert(sizeof(netp::i32_t) == 4, "assert sizeof(i32_t) failed");
		static_assert(sizeof(netp::u32_t) == 4, "assert sizeof(u32_t) failed");
		static_assert(sizeof(netp::i64_t) == 8, "assert sizeof(i64_t) failed");
		static_assert(sizeof(netp::u64_t) == 8, "assert sizeof(u64_t) failed");

#if __NETP_IS_BIG_ENDIAN
		NETP_ASSERT(netp::is_big_endian());
#else
		NETP_ASSERT(netp::is_little_endian());
#endif

#ifdef _NETP_WIN
		//CPU ia-64 is big endian, but windows run on a little endian mode on ia-64
		static_assert(!(__NETP_IS_BIG_ENDIAN), "windows always run on little endian");
#endif

		netp::random_init_seed();
		netp::tls_create<netp::impl::thread_data>();

#ifdef NETP_MEMORY_USE_ALLOCATOR_WITH_TLS_BLCOK_POOL
		cfg_memory_init_allocator_with_block_pool_manager();
		netp::tls_set<netp::allocator_with_block_pool>(cfg_memory_create_allocator_with_block_pool());
#endif

#if defined(_DEBUG_MUTEX) || defined(_DEBUG_SHARED_MUTEX)
		//for mutex/lock deubg
		netp::tls_create<netp::mutex_set_t>();
#endif
	}

	void app::_app_thread_deinit()
	{
#if defined(_DEBUG_MUTEX) || defined(_DEBUG_SHARED_MUTEX)
		tls_destroy<mutex_set_t>();
#endif
		netp::tls_destroy<netp::impl::thread_data>();

#ifdef NETP_MEMORY_USE_ALLOCATOR_WITH_TLS_BLCOK_POOL
		cfg_memory_destory_allocator_with_block_pool(netp::tls_get<netp::allocator_with_block_pool>());
		netp::tls_set<netp::allocator_with_block_pool>(nullptr);
		cfg_memory_deinit_allocator_with_block_pool_manager();
#endif
	}

	int app::_init_from_cfg_json(const char* jsonfile) {
		std::ifstream ifs(jsonfile, std::ifstream::in);
		if (!ifs.good()) {
			return -1;
		}
		nlohmann::json cfg_json = nlohmann::json::parse(ifs, nullptr, false);
		ifs.close();

		if (cfg_json.is_discarded()) {
			return -1;
		}
		if (m_is_cfg_json_loaded) { 
			return -1;
		}
		m_is_cfg_json_loaded = true;

		if (cfg_json.find("netp_memory_pool_size_level") != cfg_json.end() && cfg_json["netp_memory_pool_size_level"].is_number()) {
			cfg_memory_pool_size_level(cfg_json["netp_memory_pool_size_level"].get<int>());
		}

		if (cfg_json.find("netp_log") != cfg_json.end() && cfg_json["netp_log"].is_string()) {
			cfg_log_filepathname(cfg_json["netp_log"].get<std::string>());
		}

		if (cfg_json.find("netp_def_loop_count_by_factor") != cfg_json.end() && cfg_json["netp_def_loop_count_by_factor"].is_number_float() && cfg_json["def_loop_count_factor"].get<float>() > 0 ) {
			cfg_loop_count(u32_t(cfg_json["netp_def_loop_count_by_factor"].get<float>()*std::thread::hardware_concurrency()));
		} 
		if (cfg_json.find("netp_def_loop_count") != cfg_json.end() && cfg_json["netp_def_loop_count"].is_number()) {
			cfg_loop_count(cfg_json["netp_def_loop_count"]);
		}

		if (cfg_json.find("netp_channel_read_buf") != cfg_json.end() && cfg_json["netp_channel_read_buf"].is_number()) {
			cfg_channel_read_buf(cfg_json["netp_channel_read_buf"].get<int>());
		}
		if (cfg_json.find("netp_channel_tx_limit_clock") != cfg_json.end() && cfg_json["netp_channel_tx_limit_clock"].is_number()) {
			cfg_channel_tx_limit_clock(cfg_json["netp_channel_tx_limit_clock"].get<int>());
		}

		return netp::OK;
	}

	int app::__parse_cfg(parse_cfg_mode mode, int argc, char** argv, std::string const& param, std::string& value) {
		if (!(argc > 1)) { return -1; }

		struct ::option long_options[] = {
			{"netp-cfg", optional_argument, 0, 1},
			{"netp-log", optional_argument, 0, 2 },
			{"netp-memory-pool-size-level", optional_argument, 0, 3 },
			{"netp-def-loop-count-by-factor", optional_argument, 0, 4 },
			{"netp-def-loop-count", optional_argument, 0, 5 },
			{"netp-channel-read-buf", optional_argument, 0, 6 },
			{"netp-channel-bdlimit-clock", optional_argument, 0, 7 },
			{0,0,0,0}
		};

		int mode_fetch_val = -1;
		if (mode == mode_fetch) {
			if (param.length() == 0) {
				return -1;
			}
			for (int i = 0; i < (sizeof(long_options) / sizeof(long_options[0])); ++i) {
				if (long_options[i].name == 0) { continue; }
				if (netp::strcmp(param.c_str(), long_options[i].name) == 0) {
					mode_fetch_val = long_options[i].val;
					break;
				}
			}
			if (mode_fetch_val == -1) {
				return -1;
			}
		}

		int rt = mode == mode_fetch ? -1 : netp::OK;
		::optind = 1;
		const char* optstring = "H:h::";
		int opt;
		int opt_idx;
		while ((opt = getopt_long(argc, argv, optstring, long_options, &opt_idx)) != -1) {

			if (mode == mode_fetch) {
				if (opt != mode_fetch_val) {
					continue;
				}
				value = std::string(optarg);
				return netp::OK;
			}

			switch (opt) {
			case 1:
			{
				_init_from_cfg_json(optarg);
			}
			break;
			case 2:
			{
				cfg_log_filepathname(std::string(optarg));
			}
			break;
			case 3:
			{
				cfg_memory_pool_size_level(std::atoi(optarg));
			}
			break;
			case 4:
			{
				cfg_loop_count(std::atoi(optarg) * std::thread::hardware_concurrency());
			}
			break;
			case 5:
			{
				cfg_loop_count(std::atoi(optarg));
			}
			break;
			case 6:
			{
				cfg_channel_read_buf(std::atoi(optarg));
			}
			break;
			case 7:
			{
				cfg_channel_tx_limit_clock(std::atoi(optarg));
			}
			break;
			}
		}

		std::atomic_thread_fence(std::memory_order_release);
		return rt;
	}

	int app::_parse_cfg_fetch(int argc, char** argv, std::string const& param, std::string& value) {
		if (param.length() == 0) {
			return -1;
		}
		return __parse_cfg(mode_fetch, argc, argv, param, value);
	}

	void app::_parse_cfg(int argc, char** argv) {
		std::string empty_string;
		std::string bfr_cfg_json = std::string("netp-cfg");
		std::string bfr_cfg_json_value = std::string();
		int rt = _parse_cfg_fetch(argc, argv, bfr_cfg_json, bfr_cfg_json_value);
		if (rt == -1 || bfr_cfg_json_value.length() == 0 || (_init_from_cfg_json(bfr_cfg_json_value.c_str()) == -1)) {
			_init_from_cfg_json( netp::to_absolute_path("./netp.cfg.json", current_directory()).c_str() );
		}
		__parse_cfg(mode_do, argc, argv, empty_string, empty_string);
	}

	app::app() :
		m_loop_count(u32_t(std::thread::hardware_concurrency())),
		m_channel_read_buf_size(128*1024),
		m_channel_tx_limit_clock(30),/*resolution on windows is 15ms*/
		m_is_cfg_json_loaded(false),
		m_should_exit(false), 
		m_app_state(app_state::s_idle),
		m_logfilepathname()
	{
	}

	app::~app()
	{
		std::atomic_thread_fence(std::memory_order_acquire);
		//double check
		_event_loop_deinit();

		_deinit();
		_app_thread_deinit();
	}

	std::string app::app_name() const {
		return m_app_name;
	}

	std::string app::app_path() const {
		return m_app_path;
	}

	void app::init( int argc, char** argv ) {
		app_state __s_idle = app_state::s_idle;
		if (!m_app_state.compare_exchange_strong(__s_idle, app_state::s_init_begin, std::memory_order_acq_rel, std::memory_order_acquire)) {
			return;
		}

		m_app_name = netp::filename(std::string(*argv));
		m_app_path = netp::absolute_parent_path(std::string(*argv)) ;
		_parse_cfg(argc, argv);

		if (m_logfilepathname.length() == 0) {
			std::string data = netp::curr_local_data_str();
			std::string data_;
			netp::replace(data, std::string("-"), std::string("_"), data_);
			cfg_log_filepathname(std::string("./") + m_app_name + "." + data_ + ".log");
		}
		m_logfilepathname = netp::to_absolute_path(m_logfilepathname);

		_init();

		NRP<logger::file_logger> filelogger = netp::make_ref<netp::logger::file_logger>(netp::string_t(m_logfilepathname.c_str()));
		filelogger->set_mask_by_level(NETP_FILE_LOGGER_LEVEL);
		netp::logger_broker::instance()->add(filelogger);

#ifdef NETP_DEBUG_ARCH_INFO
		_dump_arch_info();
#endif

#ifdef NETP_DEBUG_OBJECT_SIZE
		_dump_sizeof();
#endif
		
		NRP<app_test_unit> apptest = netp::make_ref<app_test_unit>();
		if (!apptest->run()) {
			NETP_INFO("apptest failed");
			exit(-2);
			return;
		}

		app_state __s_init_begin = app_state::s_init_begin;
		if (!m_app_state.compare_exchange_strong(__s_init_begin, app_state::s_init_done, std::memory_order_acq_rel, std::memory_order_acquire)) {
			return;
		}
	}

	void app::start_loop() {
		_event_loop_init();
	}

	void app::stop_loop() {
		_event_loop_deinit();
	}

	void app::_init() {
		_app_thread_init();
		_log_init();
		_signal_init();
		_net_init();
		std::atomic_thread_fence(std::memory_order_release);
	}

	void app::_deinit() {
		_net_deinit();
		_signal_deinit();
		_log_deinit();
	}

	void app::_log_init() {
		//NETP_ASSERT( m_logger_broker == nullptr ) ;
		netp::logger_broker::instance()->init();

#ifdef NETP_ENABLE_CONSOLE_LOGGER
		NRP<logger::console_logger> clg = netp::make_ref <logger::console_logger>();
		clg->set_mask_by_level(NETP_CONSOLE_LOGGER_LEVEL);
		netp::logger_broker::instance()->add(clg);
#endif

#ifdef NETP_ENABLE_ANDROID_LOG
		NRP<android_log_print> alp = netp::make_ref<android_log_print>();
		alp->set_mask_by_level(NETP_CONSOLE_LOGGER_LEVEL);
		__android_log_print(ANDROID_LOG_INFO, "[NETP]", "add android_log_print done");
		netp::logger_broker::instance()->add(alp);
#endif
	}

	void app::_log_deinit() {
		netp::logger_broker::instance()->destroy_instance();
	}

#ifdef NETP_DEBUG_ARCH_INFO

	void app::_dump_arch_info() {
		const char bob[2] = { 0x01, 0x02 };
		const u16_t bob_u16 = *((u16_t*)(&bob[0]));
#if __NETP_IS_BIG_ENDIAN
		std::string arch_info = "endian: big_endian\n" ;
		NETP_ASSERT(bob_u16 == ((0x01<<8)|0x02));
#else
		std::string arch_info = "endian: little_endian\n";
		NETP_ASSERT(bob_u16 == ((0x02<<8)|0x01));
#endif

		arch_info += "vender: " + netp::CPUID::Vendor() + "\n";
		arch_info += "brand: " + netp::CPUID::Brand() + "\n";
		arch_info += "instructions:";
		auto support_message = [](std::string& archinfo, const char* isa_feature, bool is_supported) {
			if (is_supported) {
				archinfo += std::string(" ") + isa_feature ;
			}
		};

		support_message(arch_info, "3DNOW", netp::CPUID::_3DNOW());
		support_message(arch_info, "3DNOWEXT", netp::CPUID::_3DNOWEXT());
		support_message(arch_info, "ABM", netp::CPUID::ABM());
		support_message(arch_info, "ADX", netp::CPUID::ADX());
		support_message(arch_info, "AES", netp::CPUID::AES());
		support_message(arch_info, "AVX", netp::CPUID::AVX());
		support_message(arch_info, "AVX2", netp::CPUID::AVX2());
		support_message(arch_info, "AVX512CD", netp::CPUID::AVX512CD());
		support_message(arch_info, "AVX512ER", netp::CPUID::AVX512ER());
		support_message(arch_info, "AVX512F", netp::CPUID::AVX512F());
		support_message(arch_info, "AVX512PF", netp::CPUID::AVX512PF());
		support_message(arch_info, "BMI1", netp::CPUID::BMI1());
		support_message(arch_info, "BMI2", netp::CPUID::BMI2());
		support_message(arch_info, "CLFSH", netp::CPUID::CLFSH());
		support_message(arch_info, "CMPXCHG16B", netp::CPUID::CMPXCHG16B());
		support_message(arch_info, "CX8", netp::CPUID::CX8());
		support_message(arch_info, "ERMS", netp::CPUID::ERMS());
		support_message(arch_info, "F16C", netp::CPUID::F16C());
		support_message(arch_info, "FMA", netp::CPUID::FMA());
		support_message(arch_info, "FSGSBASE", netp::CPUID::FSGSBASE());
		support_message(arch_info, "FXSR", netp::CPUID::FXSR());
		support_message(arch_info, "HLE", netp::CPUID::HLE());
		support_message(arch_info, "INVPCID", netp::CPUID::INVPCID());
		support_message(arch_info, "LAHF", netp::CPUID::LAHF());
		support_message(arch_info, "LZCNT", netp::CPUID::LZCNT());
		support_message(arch_info, "MMX", netp::CPUID::MMX());
		support_message(arch_info, "MMXEXT", netp::CPUID::MMXEXT());
		support_message(arch_info, "MONITOR", netp::CPUID::MONITOR());
		support_message(arch_info, "MOVBE", netp::CPUID::MOVBE());
		support_message(arch_info, "MSR", netp::CPUID::MSR());
		support_message(arch_info, "OSXSAVE", netp::CPUID::OSXSAVE());
		support_message(arch_info, "PCLMULQDQ", netp::CPUID::PCLMULQDQ());
		support_message(arch_info, "POPCNT", netp::CPUID::POPCNT());
		support_message(arch_info, "PREFETCHWT1", netp::CPUID::PREFETCHWT1());
		support_message(arch_info, "RDRAND", netp::CPUID::RDRAND());
		support_message(arch_info, "RDSEED", netp::CPUID::RDSEED());
		support_message(arch_info, "RDTSCP", netp::CPUID::RDTSCP());
		support_message(arch_info, "RTM", netp::CPUID::RTM());
		support_message(arch_info, "SEP", netp::CPUID::SEP());
		support_message(arch_info, "SHA", netp::CPUID::SHA());
		support_message(arch_info, "SSE", netp::CPUID::SSE());
		support_message(arch_info, "SSE2", netp::CPUID::SSE2());
		support_message(arch_info, "SSE3", netp::CPUID::SSE3());
		support_message(arch_info, "SSSE3", netp::CPUID::SSSE3());
		support_message(arch_info, "SSE4.1", netp::CPUID::SSE41());
		support_message(arch_info, "SSE4.2", netp::CPUID::SSE42());
		support_message(arch_info, "SSE4a", netp::CPUID::SSE4a());
		support_message(arch_info, "SYSCALL", netp::CPUID::SYSCALL());
		support_message(arch_info, "TBM", netp::CPUID::TBM());
		support_message(arch_info, "XOP", netp::CPUID::XOP());
		support_message(arch_info, "XSAVE", netp::CPUID::XSAVE());

		support_message(arch_info, "NEON", netp::CPUID::NEON());

		NETP_TRACE_APP("ARCH: %s\nalignof(std::max_align_t): %d\ncore: %u\n", arch_info.c_str(), alignof(std::max_align_t), std::thread::hardware_concurrency() );
	}

#endif

#if defined(NETP_DEBUG_OBJECT_SIZE)
	void app::_dump_sizeof() {

		NETP_TRACE_APP("sizeof(void*): %u", sizeof(void*));
		NETP_TRACE_APP("sizeof(std::atomic<u8_t>): %u", sizeof(std::atomic<u8_t>));
		NETP_TRACE_APP("sizeof(std::atomic<bool>): %u", sizeof(std::atomic<bool>));
		NETP_TRACE_APP("sizeof(std::atomic<long>): %u", sizeof(std::atomic<long>));
		NETP_TRACE_APP("sizeof(std::forward_list<long>): %u", sizeof(std::forward_list<long>));
		NETP_TRACE_APP("sizeof(std::forward_list<long>::const_iterator): %u", sizeof(std::forward_list<long>::const_iterator));
		NETP_TRACE_APP("sizeof(std::forward_list<long>::iterator): %u", sizeof(std::forward_list<long>::iterator));

		NETP_TRACE_APP("sizeof(netp::__atomic_counter): %u", sizeof(netp::__atomic_counter));
		NETP_TRACE_APP("sizeof(netp::__non_atomic_counter): %u", sizeof(netp::__non_atomic_counter));
		NETP_TRACE_APP("sizeof(netp::ref_base): %u", sizeof(netp::ref_base));
		NETP_TRACE_APP("sizeof(netp::non_atomic_ref_base): %u", sizeof(netp::non_atomic_ref_base));
		NETP_TRACE_APP("sizeof(ref_ptr<netp::packet>): %u", sizeof(NRP<netp::packet>));
		NETP_TRACE_APP("sizeof(netp::packet): %u", sizeof(netp::packet));
		NETP_TRACE_APP("sizeof(netp::non_atomic_ref_packet_u16len): %u", sizeof(netp::non_atomic_ref_u16packet));

		NETP_TRACE_APP("sizeof(address): %u", sizeof(netp::address));

		NETP_TRACE_APP("sizeof(netp::channel): %u", sizeof(netp::channel));
		NETP_TRACE_APP("sizeof(netp::io_ctx): %u", sizeof(netp::io_ctx));
		NETP_TRACE_APP("sizeof(netp::fn_io_event_t): %u", sizeof(netp::fn_io_event_t));
		NETP_TRACE_APP("sizeof(std::function<void(int)>): %u", sizeof(std::function<void(int)>));
		NETP_TRACE_APP("sizeof(std::function<void(int, int)>): %u", sizeof(std::function<void(int, int)>));
		NETP_TRACE_APP("sizeof(std::deque<socket_outbound_entry, netp::allocator<socket_outbound_entry>>): %u", sizeof(std::deque<socket_outbound_entry, netp::allocator<socket_outbound_entry>>));
		NETP_TRACE_APP("sizeof(std::deque<socket_outbound_entry_to, netp::allocator<socket_outbound_entry_to>>): %u", sizeof(std::deque<socket_outbound_entry_to, netp::allocator<socket_outbound_entry_to>>));
		NETP_TRACE_APP("sizeof(netp::socket_channel): %u", sizeof(netp::socket_channel));
		NETP_TRACE_APP("sizeof(std::vector<int>): %u", sizeof(std::vector<int>));
		NETP_TRACE_APP("sizeof(std::vector<std::function<void(int)>): %u", sizeof(std::vector<std::function<void(int)>>));
		NETP_TRACE_APP("sizeof(std::vector<std::function<void(int, int)>>): %u", sizeof(std::vector<std::function<void(int, int)>>));
		NETP_TRACE_APP("sizeof(std::vector<NRP<netp::packet>, netp::allocator<NRP<netp::packet>>>): %u", sizeof(std::vector<NRP<netp::packet>, netp::allocator<NRP<netp::packet>>>));
		NETP_TRACE_APP("sizeof(netp::promise<int>): %u", sizeof(netp::promise<int>));
		NETP_TRACE_APP("sizeof(netp::promise<tuple<int,NRP<socket>>>): %u", sizeof(netp::promise<std::tuple<int, NRP<netp::socket_channel>>>));
		NETP_TRACE_APP("sizeof(netp::event_broker_promise): %u", sizeof(netp::event_broker_promise<int>));
		NETP_TRACE_APP("sizeof(netp::spin_mutex): %u", sizeof(netp::spin_mutex));
		NETP_TRACE_APP("sizeof(netp::mutex): %u", sizeof(netp::mutex));
		NETP_TRACE_APP("sizeof(netp::condition): %u", sizeof(netp::condition));
		NETP_TRACE_APP("sizeof(std::condition_variable): %u", sizeof(std::condition_variable));
		NETP_TRACE_APP("sizeof(netp::condition_any): %u", sizeof(netp::condition_any));
		NETP_TRACE_APP("sizeof(std::condition_variable_any): %u", sizeof(std::condition_variable_any));
	}
#endif

	void app::_signal_init() {
		netp::unique_lock<netp::mutex> ulk(m_mutex);
		signal_broker_init();

#if  defined(_NETP_GNU_LINUX) || defined(_NETP_ANDROID) || defined(_NETP_APPLE)
		signal_register(SIGPIPE, nullptr);
#endif

		i64_t handle_id_int =signal_register(SIGINT, std::bind(&app::handle_signal, this, std::placeholders::_1));
		if (handle_id_int > 0) {
			m_signo_tuple_vec.push_back(std::make_tuple(SIGINT, handle_id_int));
		}

		i64_t handle_id_term = signal_register(SIGTERM, std::bind(&app::handle_signal, this, std::placeholders::_1));
		if (handle_id_term > 0) {
			m_signo_tuple_vec.push_back(std::make_tuple(SIGTERM, handle_id_term));
		}
	}

	void app::_signal_deinit() {
		netp::unique_lock<netp::mutex> ulk(m_mutex);
		while (m_signo_tuple_vec.size()) {
			std::tuple<int, i64_t>& pair = m_signo_tuple_vec.back();
			signal_unregister(std::get<0>(pair), std::get<1>(pair));
			m_signo_tuple_vec.pop_back();
		}
		signal_broker_deinit();
		NETP_TRACE_APP("deinit signal end");
	}

	void app::_net_init() {
		NETP_TRACE_APP("net init begin");
#ifdef _NETP_WIN
		netp::os::winsock_init();
#endif

		NETP_ASSERT(m_def_loop_group == nullptr);
		event_loop_cfg cfg(NETP_DEFAULT_POLLER_TYPE, u8_t(f_enable_dns_resolver), m_channel_read_buf_size);
		dns_hosts(cfg.dns_hosts);
		m_def_loop_group = netp::make_ref<netp::event_loop_group>(cfg, default_event_loop_maker);
		NETP_TRACE_APP("net init end");
	}

	void app::_net_deinit() {
		NETP_TRACE_APP("net deinit begin");
#ifdef _NETP_WIN
		netp::os::winsock_deinit();
#endif

		m_def_loop_group = nullptr;
		NETP_TRACE_APP("net deinit end");
	}

	void app::_event_loop_init() {
		app_state __s_init_done = app_state::s_init_done;
		if (!m_app_state.compare_exchange_strong(__s_init_done, app_state::s_loop_run, std::memory_order_acq_rel, std::memory_order_acquire)) {
			NETP_TRACE_APP("[app]init loop failed");
			return;
		}
		NETP_ASSERT( m_def_loop_group != nullptr );
		m_def_loop_group->start(m_loop_count);
		NETP_TRACE_APP("[app]init loop done");
	}

	//remote end do not SEND FIN immedialy even we've send FIN to it, so interrupt is a necessary if we want to exit app quickly
	void app::terminate_loop() {
		app_state __s_loop_run = app_state::s_loop_run;
		if (m_app_state.compare_exchange_strong(__s_loop_run, app_state::s_loop_exit_notified, std::memory_order_acq_rel, std::memory_order_acquire)) {
			m_def_loop_group->notify_terminating();
		}
	}

	void app::wait_loop() {
		app_state __s_loop_exit_notified = app_state::s_loop_exit_notified;
		if (m_app_state.compare_exchange_strong(__s_loop_exit_notified, app_state::s_loop_wait_done, std::memory_order_acq_rel, std::memory_order_acquire)) {
			m_def_loop_group->wait();
		}
	}

	void app::_event_loop_deinit() {
		terminate_loop();
		wait_loop();
		//reset loop after all loop reference dattached from business code
	}

	//ISSUE: if the waken thread is main thread, we would get stuck here
	void app::handle_signal(int signo) {
		netp::unique_lock<netp::mutex> ulk(m_mutex);
		//NETP_ASSERT(false);
		NETP_TRACE_APP("[APP]receive signo: %d", signo);
		m_cond.notify_one();

#if defined(_NETP_GNU_LINUX) || defined(_NETP_ANDROID) || defined(_NETP_APPLE)
		if (signo == SIGPIPE) {
			return;//IGN
		}
#endif

		switch (signo) {
		case SIGINT:
		case SIGTERM:
		{
			m_should_exit = true;
		}
		break;
		}
	}

	bool app::should_exit() const {
		return m_should_exit;
	}

	static
	void test_v4_mask_by_cidr() {
		netp::ipv4_t iplist[33];
		int j = 0;
		netp::ipv4_t v4;
		v4.u32 = 0;
		iplist[j++] = v4;
		for (int i = 1; i < 32; ++i) {
			v4.u32 = (0xfffffffful - ((1ul << (32 - i)) - 1ul));
			iplist[j++] = v4;
		}
		v4.u32 = 0xfffffffful;
		iplist[j++] = v4;

		netp::ipv4_t v4mask;
		v4mask.bits = { 255,255,255,255 };

		for (netp::u8_t i = 0; i < (sizeof(iplist) / sizeof(iplist[0])); ++i) {
			NETP_ASSERT(i <= 32);
			NETP_ASSERT(netp::v4_mask_by_cidr(&v4mask, i) == (iplist[i]));
		}
	}

	static 
	void test_v6_mask_by_cidr()
	{
		netp::ipv6_t iplist[129];
		int j = 0;

		netp::ipv6_t v6;
		v6.u64.A = 0;
		v6.u64.B = 0;
		iplist[j++] = v6;

		for (int i = 1; i < 64; ++i)
		{
			v6.u64.A = 0xffffffffffffffffull - ((1ull << (64 - i)) - 1ull);
			v6.u64.B = 0;
			iplist[j++] = v6;
		}

		v6.u64.A = 0xffffffffffffffffull;
		v6.u64.B = 0;
		iplist[j++] = v6;

		for (int i = 65; i < 128; ++i) {
			v6.u64.A = 0xffffffffffffffffull;
			v6.u64.B = 0xffffffffffffffffull - ((1ull << ((128 - (i)))) - 1ull);
			iplist[j++] = v6;
		}

		v6.u64.A = 0xffffffffffffffffull;
		v6.u64.B = 0xffffffffffffffffull;
		iplist[j++] = v6;

		netp::ipv6_t v6mask;
		v6mask.bits = { 0xffff,0xffff,0xffff,0xffff, 0xffff,0xffff,0xffff,0xffff };

		for (netp::u8_t i = 0; i < (sizeof(iplist) / sizeof(iplist[0])); ++i) {
			NETP_ASSERT(i <= 128);
			NETP_ASSERT(netp::v6_mask_by_cidr(&v6mask, i) == (iplist[i]));
		}
	}

	void app_test_unit::test_generic_check() {
		const char* loopback[] = {
			"127.0.0.1",
			"10.0.0.0",
			"172.16.0.1",
			"192.168.0.0"
		};

		for (size_t i = 0; i < sizeof(loopback) / sizeof(loopback[0]); ++i) {
			ipv4_t v4 = netp::dotiptonip(loopback[i]);
			NETP_ASSERT(is_internal(v4));
		}

		test_v4_mask_by_cidr();
		test_v6_mask_by_cidr();
	}

	void app_test_unit::benchmark_hash() {
		int total = 100000;
		NRP<netp::packet> p = netp::make_ref<netp::packet>();
		for (int j = 0; j < 1500; ++j) {
			p->write<byte_t>(j%10);
		}
		{
			netp::benchmark mk("f16");
			for (int i = 0; i < total; ++i) {
				volatile u16_t h1 = netp::security::fletcher16((const uint8_t*)p->head(), p->len());
				(void)h1;
			}
		}
		{
			netp::benchmark mk("crc16");
			for (int i = 0; i < total; ++i) {
				volatile u16_t h2 = netp::security::crc16(p->head(), p->len());
				(void)h2;
			}
		}

		{
			NETP_INFO("crc16: %u", netp::security::crc16(p->head(), p->len()));
		}
	}

	bool app_test_unit::run() {
		netp::run_test<memory_test_unit>();

#ifdef _NETP_DEBUG
		test_generic_check();
		netp::run_test<helper_test_unit>();
#endif

		return true;
	}
}