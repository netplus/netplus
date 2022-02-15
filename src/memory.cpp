#include <netp/memory.hpp>
#include <netp/app.hpp>

namespace netp {

//ALIGN_SIZE SHOUDL BE LESS THAN 256bit
//STORE OFFSET IN PREVIOUS BYTES
#define _NETP_ALIGN_MALLOC_SIZE_MAX (0xffffffffff)
//1<<40 should be ok | 1000G
	union aligned_hdr {
		struct _aligned_hdr {
			u32_t size_L;
			struct __AH_4_7__ {
				u8_t size_H;
				u8_t t : 4;
				u8_t s : 4;
				u8_t alignment;
				u8_t offset;
			} AH_4_7;
		} hdr;
		u8_t __bytes_0_7[8];
	};
	static_assert(sizeof(aligned_hdr::_aligned_hdr) == 8 && sizeof(aligned_hdr) == 8, "check sizeof(aligned_hdr) failed");

	/*note
	* gcc&ubuntu20 (64 bit) on x86_64 alignof(std::max_align_t) == 16
	* gcc&raspberry pi 4 (32bit) on armv7a: alignof(std::max_align_t) == 8
	* msvc&win10 (64bit) on x86_64 alignof(std::max_align_t) == 8
	*/ 
	static_assert( alignof(std::max_align_t) <= NETP_DEFAULT_ALIGN, "check default align failed");
	static_assert( (alignof(std::max_align_t) % alignof(aligned_hdr)) == 0, "check aligned_hdr alignment");
	static_assert( (alignof(std::max_align_t) < sizeof(aligned_hdr)) ? (sizeof(aligned_hdr) % (alignof(std::max_align_t)) == 0 ) : true, "hdr alignment check");

	//@note: msvc debug compiler would not inline these func, but release version do
	__NETP_FORCE_INLINE
	static u8_t __AH_UPDATE_OFFSET__(aligned_hdr* a_hdr, size_t alignment) {
		//@ (2's complement) https://zh.wikipedia.org/wiki/%E4%BA%8C%E8%A3%9C%E6%95%B8 
		//@note: pls refer to https://en.wikipedia.org/wiki/Data_structure_alignment
		/*
		*padding = (align - (offset mod align)) mod align
		* Since the alignment is by definition a power of two,[a] the modulo operation can be reduced to a bitwise boolean AND operation.
		padding = (align - (offset & (align - 1))) & (align - 1)
        = (-offset & (align - 1))

		align&(align-1) = 0
		(align-1)&(align-1) = align-1

		1) ~(0x0001-1) + 0x0001 = 10000 = 0
		2) a + (-a) =0
		3) ~(a-1) + a = 0
		4) -a = ~(a-1)
		5) a + (~a) + 1 = 0
		
		0x0001 + 0x1110(~0x1001) = 0x1111
		0x1111 + 1 = 0x1 0000 = 0
		*/
		const u8_t offset = u8_t(sizeof(aligned_hdr)) + u8_t((~(std::size_t(a_hdr) + sizeof(aligned_hdr) - 1)) & (alignment-1));

#ifdef _NETP_DEBUG
		NETP_ASSERT(((~(std::size_t(a_hdr) + sizeof(aligned_hdr) - 1)) == (~(std::size_t(a_hdr) + sizeof(aligned_hdr)) + 1)) && (((std::size_t(a_hdr) + std::size_t(offset)) % alignment) == 0) );
		NETP_ASSERT(
			NETP_IS_DEFAULT_ALIGN(alignment) ? ((alignment <= (sizeof(aligned_hdr))) ? offset == sizeof(aligned_hdr) : offset == (alignment))
			: (offset <= (sizeof(aligned_hdr)+alignment))
		, "alignment: %u, alignof(std::max_align_t): %u, sizeof(aligned_hdr): %u, a_hdr: %ull", alignment, alignof(std::max_align_t), sizeof(aligned_hdr), std::size_t(a_hdr));
#endif

		a_hdr->hdr.AH_4_7.alignment = u8_t(alignment);
		a_hdr->hdr.AH_4_7.offset = (offset);
		/*in case if the offset is not sizeof(aligned_hdr), no cmp, just set */
		*(reinterpret_cast<u8_t*>(a_hdr) + (offset) - 1) = (offset);

		return offset;
	}

	__NETP_FORCE_INLINE static void __AH_UPDATE_SIZE(aligned_hdr* a_hdr, size_t size) {
		a_hdr->hdr.size_L = (size&0xffffffff);
#ifdef _NETP_AM64
		a_hdr->hdr.AH_4_7.size_H = ((size >> 32) & 0xff);
#endif
	}

