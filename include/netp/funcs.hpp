#ifndef _NETP_FUNCS_HPP_
#define _NETP_FUNCS_HPP_

#include <functional>
#include <atomic>
#include <string>
#include <chrono>

#include <random>
#include <netp/core.hpp>

namespace netp {

	template <class InputIt1, class InputIt2 >
	bool equal(InputIt1 first1, InputIt1 last1,
		InputIt2 first2
	) {
		for (; first1 != last1; ++first1, ++first2) {
			if (!(*first1 == *first2)) {
				return false;
			}
		}
		return true;
	}

	template <class InputIt1, class InputIt2, class BinaryPredicate>
	bool equal(InputIt1 first1, InputIt1 last1,
		InputIt2 first2, BinaryPredicate&& p
	) {
		for (; first1 != last1; ++first1, ++first2) {
			if (!p(*first1,*first2)) {
				return false;
			}
		}
		return true;
	}

	extern void random64_reset_seed(u64_t seed);
	extern void random32_reset_seed(u32_t seed);

	extern void random_init_seed();
	extern u64_t random_u64();
	extern u32_t random_u32();
	extern u16_t random_u16();
	extern u8_t random_u8();
	extern u64_t random_u64(u64_t min, u64_t max);
	extern u64_t random_u64(u64_t max);
	extern u32_t random(u32_t min, u32_t max);
	extern u32_t random(u32_t max);

	inline bool is_big_endian() {
		const unsigned int x = 1;
		return 0 == *(unsigned char*)(&x);
	}

	inline bool is_little_endian() {
		return !is_big_endian();
	}

	namespace deprecated {
		//@2018-01-23 deprecated, please use std::swap instead
		//non-atomic ,,, be carefully,,
		template <typename T>
		inline void swap(T& a, T& b) _NETP_NOEXCEPT {
			NETP_ASSERT(&a != &b);
			T tmp(std::move(a));
			a = std::move(b);
			b = std::move(tmp);
		}
	}

	template <class L, class R>
	struct is_same_class
	{
		static bool const value = false;
	};

	template <class L>
	struct is_same_class<L, L>
	{
		static bool const value = true;
	};

	u32_t get_stack_size();
	void assert_failed(const char* error, const char* file, int line, const char* function, ...);

	template <typename T>
	inline T atomic_incre(std::atomic<T>* atom, std::memory_order order = std::memory_order_acq_rel) _NETP_NOEXCEPT {
		return atom->fetch_add(1, order);
	}

	template <typename T>
	inline T atomic_decre(std::atomic<T>* atom, std::memory_order order = std::memory_order_acq_rel) _NETP_NOEXCEPT {
		return atom->fetch_sub(1, order);
	}

	template <typename T>
	inline T atomic_incre_if_not_equal(std::atomic<T>* atom, T const& val,
		std::memory_order success = std::memory_order_acq_rel,
		std::memory_order failure = std::memory_order_acquire) _NETP_NOEXCEPT
	{
		T tv = atom->load(failure);
		NETP_ASSERT(tv >= val);
		for (;;) {
			if (tv == val) {
				return tv;
			}
			if (atom->compare_exchange_weak(tv, tv + 1, success, failure)) {
				return tv;
			}
		}
	}

	template <bool b>
	inline bool is_true() {
		return true;
	}

	template <>
	inline bool is_true<false>() { return false; }

	template <typename T>
	struct less {
		inline bool operator()(T const& a, T const& b)
		{
			return a < b;
		}
	};

	template <typename T>
	struct greater {
		inline bool operator()(T const& a, T const& b)
		{
			return a > b;
		}
	};

	typedef std::function<void()> fn_defer_t;
	class defer {
		fn_defer_t& fn;
	public:
		defer(fn_defer_t&& fn_) :
			fn(fn_)
		{}
		~defer() { fn(); }
	};


	template <class _Ty>
	inline void std_container_release(_Ty& container_) {
		_Ty().swap(container_);
	}

	typedef std::chrono::steady_clock steady_clock_t;
	typedef std::chrono::system_clock system_clock_t;

	typedef std::chrono::nanoseconds nanoseconds_duration_t;
	typedef std::chrono::microseconds	microseconds_duration_t;
	typedef std::chrono::milliseconds milliseconds_duration_t;
	typedef std::chrono::seconds seconds_duration_t;

	typedef std::chrono::time_point<steady_clock_t, nanoseconds_duration_t> steady_nanoseconds_timepoint_t;
	typedef std::chrono::time_point<steady_clock_t, microseconds_duration_t> steady_microseconds_timepoint_t;
	typedef std::chrono::time_point<steady_clock_t, milliseconds_duration_t> steady_milliseconds_timepoint_t;
	typedef std::chrono::time_point<steady_clock_t, seconds_duration_t> steady_seconds_timepoint_t;

