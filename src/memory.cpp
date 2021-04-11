#include <netp/memory.hpp>

namespace netp {

/*
*  Tn = T(n-1) + (slot+1) * (1<<(n+4))
* 
const static size_t TABLE_1[T0] = {
	16 * 1,
	16 * 2,
	16 * ...,
	16 * 8,
	//end: 128 16*8
	//slot size: (slot+1)*(1<<(0+4))
};

const static size_t TABLE_2[T1] = {
	16 * 8 + 32 * 1,
	16 * 8 + 32 * 2,
	16 * 8 + 32 * ...,
	16 * 8 + 32 * 8,
	//end: 128+256=384
	//slot size: TABLE_UPBOUND[T0] + (1<<(1+4))*(slot+1)
};

const static size_t TABLE_3[T2] = {
	16 * 8 + 32 * 8 + 64 * 1,
	16 * 8 + 32 * 8 + 64 * 2,
	16 * 8 + 32 * 8 + 64 * ..,
	16 * 8 + 32 * 8 + 64 * 8
	//end: 128+256+512=896
	//size slot: TABLE_UPBOUND[T1] + (1<<(2+4))*((slot+1))
};
const static size_t TABLE_4[T3] = {
	16 * 8 + 32 * 8 + 64 * 8 + 128*1,
	16 * 8 + 32 * 8 + 64 * 8 + 128*2,
	16 * 8 + 32 * 8 + 64 * 8 + 128*...,
	16 * 8 + 32 * 8 + 64 * 8 + 128*8
	//end for 128+256+512+1024=1920
	//size slot: TABLE_UPBOUND[T2] + (1<<(4+2))*((slot+1))
};
*/

#define ___FACTOR (1)

#ifdef _NETP_AM64
#define __NETP_MEMORY_POOL_INIT_FACTOR (___FACTOR)
#else
#define __NETP_MEMORY_POOL_INIT_FACTOR (___FACTOR)
#endif

//object pool does not suit for large memory gap objects
	const static u32_t TABLE_SLOT_ENTRIES_INIT_LIMIT[TABLE::T_COUNT] = {
		(__NETP_MEMORY_POOL_INIT_FACTOR) * 2048, //256-128 Byte
		(__NETP_MEMORY_POOL_INIT_FACTOR) * 1024, //512-128 Byte
		(__NETP_MEMORY_POOL_INIT_FACTOR) * 1024,//1k-128
		(__NETP_MEMORY_POOL_INIT_FACTOR) * 1024,//2k-128
		(__NETP_MEMORY_POOL_INIT_FACTOR) * 128,//4k-128
		(__NETP_MEMORY_POOL_INIT_FACTOR) * 64,//8k-128
		(__NETP_MEMORY_POOL_INIT_FACTOR) * 32,//16k-128
		(__NETP_MEMORY_POOL_INIT_FACTOR) * 16,//32k-128
		(__NETP_MEMORY_POOL_INIT_FACTOR) * 8,//64k-128
		(__NETP_MEMORY_POOL_INIT_FACTOR) * 8,//128k-128
		(__NETP_MEMORY_POOL_INIT_FACTOR) * 4,//256k-128
		(__NETP_MEMORY_POOL_INIT_FACTOR) * 4,//512k-128
		(__NETP_MEMORY_POOL_INIT_FACTOR) * 2,//1024k-128
		(__NETP_MEMORY_POOL_INIT_FACTOR) * 2 //2M-128
	};

	 __NETP_TLS const static u32_t TABLE_BOUND[TABLE::T_COUNT+1] = {
		0,
		128, //
		128 + 256, //
		128 + 256 + 512,//
		128 + 256 + 512 + 1024,//
		128 + 256 + 512 + 1024 + 2048,//
		128 + 256 + 512 + 1024 + 2048 + 4096,//
		128 + 256 + 512 + 1024 + 2048 + 4096 + 8192,//
		128 + 256 + 512 + 1024 + 2048 + 4096 + 8192 + 16384,//
		128 + 256 + 512 + 1024 + 2048 + 4096 + 8192 + 16384 + 32768,//
		128 + 256 + 512 + 1024 + 2048 + 4096 + 8192 + 16384 + 32768 + 65536,//
		128 + 256 + 512 + 1024 + 2048 + 4096 + 8192 + 16384 + 32768 + 65536 + 131072,//
		128 + 256 + 512 + 1024 + 2048 + 4096 + 8192 + 16384 + 32768 + 65536 + 131072 + 262144,//
		128 + 256 + 512 + 1024 + 2048 + 4096 + 8192 + 16384 + 32768 + 65536 + 131072 + 262144 + 524288,//
		128 + 256 + 512 + 1024 + 2048 + 4096 + 8192 + 16384 + 32768 + 65536 + 131072 + 262144 + 524288 + 1048576//
	};