	__NETP_FORCE_INLINE const static size_t __AH_SIZE(aligned_hdr* a_hdr) {
#ifdef _NETP_AM64
		return a_hdr->hdr.size_L|(size_t(a_hdr->hdr.AH_4_7.size_H)<<32);
#else
		return a_hdr->hdr.size_L;
#endif
	}

	enum SLOT_ENTRIES_SIZE_LEVEL {
		L_DISABLED,
		L_EXTREM_LOW,
		L_LOW,
		L_MEDIUM,
		L_LARGE,
		L_EXTREM_LARGE,
		L_MAX
	};

	static SLOT_ENTRIES_SIZE_LEVEL g_memory_pool_slot_entries_size_level = L_LARGE;
	enum allocator_block_pool_state {
		s_idle,
		s_initing,
		s_init_done,
		s_deiniting
	};
	//if mutile-thread try to call cfg_memory_init_global_allocator, only one thread succeed to new the global_pool_aligned_allocator
	static std::atomic<u8_t> ___netp_allocator_with_block_pool_manager_init_done(s_idle);
	static allocator_with_block_pool_manager* ___netp_allocator_with_block_pool_manager = nullptr;
	void cfg_memory_pool_slot_entries_size_level(int l) {
#ifdef _NETP_DEBUG
		NETP_ASSERT(___netp_allocator_with_block_pool_manager_init_done.load(std::memory_order_acquire) == s_idle);
#endif
		if (l < L_DISABLED) {
			l = L_DISABLED;
		} else if (l > L_EXTREM_LARGE) {
			l = L_EXTREM_LARGE;
		}

		g_memory_pool_slot_entries_size_level = SLOT_ENTRIES_SIZE_LEVEL(l);
		std::atomic_thread_fence(std::memory_order_release);
	}

#ifdef _NETP_DEBUG
	static std::atomic<long long> ___netp_global_alloc(0);
	static std::atomic<long long> ___netp_global_dealloc(0);
	void __cfg_memory_pool_alloc_dealloc_check_reset() {
		___netp_global_alloc = ___netp_global_dealloc = 0;
	}
	void __cfg_memory_pool_alloc_dealloc_check() {
		if (___netp_global_alloc != ___netp_global_dealloc) {
			char buf[256] = { 0 };
			snprintf(buf, 256, "[netp][cfg_memory_pool]___netp_global_alloc-___netp_global_dealloc: %llu", ___netp_global_alloc - ___netp_global_dealloc);
			NETP_THROW(buf);
		}
	}
#endif

	void cfg_memory_init_allocator_with_block_pool_manager() {
		u8_t state = s_idle;
		bool challengert = false;
		do {
			challengert = ___netp_allocator_with_block_pool_manager_init_done.compare_exchange_weak(state, s_initing, std::memory_order_acq_rel, std::memory_order_acquire);
			if (state != s_idle) {
				return;
			}
		} while (!challengert);

		NETP_ASSERT(___netp_allocator_with_block_pool_manager_init_done.load(std::memory_order_acquire) == s_initing);
		NETP_ASSERT(___netp_allocator_with_block_pool_manager == nullptr);

		___netp_allocator_with_block_pool_manager = ::new allocator_with_block_pool_manager();
		___netp_allocator_with_block_pool_manager->init(true);
		std::atomic_thread_fence(std::memory_order_release);
		state = s_initing;
		challengert = ___netp_allocator_with_block_pool_manager_init_done.compare_exchange_strong(state, s_init_done, std::memory_order_acq_rel, std::memory_order_acquire);
		NETP_ASSERT(challengert == true);

#ifdef _NETP_DEBUG
		__cfg_memory_pool_alloc_dealloc_check_reset();
#endif
	}

	void cfg_memory_deinit_allocator_with_block_pool_manager() {
		u8_t state = s_init_done;
		bool challengert = false;
		do {
			challengert = ___netp_allocator_with_block_pool_manager_init_done.compare_exchange_weak(state, s_deiniting, std::memory_order_acq_rel, std::memory_order_acquire);
			if (state != s_init_done) {
				return;
			}
		} while (!challengert);
		___netp_allocator_with_block_pool_manager->deinit();
		::delete ___netp_allocator_with_block_pool_manager;
		___netp_allocator_with_block_pool_manager = nullptr;
		std::atomic_thread_fence(std::memory_order_release);
		state = s_deiniting;
		challengert = ___netp_allocator_with_block_pool_manager_init_done.compare_exchange_strong(state, s_idle, std::memory_order_acq_rel, std::memory_order_acquire);
		NETP_ASSERT(challengert == true);

#ifdef _NETP_DEBUG
		__cfg_memory_pool_alloc_dealloc_check();
#endif
	}