	typedef std::chrono::time_point<system_clock_t, nanoseconds_duration_t> system_nanoseconds_timepoint_t;
	typedef std::chrono::time_point<system_clock_t, microseconds_duration_t> system_microseconds_timepoint_t;
	typedef std::chrono::time_point<system_clock_t, milliseconds_duration_t> system_milliseconds_timepoint_t;
	typedef std::chrono::time_point<system_clock_t, seconds_duration_t> system_seconds_timepoint_t;

	template <class dur_to_t, class clock_t>
	inline std::chrono::time_point<clock_t, dur_to_t> now() {
		return std::chrono::time_point_cast<dur_to_t, clock_t,typename clock_t::duration>(clock_t::now());
	}

	template <class dur_to_t>
	inline std::chrono::time_point<std::chrono::steady_clock,dur_to_t> steady_now() {
		return std::chrono::time_point_cast<dur_to_t, std::chrono::steady_clock, typename std::chrono::steady_clock::duration>(std::chrono::steady_clock::now());
	}

	template <class dur_to_t>
	inline std::chrono::time_point<std::chrono::system_clock, dur_to_t> system_now() {
		return std::chrono::time_point_cast<dur_to_t, std::chrono::system_clock, typename std::chrono::system_clock::duration>(std::chrono::system_clock::now());
	}

	template <typename clock_t, typename duration_t
		, typename std::enable_if<std::is_same<typename clock_t::duration, duration_t>::value>::type ** = 0
	>
		inline std::chrono::time_point<clock_t, duration_t> now(typename std::enable_if<std::is_same<typename clock_t::duration, duration_t>::value>::type** = 0) {
		return clock_t::now();
	}

	inline void time_of_day(struct timeval& tv, void* tzp) {
		(void)tzp;

#ifdef _NETP_WIN

#ifdef NETP_USE_CHRONO
		std::chrono::time_point<std::chrono::system_clock, std::chrono::microseconds> tp
			= std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::system_clock::now());

		std::chrono::seconds sec = std::chrono::time_point_cast<std::chrono::seconds>(tp).time_since_epoch();
		std::chrono::microseconds mic_sec = std::chrono::time_point_cast<std::chrono::microseconds>(tp).time_since_epoch();

		std::chrono::duration<double> du = mic_sec - sec;
		tv.tv_sec = static_cast<long>(sec.count());
		tv.tv_usec = static_cast<long>(du.count() * 1000000);
#else
		time_t clock;
		struct tm tm;
		SYSTEMTIME wtm;
		GetLocalTime(&wtm);
		tm.tm_year = wtm.wYear - 1900;
		tm.tm_mon = wtm.wMonth - 1;
		tm.tm_mday = wtm.wDay;
		tm.tm_hour = wtm.wHour;
		tm.tm_min = wtm.wMinute;
		tm.tm_sec = wtm.wSecond;
		tm.tm_isdst = -1;
		clock = mktime(&tm);
		tv.tv_sec = static_cast<long int>(clock);
		tv.tv_usec = wtm.wMilliseconds * 1000;
#endif
#elif defined(_NETP_GNU_LINUX) || defined(_NETP_ANDROID)
		gettimeofday(&tv, nullptr);
#else
	#error "unknown platform"
#endif

	}


	inline void to_localtime_str(struct timeval const& tv, std::string& lcstr) {
		char buf[] = "1970-01-01 00:00:00.000000"; //our time format

		const static char* _fmt_seconds = "%Y-%m-%d %H:%M:%S.000";
		const static char* _fmt_mseconds = "%03d";

		time_t long_time = (time_t)tv.tv_sec;
		struct tm timeinfo;

#ifdef _NETP_WIN
		localtime_s(&timeinfo, &long_time);
#else
		localtime_r(&long_time, &timeinfo);
#endif

		NETP_ASSERT(strlen(buf) == 26);
		strftime(buf, sizeof(buf), _fmt_seconds, &timeinfo);
		int rt = snprintf((char*)(&buf[0] + 20), 6, _fmt_mseconds, (int)(tv.tv_usec / 1000));
		(void)rt;
		NETP_ASSERT(rt == 3);
		NETP_ASSERT(strlen(buf) == 23);
		lcstr = std::string(buf, 23);
	}

	inline void curr_localtime_str(std::string& str) {
		struct timeval tv;
		time_of_day(tv, nullptr);
		to_localtime_str(tv, str);
	}
}
#endif