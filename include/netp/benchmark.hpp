#ifndef _NETP_BENCHMARK_HPP
#define _NETP_BENCHMARK_HPP

#include <chrono>
#include <string>
#include <netp/core.hpp>
#include <netp/app.hpp>

namespace netp {
	struct benchmark {
		bool no_output;
		std::string m_tag;
		std::chrono::time_point<std::chrono::steady_clock, std::chrono::steady_clock::duration> begin;
		benchmark(std::string const& tag, bool no_output_ = false ) : 
			no_output(no_output_),
			m_tag(tag), begin(std::chrono::steady_clock::now())
		{
		}

		~benchmark() {
			std::chrono::time_point<std::chrono::steady_clock, std::chrono::steady_clock::duration> end = std::chrono::steady_clock::now();
			if (!no_output) { NETP_INFO("[%s][end]cost: %lld us", m_tag.c_str(), ((end - begin).count() / 1000)); }
		}

		inline std::chrono::steady_clock::duration elapsed() const {
			return std::chrono::steady_clock::now() -begin;
		}

		std::chrono::steady_clock::duration mark(std::string const& tag) {
			std::chrono::steady_clock::duration elapsed_ = elapsed();
			if (!no_output) {
				NETP_INFO("[%s][%s]cost: %lld us", m_tag.c_str(), tag.c_str(), ((elapsed_).count() / 1000));
			}
			return elapsed_;
		}
	};

}


#endif