	allocator_with_block_pool* cfg_memory_create_allocator_with_block_pool() {
		return ___netp_allocator_with_block_pool_manager->create_allocator_block_pool();
	}
	void cfg_memory_destory_allocator_with_block_pool(allocator_with_block_pool* allocator) {
		___netp_allocator_with_block_pool_manager->destory_allocator_block_pool(allocator);
	}

	//object pool does not suit for large memory gap objects
	//@note: tls default record size 16kb
	constexpr static u32_t TABLE_SLOT_ENTRIES_INIT_LIMIT[SLOT_ENTRIES_SIZE_LEVEL::L_MAX][TABLE::T_COUNT][8] = {
		{//L_DISABLED
			{0,0,0,0,0,0,0,0}, //
			{0,0,0,0,0,0,0,0}, //
			{0,0,0,0,0,0,0,0}, //
			{0,0,0,0,0,0,0,0},//
			{0,0,0,0,0,0,0,0},//
			{0,0,0,0,0,0,0,0},//
			{0,0,0,0,0,0,0,0},//
			{0,0,0,0,0,0,0,0},//
			{0,0,0,0,0,0,0,0},//
			{0,0,0,0,0,0,0,0},//
			{0,0,0,0,0,0,0,0},//
			{0,0,0,0,0,0,0,0},//
			{0,0,0,0,0,0,0,0},//
			//4,//512-1M
			//2,//1M-2M
			//1//
		},
		{//L_EXTREM_LOW
			{512,256,256,128,128,128,128,128}, //
			{128,128,128,128,64,64,64,64}, //
			{64,64,64,64,32,32,32,32}, //
			{32,32,32,32,32,32,32,32}, //
			{16,16,16,128,128,128,16,16},//
			{16,16,16,16,8,8,8,8},//
			{8,8,8,8,8,8,8,8},//
			{8,8,8,8,8,8,8,8},//
			{4,4,4,4,4,4,4,4},//
			{0,0,0,0,0,0,0,0,},//
			{0,0,0,0,0,0,0,0,},//
			{0,0,0,0,0,0,0,0,},//
			{0,0,0,0,0,0,0,0,},//
		},
		{//L_LOW
			{1024,512,256,256,256,256,256,256}, //
			{256,256,256,256,128,128,128,128}, //
			{128,128,128,128,64,64,64,64}, //
			{32,32,32,32,32,32,32,32}, //
			{16,16,16,256,256,256,16,16},//
			{16,16,16,16,16,16,16,16},//
			{16,16,16,16,8,8,8,8},//
			{8,8,8,8,8,8,8,8},//
			{4,4,4,4,4,4,4,4},//
			{2,2,2,2,0,0,0,0},//
			{0,0,0,0,0,0,0,0},//
			{0,0,0,0,0,0,0,0},//
			{0,0,0,0,0,0,0,0},//
		},
		{//L_MEDIUM
			{2048,1024,512,512,256,256,256,256}, //
			{256,256,256,256,128,128,128,128}, //
			{128,128,128,128,128,128,128,128}, //
			{64,64,64,64,64,64,64,64}, //
			{64,64,64,512,512,512,32,32},//
			{32,32,32,32,16,16,16,16},//
			{16,16,16,16,16,16,16,16},//
			{16,16,16,16,16,16,16,16},//
			{8,8,8,8,8,8,8,8},//
			{8,8,8,8,4,4,4,4},//
			{4,4,4,4,4,4,4,4},//
			{0,0,0,0,0,0,0,0},//
			{0,0,0,0,0,0,0,0},//
		},
		{//L_LARGE
			{4096,2048,1024,1024,512,512,512,512}, //
			{512,512,512,512,256,256,256,256}, //
			{256,256,256,256,256,256,256,256}, //
			{128,128,128,128,128,128,128,128}, //
			{128,128,128,1024,1024,1024,64,64},//
			{64,64,64,64,64,64,64,64},//
			{32,32,32,32,32,32,32,32},//
			{32,32,32,32,32,32,32,32},//
			{16,16,16,16,16,16,16,16},//
			{16,16,16,16,8,8,8,8},//
			{8,8,8,8,4,4,4,4},//
			{4,4,4,4,4,4,4,4},//
			{0,0,0,0,0,0,0,0},//
		},
		{//L_EXTREM_LARGE
			{8192,4096,4096,2048,1024,1024,1024,1024}, //(0--64] /8 Byte/s
			{1024,1024,1024,1024,512,512,512,512}, //(64--64+128] /16 Byte/s
			{512,512,512,512,512,512,512,512}, //(192--64+128+256] /32 Byte/s
			{256,256,256,256,256,256,256,256}, //(448--64+128+256+512] 64 Byte/s
			{256,256,256,2048,2048,2048,128,128},//(960--64+128+256+512+1024] [zzz] 128 Byte/s
			{128,128,128,128,128,128,128,128},//1984--64+128+256+512+1024+2048 256 Byte/s
			{128,128,128,128,64,64,64,64},//4032--4032+4096 /8
			{64,64,64,64,32,32,32,32},//8128--8128+8192
			{32,32,32,32,32,32,32,32},//16320--16320
			{32,32,32,32,16,16,16,16},//32704--32704+32768
			{16,16,16,16,8,8,8,8},//65472--65472+65536
			{8,8,8,8,4,4,4,4},//131008-131008+131072
			{4,4,4,4,4,4,4,4},//262080-262080+262144K
		}
	};

