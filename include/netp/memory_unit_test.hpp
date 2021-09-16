#ifndef _NETP_MEMORY_UNIT_TEST_HPP
#define _NETP_MEMORY_UNIT_TEST_HPP

#include <netp/core.hpp>
#include <netp/memory.hpp>
#include <netp/smart_ptr.hpp>
#include <netp/test.hpp>

namespace netp {
	struct memory_test_unit :
		public netp::test_unit
	{
		bool run();
		void test_memory();
		void test_packet();
		void test_netp_allocator(size_t loop);
		void test_std_allocator(size_t loop);
	};
}
#endif