	#define calc_SIZE_by_TABLE_SLOT(t, s) (TABLE_BOUND[t] + ((1ULL<<(t + 4))) * ((s + 1)))

	#define calc_TABLE(size, t) do { \
		for (u8_t ti = 1; ti < (TABLE::T_COUNT+1); ++ti) { \
			if (size <= TABLE_BOUND[ti]) { \
				t = (--ti); \
				break; \
			} \
		} \
	}while(false);\

#define calc_TABLE_(size,t) do { \
	size_t div128 = (size>>7); \
	switch (div128) { \
	case 0: \
	{ \
		t = T0; \
	} \
	break; \
	case 1: \
	{ \
		t = T1; \
		(size % 128) == 0 ? --t : 0; \
	} \
	break; \
	case 2: \
	{ \
		t = T1; \
	} \
	break; \
	case 3: \
	{ \
		t = T2; \
		(size % 128) == 0 ? --t : 0; \
	} \
	break; \
	case 4:case 5:case 6: \
	{ \
		t = T2; \
	} \
	break; \
	case 7: \
	{ \
		t = T3; \
		(size % 128) == 0 ? --t : 0; \
	} \
	break; \
	case 8:case 9:case 10: 	case 11:case 12:case 13: case 14: \
	{ \
		t = T3; \
	} \
	break; \
	case 15: \
	{ \
		/*1920*/ \
		t = T4; \
		(size % 128) == 0 ? --t : 0; \
	} \
	break; \
	default: \
	{ \
		for (u8_t ti = T5; ti < (TABLE::T_COUNT + 1); ++ti) { \
			if (size <= TABLE_BOUND[ti]) { \
				t = (--ti); \
				break; \
			} \
		} \
	} \
	break;\
	} \
}while(false);\

	void pool_align_allocator::preallocate_table_slot_item(table_slot_t* tst, u8_t t, u8_t slot, size_t item_count) {
		u8_t** ptr = (u8_t**)netp::aligned_malloc(sizeof(u8_t*) * (item_count), NETP_DEFAULT_ALIGN);
		size_t size = calc_SIZE_by_TABLE_SLOT(t, slot);
		size_t i;
		for (i = 0; i < (item_count); ++i) {
			ptr[i] = (u8_t*) pool_align_allocator::malloc(size, NETP_DEFAULT_ALIGN);
		}
		for (int j = 0; j < i; ++j) {
			pool_align_allocator::free(ptr[j]);
		}
		netp::aligned_free(ptr);
	}

	void pool_align_allocator::deallocate_table_slot_item(table_slot_t* tst) {
		while (tst->count) {//stop at 0
			netp::aligned_free( (tst->ptr[--tst->count]) );
		}
	}

	void pool_align_allocator::init( bool preallocate ) {
		static_assert((sizeof(table_slot_t) % NETP_DEFAULT_ALIGN) == 0, "check table slot size");
		for (u8_t t = 0; t < TABLE::T_COUNT; ++t) {
			for (u8_t s = 0; s < SLOT_MAX; ++s) {
				u8_t* __ptr = (u8_t*) netp::aligned_malloc(sizeof(table_slot_t) + (sizeof(u8_t*) * TABLE_SLOT_ENTRIES_INIT_LIMIT[t]), NETP_DEFAULT_ALIGN);
				m_tables[t][s] = (table_slot_t*)__ptr;
				m_tables[t][s]->max = TABLE_SLOT_ENTRIES_INIT_LIMIT[t];
				m_tables[t][s]->count = 0;
				m_tables[t][s]->ptr = (u8_t**)(__ptr + (sizeof(table_slot_t)));

				if (preallocate && (t<T5 /*32k*/) ) {
					preallocate_table_slot_item(m_tables[t][s], t, s, (TABLE_SLOT_ENTRIES_INIT_LIMIT[t]>>1) );
				}
			}
		}
	}

	void pool_align_allocator::deinit() {
		for (u8_t t = 0; t < TABLE::T_COUNT; ++t) {
			for (u8_t s = 0; s < SLOT_MAX; ++s) {
				deallocate_table_slot_item(m_tables[t][s]);
				//deallocate_table_slot(m_tables[t][s]);
				netp::aligned_free(m_tables[t][s]);
			}
		}
	}

	pool_align_allocator::pool_align_allocator( bool preallocate ) {
		init(preallocate);
	}

	pool_align_allocator::~pool_align_allocator() {
		deinit();
	}

	void* pool_align_allocator::malloc(size_t size, size_t align_size) {
		u8_t* uptr;
		u8_t t = T_COUNT;
		u8_t s = u8_t(-1);
		calc_TABLE(size,t);

		if (NETP_LIKELY(t<T_COUNT)) {
			size -= TABLE_BOUND[t];
			s = u8_t(size >> (t + 4));
			(size % ((1ULL << (t + 4)))) == 0 ? --s : 0;

			//NETP_ASSERT(s < SLOT_MAX);
			table_slot_t*& tst = (m_tables[t][s]);

			//fast path
__fast_path:
			if (tst->count) {
				return (tst->ptr[--tst->count]);
			}
			//borrow
			size_t c = global_pool_align_allocator::instance()->borrow(t, s, tst);
			NETP_ASSERT(c == tst->count);
			if (c != 0) {
				goto __fast_path;	
			}

			//update size for new malloc
			size = calc_SIZE_by_TABLE_SLOT(t, s);
		}

		//aligned_malloc has a 8 bytes h
		uptr = (u8_t*)netp::aligned_malloc((size), align_size);
		if (NETP_LIKELY(uptr != 0)) {
			*(uptr - 2) = ((t << 4) | (s & 0xf));
		}
		return uptr;
	}

	void pool_align_allocator::free(void* ptr) {
		if (NETP_UNLIKELY(ptr == 0)) { return; }

		u8_t t = *((u8_t*)ptr - 2);
		u8_t s = (t & 0xf);
		t = (t>>4);

		//u8_t slot = (slot_ & 0xf);
		if (NETP_LIKELY(t < T_COUNT)) {
			NETP_ASSERT(s < SLOT_MAX);
			table_slot_t*& tst = (m_tables[t][s]);
			if ((tst->count < tst->max)) {
				tst->ptr[tst->count++] = (u8_t*)ptr;
				if (tst->count == tst->max) {
					global_pool_align_allocator::instance()->commit(t, s, tst);
				}
				return;
			}
		}

		netp::aligned_free(ptr);
	}


	//alloc, then copy
	void* pool_align_allocator::realloc(void* ptr, size_t size, size_t align_size) {
		//align_alloc first
		NETP_ASSERT(ptr != 0);
		u8_t old_t = *((u8_t*)ptr - 2);
		u8_t old_s = (old_t & 0xf);
		old_t = (old_t >> 4);

		if (NETP_LIKELY(old_t < T_COUNT)) {
			u8_t* newptr = (u8_t*)malloc(size, align_size);
			//we would never get old ptr, cuz old ptr have not yet been returned
			NETP_ASSERT(newptr != ptr);

			size_t old_size = calc_SIZE_by_TABLE_SLOT(old_t, old_s);
			NETP_ASSERT(old_size > 0);
			//do copy
			std::memcpy(newptr, ptr, old_size);

			//free ptr
			free(ptr);
			return newptr;
		}

		u8_t new_T = T_COUNT;
		calc_TABLE(size, new_T);

		if ( NETP_LIKELY(new_T == T_COUNT)) {
			u8_t* newptr = (u8_t*)netp::aligned_realloc(ptr, size, align_size);
			if (NETP_LIKELY(newptr != 0)) {
				*(newptr - 2) = ((T_COUNT << 4) | (0xf));
			}
			return newptr;
		} else {
			//smaller t, smaller size
			//in case of new size <old_size 
			u8_t* newptr = (u8_t*) malloc(size,align_size);
			std::memcpy(newptr, ptr, size);

			netp::aligned_free(ptr);
			return newptr;
		}
	}

	
	global_pool_align_allocator::global_pool_align_allocator():
		pool_align_allocator(false)
	{}

	global_pool_align_allocator::~global_pool_align_allocator() {
		for (size_t t = 0; t < sizeof(m_tables) / sizeof(m_tables[0]); ++t) {
			for (size_t s = 0; s < SLOT_MAX; ++s) {
				lock_guard<spin_mutex> lg(m_table_slots_mtx[t][s]);
				u32_t& _gcount = m_tables[t][s]->count;
				while ( _gcount>0) {
					netp::aligned_free(m_tables[t][s]->ptr[ --_gcount ]);
				}
			}
		}
	}
	

	//default: one thread -- main thread
	void global_pool_align_allocator::incre_thread_count() {
		for (size_t t = 0; t < sizeof(m_tables) / sizeof(m_tables[0]); ++t) {
			for (size_t s = 0; s < SLOT_MAX; ++s) {
				lock_guard<spin_mutex> lg(m_table_slots_mtx[t][s] );
				u32_t max_n = m_tables[t][s]->max + TABLE_SLOT_ENTRIES_INIT_LIMIT[t];
				u8_t* __ptr = (u8_t*)(netp::aligned_realloc(m_tables[t][s], sizeof(table_slot_t) + sizeof(u8_t*) * max_n, NETP_DEFAULT_ALIGN));
				m_tables[t][s] = (table_slot_t*)__ptr;
				m_tables[t][s]->max = max_n;
				m_tables[t][s]->ptr = (u8_t**) (__ptr + sizeof(table_slot_t));
			}
		}
	}

	void global_pool_align_allocator::decre_thread_count() {
		for (size_t t = 0; t < sizeof(m_tables) / sizeof(m_tables[0]); ++t) {
			for (size_t s = 0; s < SLOT_MAX; ++s) {
				lock_guard<spin_mutex> lg(m_table_slots_mtx[t][s]);
				NETP_ASSERT(m_tables[t][s]->max >= TABLE_SLOT_ENTRIES_INIT_LIMIT[t]);
				u32_t max_n = m_tables[t][s]->max - TABLE_SLOT_ENTRIES_INIT_LIMIT[t];
				u32_t& _gcount = m_tables[t][s]->count;
				//purge exceed count first
				while (_gcount > max_n) {
					netp::aligned_free(m_tables[t][s]->ptr[--_gcount]);
				}
				u8_t* __ptr = (u8_t*)(netp::aligned_realloc(m_tables[t][s], sizeof(table_slot_t) + sizeof(u8_t*) * max_n, NETP_DEFAULT_ALIGN));
				m_tables[t][s] = (table_slot_t*)__ptr;
				m_tables[t][s]->max = max_n;
				m_tables[t][s]->ptr = (u8_t**)(__ptr + sizeof(table_slot_t));
			}
		}
	}

	u32_t global_pool_align_allocator::commit(u8_t t, u8_t s, table_slot_t* tst) {
		lock_guard<spin_mutex> lg(m_table_slots_mtx[t][s]);
		u32_t& _gcount = m_tables[t][s]->count;
		u32_t& _tcount = tst->count;
		u32_t tt = 0;
		if ( (_gcount < (m_tables[t][s]->max - 1)) && ( _tcount > ( (tst->max)>>1) ) ) {
			m_tables[t][s]->ptr[_gcount++] = tst->ptr[--_tcount];
			++tt;
		}
		return tt;
	}

	u32_t global_pool_align_allocator::borrow(u8_t t, u8_t s, table_slot_t* tst) {
		NETP_ASSERT( tst->count ==0 );
		NETP_ASSERT(tst->max > 0);
		lock_guard<spin_mutex> lg(m_table_slots_mtx[t][s]);
		u32_t& _gcount = m_tables[t][s]->count;
		u32_t& _tcount = tst->count;
		while ((_gcount > 0) && (_tcount < ((tst->max) >> 1))) {
			tst->ptr[_tcount++] = m_tables[t][s]->ptr[--_gcount];
		}
		return _tcount;
	}
}