#ifndef _NETP_TEST_HPP
#define _NETP_TEST_HPP

#include <netp/core.hpp>
#include <netp/smart_ptr.hpp>

namespace netp {
	class test_unit :
		public netp::ref_base
	{
		virtual bool run() = 0;
	};

	template<class unit>
	void run_test() {
		NRP<unit> u = netp::make_ref<unit>();
		NETP_ASSERT(u->run());
	}
}

#endif