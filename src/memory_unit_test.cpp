
#include <netp/memory_unit_test.hpp>
#include <netp/benchmark.hpp>
#include <netp/packet.hpp>

namespace netp {

	bool memory_test_unit::run() {
		test_memory();
		test_packet();

		test_netp_allocator(2);
		test_std_allocator(2);
		return true;
	}

	void memory_test_unit::test_memory() {
		NRP<packet>* p = netp::allocator<NRP<packet>>::make();
		netp::allocator<NRP<packet>>::trash(p);

		NRP<packet>* pn = netp::allocator<NRP<packet>>::make_array(10);
		netp::allocator<NRP<packet>>::trash_array(pn, 10);

		u8_t* u8ptr = netp::allocator<u8_t>::malloc(14, 16);
		netp::allocator<u8_t>::free(u8ptr);
	}

	void memory_test_unit::test_packet() {
		NRP<netp::packet> p = netp::make_ref<netp::packet>();
		for (size_t i = 0; i <= TABLE_BOUND[T_COUNT] + 128; ++i) {
			p->write<u8_t>(u8_t(1));
		}

		NRP<netp::packet> p2 = netp::make_ref<netp::packet>();
		for (size_t i = 0; i <= TABLE_BOUND[T_COUNT] + 128; ++i) {
			p2->write_left<u8_t>(u8_t(1));
		}

		NETP_ASSERT(*p == *p2);
	}

#ifdef _NETP_DEBUG
#define POOL_CACHE_MAX_SIZE (16*1024)
#else
#define POOL_CACHE_MAX_SIZE (64*1024)
#endif

	template <class vec_T, size_t size_max>
	void test_vector_pushback(size_t loop) {
		for (size_t k = 0; k < loop; ++k) {
			vec_T vector_int;
			for (size_t i = 0; i < size_max; ++i) {
				//NETP_INFO("vec.capacity(): %u", vector_int.capacity());
				vector_int.push_back(i);
			}
		}
	}

	struct __nonpod :
		public netp::non_atomic_ref_base
	{};

	template <class vec_T, size_t size_max>
	void test_vector_pushback_nonpod(size_t loop) {
		for (size_t k = 0; k < loop; ++k) {
			vec_T vector_int;
			for (size_t i = 0; i < size_max; ++i) {
				//NETP_INFO("vec.capacity(): %u", vector_int.capacity());
				vector_int.push_back(netp::make_ref<__nonpod>());
			}
		}
	}

	void memory_test_unit::test_netp_allocator(size_t loop) {
		{
			netp::benchmark mk("std::vector<size_t,netp::allocator<size_t>");
			test_vector_pushback<std::vector<size_t, netp::allocator<size_t>>, POOL_CACHE_MAX_SIZE>(loop);
		}
		{
			netp::benchmark mk("std::vector<size_t,netp::allocator<size_t>");
			test_vector_pushback<std::vector<size_t, netp::allocator<size_t>>, POOL_CACHE_MAX_SIZE>(loop);
		}

		{
			netp::benchmark mk("std::vector<NRP<__nonpod>,netp::allocator<NRP<__nonpod>>");
			test_vector_pushback_nonpod<std::vector<NRP<__nonpod>, netp::allocator<NRP<__nonpod>>>, POOL_CACHE_MAX_SIZE>(loop);
		}
		{
			netp::benchmark mk("std::vector<NRP<__nonpod>,netp::allocator<NRP<__nonpod>>");
			test_vector_pushback_nonpod<std::vector<NRP<__nonpod>, netp::allocator<NRP<__nonpod>>>, POOL_CACHE_MAX_SIZE>(loop);
		}
	}

	void memory_test_unit::test_std_allocator(size_t loop) {
		{
			netp::benchmark mk("std::vector<size_t,std::allocator<size_t>");
			test_vector_pushback<std::vector<size_t, std::allocator<size_t>>, POOL_CACHE_MAX_SIZE>(loop);
		}
		{
			netp::benchmark mk("std::vector<size_t,std::allocator<size_t>");
			test_vector_pushback<std::vector<size_t, std::allocator<size_t>>, POOL_CACHE_MAX_SIZE>(loop);
		}

		{
			netp::benchmark mk("std::vector<NRP<__nonpod>,std::allocator<NRP<__nonpod>>");
			test_vector_pushback_nonpod<std::vector<NRP<__nonpod>, std::allocator<NRP<__nonpod>>>, POOL_CACHE_MAX_SIZE>(loop);
		}
		{
			netp::benchmark mk("std::vector<NRP<__nonpod>,std::allocator<NRP<__nonpod>>");
			test_vector_pushback_nonpod<std::vector<NRP<__nonpod>, std::allocator<NRP<__nonpod>>>, POOL_CACHE_MAX_SIZE>(loop);
		}
	}

}