#include <netp/memory.hpp>

namespace netp {

	//@TODO: implement a global pool to distribute object across threads
/*
const static size_t TABLE_1[T0] = {
	16 * 1,
	16 * 2,
	16 * ...,
	16 * 16,
	//end: 256 16*16
	//slot size: (slot+1)*(16)
};

const static size_t TABLE_2[T1] = {
	32 * 8 + 32 * 1,
	32 * 8 + 32 * 2,
	32 * 8 + 32 * ...,
	32 * 8 + 32 * 8,
	//end: 256+256=512, 1<<9
	//slot size: TABLE_UPBOUND[T0] + (1<<(4+1))*(slot+1)
};

const static size_t TABLE_3[T2] = {
	32 * 8 + 32 * 8 + 64 * 1,
	32 * 8 + 32 * 8 + 64 * 2,
	32 * 8 + 32 * 8 + 64 * ..,
	32 * 8 + 32 * 8 + 64 * 8
	//end: 256+256+512=1024, 1<<10
	//size slot: TABLE_UPBOUND[T1] + (1<<(4+2))*((slot+1))
};
const static size_t TABLE_4[T3] = {
	32 * 8 + 32 * 8 + 64 * 8 + 128*1,
	32 * 8 + 32 * 8 + 64 * 8 + 128*2,
	32 * 8 + 32 * 8 + 64 * 8 + 128*...,
	32 * 8 + 32 * 8 + 64 * 8 + 128*8
	//end for 256+256+512+1024=2048, 1<<11
	//size slot: TABLE_UPBOUND[T2] + (1<<(4+3))*((slot+1))
};
const static size_t TABLE_5[T4] = {
	32 * 8 + 32 * 8 + 64 * 8 + 128*8 + 256*1,
	32 * 8 + 32 * 8 + 64 * 8 + 128*8 + 256*2,
	32 * 8 + 32 * 8 + 64 * 8 + 128*8 + 256*...,
	32 * 8 + 32 * 8 + 64 * 8 + 128*8 + 256*8,
	//end: 256+256+512+1024+2048=4096, 1<<12
	//size slot: TABLE_UPBOUND[T3] + (1<<(4+4))*((slot+1))
};
const static size_t TABLE_5[T5] = {
	32 * 8 + 32 * 8 + 64 * 8 + 128*8 + 256*8+512*1,
	32 * 8 + 32 * 8 + 64 * 8 + 128*8 + 256*8+512*2,
	32 * 8 + 32 * 8 + 64 * 8 + 128*8 + 256*8+512*...
	32 * 8 + 32 * 8 + 64 * 8 + 128*8 + 256*8+512*8
	//end: 8192, 1<<13
	//size slot: TABLE_UPBOUND[T4] + (1<<(4+5))*((slot+1))
};
const static size_t TABLE_5[T6] = {
	//end: 8192+8192=16384, 1<<14
	//size slot: TABLE_UPBOUND[T5] + (1<<(4+6))*((slot+1))
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
const static size_t TABLE_5[T14] = {
	//end: 4M, 1<<22
	//size slot: TABLE_UPBOUND[T13] + (1<<(4+13))*(slot+1)
};
const static size_t TABLE_5[T15] = {
	//end: 8M, 1<<23
	//size slot: TABLE_UPBOUND[T14] + (1<<(4+13))*(slot+1)
};

*/
#define SLOT_MAX(t) (((t) == 0) ? u8_t(16):u8_t(8))

#define ___FACTOR (1)

#ifdef _NETP_AM64
#define __NETP_MEMORY_POOL_INIT_FACTOR (___FACTOR)
#else
#define __NETP_MEMORY_POOL_INIT_FACTOR (___FACTOR)
#endif

//object pool does not suit for large memory gap objects
	const static u32_t TABLE_SLOT_ENTRIES_INIT_LIMIT[TABLE::T_COUNT] = {
		(__NETP_MEMORY_POOL_INIT_FACTOR) * 4096, //256 Byte
		(__NETP_MEMORY_POOL_INIT_FACTOR) * 2048, //512 Byte
		(__NETP_MEMORY_POOL_INIT_FACTOR) * 2048,//1k
		(__NETP_MEMORY_POOL_INIT_FACTOR) * 2048,//2k
		(__NETP_MEMORY_POOL_INIT_FACTOR) * 512,//4k
		(__NETP_MEMORY_POOL_INIT_FACTOR) * 256,//8k
		(__NETP_MEMORY_POOL_INIT_FACTOR) * 128,//16k
		(__NETP_MEMORY_POOL_INIT_FACTOR) * 64,//32k
		(__NETP_MEMORY_POOL_INIT_FACTOR) * 32,//64k
		(__NETP_MEMORY_POOL_INIT_FACTOR) * 16,//128k
		(__NETP_MEMORY_POOL_INIT_FACTOR) * 8,//256k
		(__NETP_MEMORY_POOL_INIT_FACTOR) * 4,//512k
		(__NETP_MEMORY_POOL_INIT_FACTOR) * 4,//1024k
		(__NETP_MEMORY_POOL_INIT_FACTOR) * 2, //2M
		(__NETP_MEMORY_POOL_INIT_FACTOR) * 1, //4M
		(__NETP_MEMORY_POOL_INIT_FACTOR) * 1 //8M
	};

	const static size_t TABLE_BOUND[TABLE::T_COUNT] = {
		1 << 8, //256
		1 << 9, //512
		1 << 10,//1k
		1 << 11,//2k
		1 << 12,//4k
		1 << 13,//8k
		1 << 14,//16k
		1 << 15,//32k
		1 << 16,//64k
		1 << 17,//128k
		1 << 18,//256k
		1 << 19,//512k
		1 << 20,//1024k
		1 << 21, //2M
		1 << 22, //4M
		1 << 23 //8M
	};

