#ifndef _NETP_CORE_COMPILER_GCC_ARMV7A_HPP
#define _NETP_CORE_COMPILER_GCC_ARMV7A_HPP


//it's strange that construct for netp::condition need the address aligned to 8bytes by placement new,
//and it get SIGBUS on a alignment of 4bytes on the member of std::condition_variable construction
//construct std::condition_variable dependently on address aligned to 4bytes is ok
//need deep investigation

//note: std::malloc on armv7a always return address with 8 aligned
//according to: https://community.arm.com/developer/ip-products/processors/f/cortex-m-forum/7154/alignment-in-arm
// stack should always aligned to 8bytes on arm32

/*
for (int i = 1; i < 10000; ++i) {
	void* m = std::malloc(i);
	NETP_ASSERT((std::size_t(m) % 8) == 0);
	std::free(m);
}


NETP_INFO("sizeof(std::mutex): %d", sizeof(std::mutex));
NETP_INFO("sizeof(std::condition_variable): %d", sizeof(std::condition_variable));
NETP_INFO("sizeof(netp::condition): %d", sizeof(netp::condition));

bfr::bpoll_handle* bh_ptr_1 = (bfr::bpoll_handle*)T_aligned_malloc(sizeof(bfr::bpoll_handle), 1);
bfr::bpoll_handle* bh_ptr_2 = (bfr::bpoll_handle*)T_aligned_malloc(sizeof(bfr::bpoll_handle), 4);
bfr::bpoll_handle* bh_ptr_3 = (bfr::bpoll_handle*)T_aligned_malloc(sizeof(bfr::bpoll_handle), 8);
bfr::bpoll_handle* bh_ptr_4 = (bfr::bpoll_handle*)T_aligned_malloc(sizeof(bfr::bpoll_handle), 16);

std::condition_variable* std_cond_ptr = (std::condition_variable*)T_aligned_malloc(sizeof(std::condition_variable), 4);
::new ((void*)std_cond_ptr)(std::condition_variable)();
NETP_INFO("make std_cond_ptr by placement new done");

netp::condition_variable* netp_cond_ptr = (netp::condition_variable*)T_aligned_malloc(sizeof(netp::condition_variable), 4);
::new ((void*)netp_cond_ptr)(netp::condition_variable)();
NETP_INFO("make netp_cond_ptr by placement new done");

NRP<bfr::bpoll_handle> bh = netp::make_ref<bfr::bpoll_handle>((nullptr));
NETP_INFO("make bh done,pass 0");

NRP<netp::io_event_loop> L = netp::io_event_loop_group::instance()->next();
NRP<bfr::bpoll_handle> bh2 = netp::make_ref<bfr::bpoll_handle>(L);
NETP_INFO("make bh done");
*/


//so, the current solution is just set the align to 8bytes
 
#define NETP_DEFAULT_ALIGN (8)

#ifdef __ARM_NEON__
	#define NETP_ENABLE_NEON
#endif

#endif