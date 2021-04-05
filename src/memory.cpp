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
const static size_t TABLE_5[T4] = {
};
const static size_t TABLE_5[T5] = {
};
const static size_t TABLE_5[T6] = {
	//end: 8192+8192=16384, 1<<14
	//size slot: TABLE_UPBOUND[T5] + (1<<(4+5))*((slot+1))
};
const static size_t TABLE_5[T7] = {
	//end: 32768, 1<<15
	//size slot: TABLE_UPBOUND[T6] + (1<<(4+7))*(slot+1)
};
const static size_t TABLE_5[T8] = {
	//end: 32768+32768, 1<<16
	//size slot: TABLE_UPBOUND[T7] + (1<<(4+8))*(slot+1)
};
const static size_t TABLE_5[T9] = {
	//end: 128k, 1<<17
	//size slot: TABLE_UPBOUND[T8] + (1<<(4+9))*(slot+1)
};
const static size_t TABLE_5[T10] = {
	//end: 256k, 1<<18
	//size slot: TABLE_UPBOUND[T9] + (1<<(4+10))*(slot+1)
};
const static size_t TABLE_5[T11] = {
	//end: 512k, 1<<19
	//size slot: TABLE_UPBOUND[T10] + (1<<(4+11))*(slot+1)
};
const static size_t TABLE_5[T12] = {
	//end: 1024k, 1<<20
	//size slot: TABLE_UPBOUND[T11] + (1<<(4+12))*(slot+1)
};
const static size_t TABLE_5[T13] = {
	//end: 2M, 1<<21
	//size slot: TABLE_UPBOUND[T12] + (1<<(4+13))*(slot+1)
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
		(__NETP_MEMORY_POOL_INIT_FACTOR) * 2048, //256 Byte
		(__NETP_MEMORY_POOL_INIT_FACTOR) * 1024, //512 Byte
		(__NETP_MEMORY_POOL_INIT_FACTOR) * 1024,//1k
		(__NETP_MEMORY_POOL_INIT_FACTOR) * 1024,//2k
		(__NETP_MEMORY_POOL_INIT_FACTOR) * 256,//4k
		(__NETP_MEMORY_POOL_INIT_FACTOR) * 128,//8k
		(__NETP_MEMORY_POOL_INIT_FACTOR) * 64,//16k
		(__NETP_MEMORY_POOL_INIT_FACTOR) * 32,//32k
		(__NETP_MEMORY_POOL_INIT_FACTOR) * 32,//64k
		(__NETP_MEMORY_POOL_INIT_FACTOR) * 16,//128k
		(__NETP_MEMORY_POOL_INIT_FACTOR) * 8,//256k
		(__NETP_MEMORY_POOL_INIT_FACTOR) * 4,//512k
		(__NETP_MEMORY_POOL_INIT_FACTOR) * 4,//1024k
		(__NETP_MEMORY_POOL_INIT_FACTOR) * 2 //2M
	};

	const static size_t TABLE_BOUND[TABLE::T_COUNT+1] = {
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
		128 + 256 + 512 + 1024 + 2048 + 4096 + 8192 + 16384 + 32768 + 65536 + 131072 + 262144 + 524288 + 1048576,//
	};

	__NETP_FORCE_INLINE void calc_SIZE_by_TABLE_SLOT(size_t& size, u8_t table, u8_t slot) {
		size = TABLE_BOUND[table] + (size_t(1)<<(size_t(table) + 4)) * ((size_t(slot) + 1));
	}

	__NETP_FORCE_INLINE void calc_TABLE_SLOT(size_t& size, u8_t& table, u8_t& slot) {
		for (u8_t t = 1; t < (TABLE::T_COUNT+1); ++t) {
			if (size <= TABLE_BOUND[t]) {
				table = (t);
				--table;

				size -= TABLE_BOUND[table];
				slot = u8_t(size >> (table + 4));
				(size % (size_t(1) << (size_t(table) + 4))) == 0 ? --slot : 0;
				size = TABLE_BOUND[table] + (size_t(1) << ((size_t(table) + 4))) * ((size_t(slot) + 1));
				break;
			}
		}
	}

	void pool_align_allocator::allocate_table_slot(table_slot_t* tst, size_t item_max) {
		tst->max = item_max;
		tst->count = 0;
		tst->ptr = (u8_t**)netp::aligned_malloc(sizeof(u8_t*) * (item_max), NETP_DEFAULT_ALIGN);
	}
	void pool_align_allocator::deallocate_table_slot(table_slot_t* tst) {
		tst->max = 0;
		tst->count = 0;
		netp::aligned_free((u8_t**)tst->ptr);
	}

	void pool_align_allocator::preallocate_table_slot_item(table_slot_t* tst, u8_t t, u8_t slot, size_t item_count) {
		u8_t** ptr = (u8_t**)netp::aligned_malloc(sizeof(u8_t*) * (item_count), NETP_DEFAULT_ALIGN);
		size_t i;
		size_t size = 0;
		calc_SIZE_by_TABLE_SLOT(size, t, slot);
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
		for (u8_t t = 0; t < TABLE::T_COUNT; ++t) {
			for (u8_t s = 0; s < SLOT_MAX; ++s) {
				m_tables[t][s] = (table_slot_t*)netp::aligned_malloc(sizeof(table_slot_t), NETP_DEFAULT_ALIGN);
				allocate_table_slot( m_tables[t][s], TABLE_SLOT_ENTRIES_INIT_LIMIT[t]);

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
				deallocate_table_slot(m_tables[t][s]);
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
		calc_TABLE_SLOT(size, t, s );
		NETP_ASSERT(t <= T_COUNT);
		if (NETP_LIKELY(t != T_COUNT)) {
			NETP_ASSERT(s != size_t(-1));
			NETP_ASSERT(SLOT_MAX > s);
			table_slot_t* tst = (m_tables[t][s]);

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
		}

		//aligned_malloc has a 4 bytes h
		uptr = (u8_t*)netp::aligned_malloc((size), align_size);
		if (NETP_LIKELY(uptr != 0)) {
			*(uptr - 2) = ((t << 4) | (s & 0xf));
		}
		return uptr;
	}

	void pool_align_allocator::free(void* ptr) {
		if (NETP_UNLIKELY(ptr == nullptr)) { return; }

		u8_t& slot_ = *((u8_t*)ptr - 2);
		u8_t t = (slot_ >> 4);
		u8_t s = (slot_ & 0xf);

		//u8_t slot = (slot_ & 0xf);
		if (NETP_UNLIKELY(t == T_COUNT)) {
			return netp::aligned_free(ptr);
		}

		NETP_ASSERT(t < TABLE::T_COUNT);
		NETP_ASSERT(SLOT_MAX > s);
		table_slot_t* tst = (m_tables[t][s]);

		if (( tst->count < tst->max)) {
			tst->ptr[tst->count++] = (u8_t*)ptr;

			if (tst->count == tst->max) {
				global_pool_align_allocator::instance()->commit(t, s, tst);
			}
			return;
		}

		netp::aligned_free(ptr);
	}


	//alloc, then copy
	void* pool_align_allocator::realloc(void* ptr, size_t size, size_t align_size) {
		//align_alloc first

		if (ptr == 0) { 
			return malloc(size, align_size);
		}

		u8_t* uptr=0;
		u8_t t = T_COUNT;
		u8_t slot = u8_t(-1);
		calc_TABLE_SLOT(size, t, slot);
		NETP_ASSERT(t <= T_COUNT);

		if (NETP_LIKELY(t != T_COUNT)) {
			//we would never get old ptr, cuz old ptr have not yet been returned
			NETP_ASSERT(slot != size_t(-1));
			NETP_ASSERT(SLOT_MAX > slot);
			table_slot_t* tst= (m_tables[t][slot]);
			if (tst->count) {
				uptr = tst->ptr[--tst->count];
			}
		}

		if (uptr == 0) {
			//not hit from cache
			uptr = (u8_t*)malloc(size, align_size);

			if (NETP_UNLIKELY(uptr == 0)) {
				return uptr;
			}
		}

		u8_t& old_t_slot = *((u8_t*)ptr - 2);
		u8_t old_t = (old_t_slot >> 4);
		u8_t old_slot = (old_t_slot & 0xf);
		size_t old_size=0;
		calc_SIZE_by_TABLE_SLOT(old_size,old_t, old_slot);
		NETP_ASSERT(old_size > 0);
		//do copy
		std::memcpy(uptr, ptr, old_size);

		//free ptr
		free(ptr);

		return uptr;
	}

	global_pool_align_allocator::global_pool_align_allocator():
		pool_align_allocator(false)
	{}

	global_pool_align_allocator::~global_pool_align_allocator() {
		for (size_t t = 0; t < sizeof(m_tables) / sizeof(m_tables[0]); ++t) {
			for (size_t s = 0; s < SLOT_MAX; ++s) {
				lock_guard<spin_mutex> lg(m_table_slots_mtx[t][s]);
				size_t& _gcount = m_tables[t][s]->count;
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
				m_tables[t][s]->max += TABLE_SLOT_ENTRIES_INIT_LIMIT[t];
				m_tables[t][s]->ptr = (u8_t**) (netp::aligned_realloc( m_tables[t][s]->ptr, sizeof(u8_t*) * m_tables[t][s]->max, NETP_DEFAULT_ALIGN));
			}
		}
	}

	void global_pool_align_allocator::decre_thread_count() {
		for (size_t t = 0; t < sizeof(m_tables) / sizeof(m_tables[0]); ++t) {
			for (size_t s = 0; s < SLOT_MAX; ++s) {
				lock_guard<spin_mutex> lg(m_table_slots_mtx[t][s]);
				NETP_ASSERT(m_tables[t][s]->max >= TABLE_SLOT_ENTRIES_INIT_LIMIT[t]);
				m_tables[t][s]->max -= TABLE_SLOT_ENTRIES_INIT_LIMIT[t];
			}
		}
	}

	size_t global_pool_align_allocator::commit(u8_t t, u8_t s, table_slot_t* tst) {
		lock_guard<spin_mutex> lg(m_table_slots_mtx[t][s]);
		size_t& _gcount = m_tables[t][s]->count;
		size_t& _tcount = tst->count;
		size_t tt = 0;
		if ( (_gcount < (m_tables[t][s]->max - 1)) && ( _tcount > ( (tst->max)>>1) ) ) {
			m_tables[t][s]->ptr[_gcount++] = tst->ptr[--_tcount];
			++tt;
		}
		return tt;
	}

	size_t global_pool_align_allocator::borrow(u8_t t, u8_t s, table_slot_t* tst) {
		NETP_ASSERT( tst->count ==0 );
		NETP_ASSERT(tst->max > 0);
		lock_guard<spin_mutex> lg(m_table_slots_mtx[t][s]);
		size_t& _gcount = m_tables[t][s]->count;
		size_t& _tcount = tst->count;
		while ((_gcount > 0) && (_tcount < ((tst->max) >> 1))) {
			tst->ptr[_tcount++] = m_tables[t][s]->ptr[--_gcount];
		}
		return _tcount;
	}
}