	__NETP_FORCE_INLINE void calc_SIZE_by_TABLE_SLOT(size_t& size, u8_t table, u8_t slot) {
		if (table == TABLE::T0) {
			size = ((size_t(slot)+1)<<4);
			return;
		}
		size = TABLE_BOUND[table-1] + (size_t(1)<<(size_t(table) + 4)) * ((size_t(slot) + 1));
	}

	__NETP_FORCE_INLINE void calc_TABLE_SLOT(size_t& size, u8_t& table, u8_t& slot) {
		for (u8_t t = 0; t < (TABLE::T_COUNT); ++t) {
			if (size <= TABLE_BOUND[t]) {
				table = (t);
				break;
			}
		}

		if(table ==TABLE::T0)
		{
			slot = u8_t(size >> 4);
			(size % 16) == 0 ? --slot : 0;
			size = ((size_t(slot)+1)<<4);
			return;
		}

		size -= TABLE_BOUND[table-1];
		slot = u8_t(size >> (table + 4));
		(size % (size_t(1) << (size_t(table) + 4) )) == 0 ? --slot : 0;
		size = TABLE_BOUND[table-1] + (size_t(1) << ((size_t(table) + 4)))*((size_t(slot)+1));
	}

	void pool_align_allocator::init_table_slot(u8_t t, u8_t slot, std::vector<void*>& slotv, size_t capacity) {
		slotv.reserve(capacity);

		//if (t > TABLE::T4) { 
		//	return;
		//}

		for (size_t i = 0; i < (capacity >> 1); ++i) {
			size_t size = 0;
			calc_SIZE_by_TABLE_SLOT(size, t, slot);
			pool_align_allocator::free(pool_align_allocator::malloc(size, NETP_DEFAULT_ALIGN));
		}
	}

	void pool_align_allocator::init_table(u8_t t, std::vector<void*>* table) {
		const u8_t slot_max = SLOT_MAX(t);
		for (u8_t slot = 0; slot < slot_max; ++slot) {
			init_table_slot(t, slot, *(table + slot), m_entries_limit[t]);
		}
	}

	void pool_align_allocator::free_table_slot(std::vector<void*>& slot) {
		while (slot.size()) {
			netp::aligned_free(slot.back());
			slot.pop_back();
		}
	}

	void pool_align_allocator::free_table(u8_t t, std::vector<void*>* table) {
		const u8_t slot_max = SLOT_MAX(t);
		for (u8_t i = 0; i < slot_max; ++i) {
			free_table_slot(*(table + i));
		}
	}

	pool_align_allocator::pool_align_allocator() {
		init();
	}

	pool_align_allocator::~pool_align_allocator() {
		deinit();
	}

	void pool_align_allocator::init() {
		for (u8_t t = 0; t < TABLE::T_COUNT; ++t) {
			m_tables[t] = new std::vector<void*>[SLOT_MAX(t)];
			set_slot_entries_limit(t, TABLE_SLOT_ENTRIES_INIT_LIMIT[t]);
			init_table(t, m_tables[t]);
		}
	}

	void pool_align_allocator::deinit() {
		for (u8_t t = 0; t < TABLE::T_COUNT; ++t) {
			free_table(t,m_tables[t]);
			delete[] m_tables[t];
		}
	}

	void* pool_align_allocator::malloc(size_t size, size_t align_size) {
		u8_t* uptr;
		u8_t t = T_COUNT;
		u8_t slot = u8_t(-1);
		calc_TABLE_SLOT(size, t, slot );
		NETP_ASSERT(t <= T_COUNT);
		if (NETP_LIKELY(t != T_COUNT)) {
			NETP_ASSERT(slot != size_t(-1));
			NETP_ASSERT(SLOT_MAX(t) > slot);
			std::vector<void*>& table_slot = *(m_tables[t] + slot);

			if (table_slot.size()) {
				uptr = (u8_t*)table_slot.back();
				table_slot.pop_back();
				return uptr;
			}
		}

		//aligned_malloc has a 4 bytes h
		uptr = (u8_t*)netp::aligned_malloc((size), align_size);
		if (NETP_LIKELY(uptr != 0)) {
			*(uptr - 2) = ((t << 4) | (slot & 0xf));
		}
		return uptr;
	}

	void pool_align_allocator::free(void* ptr) {
		if (NETP_UNLIKELY(ptr == nullptr)) { return; }

		u8_t& slot_ = *((u8_t*)ptr - 2);
		u8_t t = (slot_ >> 4);
		u8_t slot = (slot_ & 0xf);

		//u8_t slot = (slot_ & 0xf);
		if (NETP_UNLIKELY(t == T_COUNT)) {
			return netp::aligned_free(ptr);
		}

		NETP_ASSERT(t < TABLE::T_COUNT);
		NETP_ASSERT(SLOT_MAX(t) > slot);
		std::vector<void*>& table_slot = *(m_tables[t] + slot);

		if ((table_slot.size() < m_entries_limit[t])) {
			table_slot.push_back(ptr);
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
			NETP_ASSERT(slot != size_t(-1));
			NETP_ASSERT(SLOT_MAX(t) > slot);
			std::vector<void*>& table_slot = *(m_tables[t] + slot);
			if (table_slot.size()) {
				uptr = (u8_t*)table_slot.back();
				table_slot.pop_back();
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
}