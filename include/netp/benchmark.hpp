#ifndef _NETP_BENCHMARK_HPP
#define _NETP_BENCHMARK_HPP

#include <chrono>
#include <string>
#include <netp/core.hpp>
#include <netp/app.hpp>

namespace netp {
	enum benchmark_flag {
		bf_no_end_output = 1<<0,
		bf_no_mark_output =1<<1,
		bf_force_us = 1<<2	
	};
	struct benchmark {
		int flag;
		std::string m_tag;
		std::chrono::time_point<std::chrono::steady_clock, std::chrono::steady_clock::duration> begin;
		benchmark(std::string const& tag, int flag_ = bf_no_end_output ) :
			flag(flag_),
			m_tag(tag),
			begin(std::chrono::steady_clock::now())
		{
		}

		~benchmark() {
			std::chrono::time_point<std::chrono::steady_clock, std::chrono::steady_clock::duration> end = std::chrono::steady_clock::now();
			if (!(flag&bf_no_end_output)) { NETP_INFO("[%s][end]cost: %lld %s", m_tag.c_str(), ((end - begin).count() / ( (flag&bf_force_us) ?1000:1)), (flag & bf_force_us) ? "us" : "ns"); }
		}

		inline std::chrono::steady_clock::duration elapsed() const {
			return std::chrono::steady_clock::now() -begin;
		}

		std::chrono::steady_clock::duration mark(std::string const& tag) {
			std::chrono::steady_clock::duration elapsed_ = elapsed();
			if (!(flag&bf_no_mark_output)) {
				NETP_INFO("[%s][%s]cost: %lld %s", m_tag.c_str(), tag.c_str(), ((elapsed_).count() / ((flag & bf_force_us) ? 1000 : 1) ), (flag & bf_force_us) ? "us":"ns");
			}
			return elapsed_;
		}
	};

}


#endif