	constexpr static u8_t TABLE_FROM_DIV64[64] = {
		T0,T1,T1,T2,T2,T2,T2,T3,
		T3,T3,T3,T3,T3,T3,T3,T4,
		T4,T4,T4,T4,T4,T4,T4,T4,
		T4,T4,T4,T4,T4,T4,T4,T5,
		T5,T5,T5,T5,T5,T5,T5,T5,
		T5,T5,T5,T5,T5,T5,T5,T5,
		T5,T5,T5,T5,T5,T5,T5,T5,
		T5,T5,T5,T5,T5,T5,T5,T6
	};

	constexpr static u32_t TABLE_BOUND[TABLE::T_COUNT + 1] = {
		0,
		64 + 0, //T0_MAX: 64
		64 + 128, //T1_MAX: 192
		64 + 128 + 256, //T2_MAX: 448
		64 + 128 + 256 + 512,//T3_MAX:  960
		64 + 128 + 256 + 512 + 1024,//T4_MAX: 1984
		64 + 128 + 256 + 512 + 1024 + 2048,//T5_MAX: 4032
		64 + 128 + 256 + 512 + 1024 + 2048 + 4096,//T6_MAX: 8128
		64 + 128 + 256 + 512 + 1024 + 2048 + 4096 + 8192,//T7_MAX
		64 + 128 + 256 + 512 + 1024 + 2048 + 4096 + 8192 + 16384,//T8_MAX
		64 + 128 + 256 + 512 + 1024 + 2048 + 4096 + 8192 + 16384 + 32768,//T9_MAX
		64 + 128 + 256 + 512 + 1024 + 2048 + 4096 + 8192 + 16384 + 32768 + 65536,//T10_MAX
		64 + 128 + 256 + 512 + 1024 + 2048 + 4096 + 8192 + 16384 + 32768 + 65536 + 131072,//T11: 262080
		64 + 128 + 256 + 512 + 1024 + 2048 + 4096 + 8192 + 16384 + 32768 + 65536 + 131072 + 262144 //T12: 524214
	 };

	#define NETP_ALIGNED_ALLOCATOR_SLOT_MAX (8)

	#define _netp_memory_calc_F_by_TABLE(t) (t+3)
	#define _netp_memory_calc_SIZE_by_TABLE_SLOT(t,s) (TABLE_BOUND[t] + ((1<<(_netp_memory_calc_F_by_TABLE(t)))) * (((s) + 1)))
	
	#ifdef _NETP_DEBUG
		#define NETP_ASSERT_calc_TABLE NETP_ASSERT
	#else
		#define NETP_ASSERT_calc_TABLE(...) 
	#endif

#define _netp_memory_calc_size_64step(size) (size>>6)
#define _netp_memory_calc_size_mod_64step(size) ((size%64))
#define _netp_memory_calc_size_4096step(size) ((size-4032)>>12)
#define _netp_memory_calc_size_mod_4096step(size) (((size-4032)%4096))

#define _netp_memory_calc_TABLE_full_search(size,t,s) do { \
	for (u8_t ti = T1; ti < (TABLE::T_COUNT + 1); ++ti) {\
		if (size < TABLE_BOUND[ti]) {\
			t = (--ti); \
			break; \
		} else if (size == TABLE_BOUND[ti]) { \
			t = ti; \
			s = (0); \
			break; \
		} \
	} \
	}while (false)

	static_assert(_netp_memory_calc_size_64step(TABLE_BOUND[T6]) <= 63, "table bound check");
	static_assert(_netp_memory_calc_size_4096step(TABLE_BOUND[T12]) <= 63, "table bound check");

