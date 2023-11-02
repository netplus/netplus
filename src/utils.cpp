#include <netp/string.hpp>
#include <netp/utils.hpp>
#include <netp/exception.hpp>
#include <netp/logger_broker.hpp>

namespace netp {

	std::atomic<i32_t> s_event_handler_id{ 1 };

	__NETP_TLS std::mt19937_64 __netp_rndg_64;
	__NETP_TLS std::mt19937 __netp_rndg_32;

	//note: gcc do not has const distri construction for random generator
	std::uniform_int_distribution<u64_t> __netp_distr64;
	std::uniform_int_distribution<u32_t> __netp_distr32;
	std::uniform_int_distribution<u16_t> __netp_distr16;
	std::uniform_int_distribution<u16_t> __netp_distr8(0, 0xff);

	void random64_reset_seed(u64_t seed) {
		__netp_rndg_64.seed(seed);
	}

	void random32_reset_seed(u32_t seed) {
		__netp_rndg_32.seed(seed);
	}

	void random_init_seed() {
		std::random_device rd;
		random64_reset_seed(rd());
		random32_reset_seed(rd());
	}

	u64_t random_u64() {
		return __netp_distr64(__netp_rndg_64);
	}

	u32_t random_u32() {
		return __netp_distr32(__netp_rndg_32);
	}

	u16_t random_u16() {
		return __netp_distr16(__netp_rndg_32);
	}

	u8_t random_u8() {
		return u8_t(__netp_distr8(__netp_rndg_32));
	}

	u64_t random_u64(u64_t min, u64_t max) {
		std::uniform_int_distribution<uint64_t> __distr(min, max);
		return __distr(__netp_rndg_64);
	}

	u64_t random_u64(u64_t max) {
		return random_u64(u64_t(0),max);
	}

	//[min,max]
	u32_t random(u32_t min, u32_t max) {
		std::uniform_int_distribution<uint32_t> __distr(min, max);
		return __distr(__netp_rndg_32);
	}

	u32_t random(u32_t max) {
		return random(u32_t(0), max);
	}

	u32_t get_stack_size() {
#ifdef _NETP_GNU_LINUX
		pthread_attr_t attr;
		pthread_attr_init(&attr);
		::size_t s;
		pthread_attr_getstacksize(&attr, &s);
		int rt = pthread_attr_destroy(&attr);
		NETP_ASSERT(rt == 0);
		(void)&rt;
		return s;
#else
		return 0; //not implemented
#endif
	}

#define _ASSERT_INFO_MAX_LEN 512
#define _ASSERT_MSG_MAX_LEN 512
#define _ASSERT_MSG_TOTAL_LEN (_ASSERT_INFO_MAX_LEN+_ASSERT_MSG_MAX_LEN)
	void assert_failed(const char* check, const char* file, int line, const char* function, ...)
	{
		char _info[_ASSERT_INFO_MAX_LEN] = { 0 };
		va_list argp;
		va_start(argp, function);
		char* fmt = va_arg(argp, char*);
		int i = vsnprintf(_info, _ASSERT_INFO_MAX_LEN, fmt, argp);
		va_end(argp);

		if ((i < 0) || i > _ASSERT_INFO_MAX_LEN) {
			throw netp::exception(netp::E_ASSERT_FAILED, "assert: call vsnprintf failed", file, line, function);
		}

		if (i == 0) {
			static const char* _no_info_attached = "no info";
			netp::strncpy(_info, _no_info_attached, netp::strlen(_no_info_attached));
		}
		char _message[_ASSERT_MSG_TOTAL_LEN] = { 0 };
		int c = snprintf(_message, _ASSERT_MSG_TOTAL_LEN, "assert: %s, info: %s\nfile : %s\nline : %d\nfunc : %s",
			check, _info, file, line, function);

		if ((c < 0) || c > (_ASSERT_MSG_TOTAL_LEN - i)) {
			throw netp::exception(netp::E_ASSERT_FAILED, "assert: format failed", file, line, function);
		}
		throw netp::exception(netp::E_ASSERT_FAILED, _message, file, line, function);
	}
}