#define _netp_memory_calc_TABLE(size,t,s) do { \
	if(size<=TABLE_BOUND[T6]/*4032*/) {\
		const u8_t step = u8_t(_netp_memory_calc_size_64step(size));\
		t = TABLE_FROM_DIV64[step];\
		s=(step&(step+1))|(_netp_memory_calc_size_mod_64step(size)) ;\
	} else if(size<=TABLE_BOUND[T12]/*262080*/) { \
		const u8_t step = u8_t(_netp_memory_calc_size_4096step(size)); \
		t = (T6+TABLE_FROM_DIV64[step]);\
		s=(step&(step+1))|(_netp_memory_calc_size_mod_4096step(size)!=0?u8_t(-1):0) ;\
	} else if(size<=TABLE_BOUND[T_COUNT]) { \
		for (u8_t ti = (T12+1); ti < (TABLE::T_COUNT + 1); ++ti) {\
			if (size < TABLE_BOUND[ti]) {\
				t = (--ti); \
				break; \
			} else if (size == TABLE_BOUND[ti]) {\
				t = ti; \
				s = (0); \
				break; \
			} \
		}\
	}\
}while(false);

#ifdef _NETP_DEBUG_MEMORY_TABLE
	void 	memory_test_table() {
		for (u32_t size = 1; (size < TABLE_BOUND[T_COUNT-1]+1000); ++size) {
			u8_t t = T_COUNT;
			u8_t s = u8_t(-1);
			u8_t t_fullsearch = T_COUNT;
			u8_t s_fullsearch = u8_t(-1);
			_netp_memory_calc_TABLE(size, t, s);
			_netp_memory_calc_TABLE_full_search(size, t_fullsearch, s_fullsearch);
			NETP_ASSERT(t == t_fullsearch, "size: %u,t: %u, s: %u, t_fullsearch:%u, s_fullsearch: %u", size, t, s, t_fullsearch, s_fullsearch);
			NETP_ASSERT(s ==0 ? s == s_fullsearch:true, "size: %u,t: %u, s: %u, t_fullsearch:%u, s_fullsearch:%u",size, t,s, t_fullsearch,s_fullsearch);
		}
	}
#endif
	

	void allocator_with_block_pool::preallocate_table_slot_item(table_slot_t* tst, u8_t t, u8_t s, size_t item_count) {
		(void)tst;
		size_t size = _netp_memory_calc_SIZE_by_TABLE_SLOT(t, s);
		size_t i;
		for (i = 0; i < (item_count); ++i) {
			aligned_hdr* a_hdr = (aligned_hdr*)std::malloc(sizeof(aligned_hdr) + size);

			if (a_hdr == 0) {
				NETP_THROW("[preallocate_table_slot_item]std::malloc falied");
			}
#ifdef _NETP_DEBUG
			++___netp_global_alloc;
#endif

#ifdef _NETP_DEBUG
			NETP_ASSERT((std::size_t(a_hdr) % alignof(std::max_align_t)) == 0);
#endif
			a_hdr->hdr.AH_4_7.t = t;
			a_hdr->hdr.AH_4_7.s = s;
			u8_t offset = __AH_UPDATE_OFFSET__(a_hdr, NETP_DEFAULT_ALIGN);
			allocator_with_block_pool::free((u8_t*)a_hdr + offset);
		}
	}

	void allocator_with_block_pool::deallocate_table_slot_item(table_slot_t* tst) {
		while (tst->count) {//stop at 0
#ifdef _NETP_DEBUG
			++___netp_global_dealloc;
#endif
			std::free( (void*)(tst->ptr[--tst->count]) );
		}
	}

	void allocator_with_block_pool::init( bool is_mgr ) {
		std::atomic_thread_fence(std::memory_order_acquire);
		for (u8_t t = 0; t < TABLE::T_COUNT; ++t) {
			m_tables[t] = (table_slot_t**) std::malloc(sizeof(table_slot_t**) * NETP_ALIGNED_ALLOCATOR_SLOT_MAX);
			for (u8_t s = 0; s < NETP_ALIGNED_ALLOCATOR_SLOT_MAX; ++s) {
				const u32_t max_n = !is_mgr ? TABLE_SLOT_ENTRIES_INIT_LIMIT[g_memory_pool_slot_entries_size_level][t][s]:0;
				u8_t* __ptr = (u8_t*) std::malloc(sizeof(table_slot_t) + (sizeof(u8_t*) * max_n));
				m_tables[t][s] = (table_slot_t*)__ptr;
				m_tables[t][s]->max = max_n;
				m_tables[t][s]->count = 0;
				m_tables[t][s]->ptr = (u8_t**)(__ptr + (sizeof(table_slot_t)));

				if ( (max_n >0) && (t< TABLE::T_COUNT /*<1k*/) ) {
					preallocate_table_slot_item(m_tables[t][s], t, s, (TABLE_SLOT_ENTRIES_INIT_LIMIT[g_memory_pool_slot_entries_size_level][t][s]>>1) );
				}
			}
		}
	}

	void allocator_with_block_pool::deinit() {
		for (u8_t t = 0; t < TABLE::T_COUNT; ++t) {
			for (u8_t s = 0; s < NETP_ALIGNED_ALLOCATOR_SLOT_MAX; ++s) {
				deallocate_table_slot_item(m_tables[t][s]);
				//deallocate_table_slot(m_tables[t][s]);
				std::free(m_tables[t][s]);
			}
			std::free(m_tables[t]);
		}
	}

	allocator_with_block_pool::allocator_with_block_pool()
	{
	}

	allocator_with_block_pool::~allocator_with_block_pool() {
	}

	void* allocator_with_block_pool::malloc(size_t size, size_t alignment) {
#ifdef _NETP_DEBUG
		NETP_ASSERT( (size<_NETP_ALIGN_MALLOC_SIZE_MAX) && ((alignment % alignof(std::max_align_t)) == 0 && alignment >= alignof(std::max_align_t)) );
#endif
		aligned_hdr* a_hdr;
		u8_t t = T_COUNT;
		u8_t s = u8_t(-1);
		//assume that the std::malloc always return address aligned to alignof(std::max_align_t)
		size_t slot_size = NETP_IS_DEFAULT_ALIGN(alignment) ? ((alignment<=(sizeof(aligned_hdr)))?size: (size + alignment - sizeof(aligned_hdr)))
			: (size+alignment);
		_netp_memory_calc_TABLE(slot_size,t,s);

		if (NETP_LIKELY(t < T_COUNT) ) {

			if (s==0) {
				--t;
				s = (NETP_ALIGNED_ALLOCATOR_SLOT_MAX - 1);
			} else {
				s = u8_t( (slot_size-TABLE_BOUND[t]) >> (_netp_memory_calc_F_by_TABLE(t)));
				slot_size = _netp_memory_calc_SIZE_by_TABLE_SLOT(t, s);
		
				#ifdef _NETP_DEBUG
					NETP_ASSERT(s < NETP_ALIGNED_ALLOCATOR_SLOT_MAX );
				#endif
			}

			table_slot_t*& tst = (m_tables[t][s]);
			if (tst->count) {
__fast_path:
	#ifdef _NETP_DEBUG
				NETP_ASSERT(TABLE_SLOT_ENTRIES_INIT_LIMIT[g_memory_pool_slot_entries_size_level][t][s]>0, "l: %u, t: %u, s: %u, tst->count: %u, tst->max: %u", g_memory_pool_slot_entries_size_level, t,s , tst->count, tst->max );
				NETP_ASSERT(tst->ptr[tst->count-1] != 0, "tst->count: %u, tst->max: %u", tst->count , tst->max );
	#endif
				 a_hdr = (aligned_hdr*) (tst->ptr[--tst->count]);

				 //update new size
				 __AH_UPDATE_SIZE(a_hdr, size);
				const u8_t offset = __AH_UPDATE_OFFSET__(a_hdr, alignment);
#ifdef _NETP_DEBUG
				NETP_ASSERT((sizeof(aligned_hdr) + _netp_memory_calc_SIZE_by_TABLE_SLOT(t,s) - offset) >= size);
#endif
				 return (u8_t*)a_hdr + offset ;
			}

			if (tst->max) {
				//check global
				//tst->max ==0 means no pool object allowed in this slot
				//borrow
				size_t c = ___netp_allocator_with_block_pool_manager->borrow(t, s, tst, (tst->max) >> 1);
#ifdef _NETP_DEBUG
				NETP_ASSERT(c == tst->count);
#endif
				if (c != 0) {
					goto __fast_path;
				}
			}
		}

		//borrow failed || t == T_COUNT, continue with malloc
		//std::malloc always return ptr aligned to alignof(std::max_align_t), so ,we do not need to worry about the hdr access
		a_hdr = (aligned_hdr*)std::malloc( sizeof(aligned_hdr)+slot_size );
		if (NETP_UNLIKELY(a_hdr == 0)) {
			return 0;
		}
#ifdef _NETP_DEBUG
		++___netp_global_alloc;
#endif

#ifdef _NETP_DEBUG
		NETP_ASSERT((std::size_t(a_hdr) % alignof(std::max_align_t)) == 0);
#endif

		//size is used by realloc
		__AH_UPDATE_SIZE(a_hdr, size);
		a_hdr->hdr.AH_4_7.t = t;
		a_hdr->hdr.AH_4_7.s = s;
		u8_t offset = __AH_UPDATE_OFFSET__(a_hdr, alignment);
#ifdef _NETP_DEBUG
		NETP_ASSERT( (sizeof(aligned_hdr) + slot_size - offset ) >= size );
#endif
		return (u8_t*)a_hdr + offset;
	}

	void allocator_with_block_pool::free(void* ptr) {
		if (NETP_UNLIKELY(ptr == 0)) { return; }

		u8_t offset = *(reinterpret_cast<u8_t*>(ptr) - 1);
		aligned_hdr* a_hdr = (aligned_hdr*)((u8_t*)ptr - offset);
#ifdef _NETP_DEBUG
		NETP_ASSERT( a_hdr->hdr.AH_4_7.offset == offset );
#endif
		u8_t t = a_hdr->hdr.AH_4_7.t;
		u8_t s = a_hdr->hdr.AH_4_7.s;
		table_slot_t*& tst = (m_tables[t][s]);

		//if tst->max == tst->count ==0, skiped
		if ((t < T_COUNT) && (tst->count<tst->max) ) {
			tst->ptr[tst->count++] = (u8_t*)a_hdr;
			if (tst->count == tst->max) {
				___netp_allocator_with_block_pool_manager->commit(t, s, tst, (tst->max) >> 1);
			}
			return;
		}
#ifdef _NETP_DEBUG
		++___netp_global_dealloc;
#endif
		std::free((void*)a_hdr);
	}

	//alloc, then copy
	void* allocator_with_block_pool::realloc(void* old_ptr, size_t size, size_t alignment) {
		//align_alloc first
		if (old_ptr == 0) { return allocator_with_block_pool::malloc(size, alignment); }

		u8_t* n_ptr = (u8_t*)allocator_with_block_pool::malloc(size, alignment);

		if (NETP_UNLIKELY(n_ptr == 0)) {
			return 0;
		}

		//we would never get old ptr, cuz old ptr have not yet been returned
#ifdef _NETP_DEBUG
		NETP_ASSERT(n_ptr != old_ptr);
#endif

		u8_t old_offset = *(reinterpret_cast<u8_t*>(old_ptr) - 1);
		aligned_hdr* old_a_hdr = (aligned_hdr*)((u8_t*)old_ptr - old_offset);

#ifdef _NETP_DEBUG
		NETP_ASSERT(old_a_hdr->hdr.AH_4_7.offset == old_offset);
#endif
		size_t old_size = __AH_SIZE(old_a_hdr);
		//do copy
		std::memcpy(n_ptr, old_ptr, NETP_MIN(size, old_size));

		//free ptr
		allocator_with_block_pool::free(old_ptr);
		return n_ptr;
	}

	allocator_with_block_pool_manager::allocator_with_block_pool_manager()
	{
		for (size_t t = 0; t < sizeof(m_tables) / sizeof(m_tables[0]); ++t) {
			m_table_slots_mtx[t] = ::new spin_mutex[NETP_ALIGNED_ALLOCATOR_SLOT_MAX];
		}
	}

	allocator_with_block_pool_manager::~allocator_with_block_pool_manager() {
		//deallocate mutex
		for (size_t t = 0; t < sizeof(m_tables) / sizeof(m_tables[0]); ++t) {
			::delete[] m_table_slots_mtx[t];
		}
	}

	//default: one thread -- main thread
	allocator_with_block_pool* allocator_with_block_pool_manager::create_allocator_block_pool() {

		allocator_with_block_pool* alloc = ::new allocator_with_block_pool();
		NETP_ALLOC_CHECK(alloc, sizeof(alloc));
		alloc->init(false);

		for (size_t t = 0; t < sizeof(m_tables) / sizeof(m_tables[0]); ++t) {
			for (size_t s = 0; s < NETP_ALIGNED_ALLOCATOR_SLOT_MAX; ++s) {
				lock_guard<spin_mutex> lg(m_table_slots_mtx[t][s] );
				if (TABLE_SLOT_ENTRIES_INIT_LIMIT[g_memory_pool_slot_entries_size_level][t][s] == 0) {
#ifdef _NETP_DEBUG
					NETP_ASSERT(m_tables[t][s]->max == 0);
#endif
					continue;
				}
				u32_t max_n = (m_tables[t][s]->max + TABLE_SLOT_ENTRIES_INIT_LIMIT[g_memory_pool_slot_entries_size_level][t][s]);
				u8_t* __ptr = (u8_t*)(std::realloc(m_tables[t][s], sizeof(table_slot_t) + sizeof(u8_t*) * max_n));
				m_tables[t][s] = (table_slot_t*)__ptr;
				m_tables[t][s]->max = max_n;
				m_tables[t][s]->ptr = (u8_t**) (__ptr + sizeof(table_slot_t));
			}
		}

		return alloc;
	}

	void allocator_with_block_pool_manager::destory_allocator_block_pool(allocator_with_block_pool* abp) {
		abp->deinit();
		::delete abp;

		for (size_t t = 0; t < sizeof(m_tables) / sizeof(m_tables[0]); ++t) {
			for (size_t s = 0; s < NETP_ALIGNED_ALLOCATOR_SLOT_MAX; ++s) {
				lock_guard<spin_mutex> lg(m_table_slots_mtx[t][s]);
				if (TABLE_SLOT_ENTRIES_INIT_LIMIT[g_memory_pool_slot_entries_size_level][t][s] == 0) {
#ifdef _NETP_DEBUG
					NETP_ASSERT(m_tables[t][s]->max == 0);
#endif
					continue;
				}

#ifdef _NETP_DEBUG
				NETP_ASSERT(m_tables[t][s]->max >= TABLE_SLOT_ENTRIES_INIT_LIMIT[g_memory_pool_slot_entries_size_level][t][s]);
#endif
				u32_t max_n = (m_tables[t][s]->max - TABLE_SLOT_ENTRIES_INIT_LIMIT[g_memory_pool_slot_entries_size_level][t][s]);
				u32_t& _gcount = m_tables[t][s]->count;
				//purge exceed count first
				while (_gcount > max_n) {
#ifdef _NETP_DEBUG
					NETP_ASSERT(m_tables[t][s]->count != 0);
					++___netp_global_dealloc;
#endif
					std::free((void*)(m_tables[t][s]->ptr[--_gcount]));
				}
				u8_t* __ptr = (u8_t*)(std::realloc(m_tables[t][s], sizeof(table_slot_t) + sizeof(u8_t*) * max_n));
				m_tables[t][s] = (table_slot_t*)__ptr;
				m_tables[t][s]->max = max_n;
				m_tables[t][s]->ptr = (u8_t**)(__ptr + sizeof(table_slot_t));
			}
		}
	}

	u32_t allocator_with_block_pool_manager::commit(u8_t t, u8_t s, table_slot_t* tst, u32_t commit_count) {
#ifdef _NETP_DEBUG
		NETP_ASSERT( commit_count <= tst->count );
#endif
		lock_guard<spin_mutex> lg(m_table_slots_mtx[t][s]);
		u32_t& _gcount = m_tables[t][s]->count;
		u32_t& _tcount = tst->count;
		u32_t commited = 0;
		if ( (_gcount < (m_tables[t][s]->max)) && (commited < commit_count) ) {
			m_tables[t][s]->ptr[_gcount++] = tst->ptr[--_tcount];
			++commited;
		}
		return commited;
	}

	u32_t allocator_with_block_pool_manager::borrow(u8_t t, u8_t s, table_slot_t* tst, u32_t borrow_count) {
#ifdef _NETP_DEBUG
		NETP_ASSERT((tst->count ==0) && (tst->max > 0) );
#endif

		lock_guard<spin_mutex> lg(m_table_slots_mtx[t][s]);
		u32_t& _gcount = m_tables[t][s]->count;
		u32_t& _tcount = tst->count;
		while ((_gcount > 0) && (_tcount < borrow_count)) {
			tst->ptr[_tcount++] = m_tables[t][s]->ptr[--_gcount];
		}
		return _tcount;
	}
}