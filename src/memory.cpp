#include <netp/memory.hpp>

namespace netp {

	/*
const static size_t TABLE_1[TABLE_SIZE::S0] = {
	16 * 1,
	16 * 2,
	16 * 3,
	16 * 4,
	16 * 5,
	16 * 6,
	16 * 7,
	16 * 8
	//end for 128 16*8
};

const static size_t TABLE_2[TABLE_SIZE::S1] = {
	[128--640
	16 * 8 + 32 * 1,
	16 * 8 + 32 * 2,
	16 * 8 + 32 * 3,
	16 * 8 + 32 * 4,
	16 * 8 + 32 * 5,
	16 * 8 + 32 * 6,
	16 * 8 + 32 * 7,
	16 * 8 + 32 * 8,
	//end for 128+256=384=48*8
};

const static size_t TABLE_3[TABLE_SIZE::S2] = {
	//2048 -- 2048+512
	16 * 8 + 32 * 8 + 64 * 1,
	16 * 8 + 32 * 8 + 64 * 2,
	16 * 8 + 32 * 8 + 64 * 3,
	16 * 8 + 32 * 8 + 64 * 4,
	16 * 8 + 32 * 8 + 64 * 5,
	16 * 8 + 32 * 8 + 64 * 6,
	16 * 8 + 32 * 8 + 64 * 7,
	16 * 8 + 32 * 8 + 64 * 8
	//end for 128+256+512=896=112*8
};
const static size_t TABLE_4[TABLE_SIZE::S3] = {
	16 * 8 + 32 * 8 + 64 * 8 + 128*1,
	16 * 8 + 32 * 8 + 64 * 8 + 128*2,
	16 * 8 + 32 * 8 + 64 * 8 + 128*3,
	16 * 8 + 32 * 8 + 64 * 8 + 128*4,
	16 * 8 + 32 * 8 + 64 * 8 + 128*5,
	16 * 8 + 32 * 8 + 64 * 8 + 128*6,
	16 * 8 + 32 * 8 + 64 * 8 + 128*7,
	16 * 8 + 32 * 8 + 64 * 8 + 128*8
	//end for 128+256+512+1024=1920=240*8
};
const static size_t TABLE_5[TABLE_SIZE::S4] = {
	16 * 8 + 32 * 8 + 64 * 8 + 128*8 + 256*1,
	16 * 8 + 32 * 8 + 64 * 8 + 128*8 + 256*2,
	16 * 8 + 32 * 8 + 64 * 8 + 128*8 + 256*3,
	16 * 8 + 32 * 8 + 64 * 8 + 128*8 + 256*4,
	16 * 8 + 32 * 8 + 64 * 8 + 128*8 + 256*5,
	16 * 8 + 32 * 8 + 64 * 8 + 128*8 + 256*6,
	16 * 8 + 32 * 8 + 64 * 8 + 128*8 + 256*7,
	16 * 8 + 32 * 8 + 64 * 8 + 128*8 + 256*8,
	//end for 128+256+512+1024+2048=3968=496*8
};
const static size_t TABLE_5[TABLE_SIZE::S5] = {
	16 * 8 + 32 * 8 + 64 * 8 + 128*8 + 256*8+512*1,
	16 * 8 + 32 * 8 + 64 * 8 + 128*8 + 256*8+512*2,
	16 * 8 + 32 * 8 + 64 * 8 + 128*8 + 256*8+512*3,
	16 * 8 + 32 * 8 + 64 * 8 + 128*8 + 256*8+512*4,
	16 * 8 + 32 * 8 + 64 * 8 + 128*8 + 256*8+512*5,
	16 * 8 + 32 * 8 + 64 * 8 + 128*8 + 256*8+512*6,
	16 * 8 + 32 * 8 + 64 * 8 + 128*8 + 256*8+512*7,
	16 * 8 + 32 * 8 + 64 * 8 + 128*8 + 256*8+512*8
	//end for 128+256+512+1024+2048+4096=8064=1008*8
};
const static size_t TABLE_5[TABLE_SIZE::S6] = {
	(16 * 8 + 32 * 8 + 64 * 8 + 128*8 + 256*8+512*8)*2
	(16 * 8 + 32 * 8 + 64 * 8 + 128*8 + 256*8+512*8)*4
	(16 * 8 + 32 * 8 + 64 * 8 + 128*8 + 256*8+512*8)*6
	(16 * 8 + 32 * 8 + 64 * 8 + 128*8 + 256*8+512*8)*8
	(16 * 8 + 32 * 8 + 64 * 8 + 128*8 + 256*8+512*8)*10
	(16 * 8 + 32 * 8 + 64 * 8 + 128*8 + 256*8+512*8)*12
	(16 * 8 + 32 * 8 + 64 * 8 + 128*8 + 256*8+512*8)*14
	(16 * 8 + 32 * 8 + 64 * 8 + 128*8 + 256*8+512*8)*16

	//end for (128+256+512+1024+2048+4096)*16=8064*16=126K=16128*8
};
const static size_t TABLE_5[TABLE_SIZE::S7] = {
	((16 * 8 + 32 * 8 + 64 * 8 + 128*8 + 256*8+512*8)*16)*2
	((16 * 8 + 32 * 8 + 64 * 8 + 128*8 + 256*8+512*8)*16)*4
	((16 * 8 + 32 * 8 + 64 * 8 + 128*8 + 256*8+512*8)*16)*6
	((16 * 8 + 32 * 8 + 64 * 8 + 128*8 + 256*8+512*8)*16)*8
	((16 * 8 + 32 * 8 + 64 * 8 + 128*8 + 256*8+512*8)*16)*10
	((16 * 8 + 32 * 8 + 64 * 8 + 128*8 + 256*8+512*8)*16)*12
	((16 * 8 + 32 * 8 + 64 * 8 + 128*8 + 256*8+512*8)*16)*14
	((16 * 8 + 32 * 8 + 64 * 8 + 128*8 + 256*8+512*8)*16)*16

	//end for ((128+256+512+1024+2048+4096)*16)*16=(8064*16)*16=126K*16=2016K=258048*8
};
*/
#define SLOT_MAX(t) (((t) == 0) ? u8_t(16):u8_t(8))

#ifdef _DEBUG
	#define ___FACTOR (1)
#else
	#define ___FACTOR (4)
#endif

#ifdef _NETP_AM64
#define INIT_FACTOR (___FACTOR<<1)
#else
#define INIT_FACTOR (___FACTOR)
#endif

//object pool does not suit for large memory gap objects, so when object size incres, reduce count greatly
//slot count
//[128,384,896,1920,3968,8064,126k,2M]
	const static u32_t TABLE_SLOT_ENTRIES_INIT_LIMIT[TABLE::T_COUNT] = {
		1024*2*(INIT_FACTOR),
		1024*(INIT_FACTOR),
		512* (INIT_FACTOR),
		1024*(INIT_FACTOR),//2k
		32*(INIT_FACTOR),
		8*INIT_FACTOR,
		2 * INIT_FACTOR,
		NETP_MAX(1,1*(INIT_FACTOR>>1))
	};

	const static size_t TABLE_UP_BOUND[TABLE::T_COUNT+1] = {
		0,
		8*16,
		48*8,
		112*8,
		240*8,
		496 * 8,
		1008 * 8,
		16128 * 8,
		258048 * 8
	};

	inline void calc_SIZE_by_TABLE_SLOT(size_t& size, u8_t table, u8_t slot) {
		switch (table) {
		case TABLE::T0:
		{
			size = ((slot + 1) << 3);
		}
		break;
		case TABLE::T1: 
		case TABLE::T2:
		case TABLE::T3:
		case TABLE::T4:
		case TABLE::T5:
		{
			size= TABLE_UP_BOUND[table] + ((slot+1) << (table+4));
		}
		break;
		case TABLE::T6:
		case TABLE::T7:
		{
			size= (TABLE_UP_BOUND[table ] * ((slot+1))<<1);
		}
		break;
		}
	}

	inline void calc_TABLE_SLOT(size_t& size, u8_t& table, u8_t& slot) {
		for (u8_t t = 1; t < (TABLE::T_COUNT+1); ++t) {
			if (size <= TABLE_UP_BOUND[t]) {
				table = (--t);
				break;
			}
		}

		if(table == TABLE::T0)
		{
			slot = u8_t(size >> 3);
			(size % 8) != 0 ? ++slot : 0;
			size = (slot << 3);
			--slot;
			return;
		}
		else if(table < TABLE::T6)
		{
			size -= TABLE_UP_BOUND[table];
			slot = u8_t(size >> (table + 4));
			(size % size_t(1 << (table + 4))) != 0 ? ++slot : 0;
			size = TABLE_UP_BOUND[table] + (slot << (table + 4));
			--slot;
			return;
		} 
		else if(table < TABLE::T_COUNT)
		{
			const size_t LIMIT = TABLE_UP_BOUND[table];
			for (u8_t j = 0; j < 8; ++j) {
				if (size <= (((j + 1) << 1) * LIMIT)) {
					size = (((j + 1) << 1) * LIMIT);
					slot = j;
					return;
				}
			}
		}
	}

	void pool_align_allocator::init_table_slot(u8_t t, u8_t slot, std::vector<void*>& slotv, size_t capacity) {
		slotv.reserve(capacity >> 1);
		if (t > TABLE::T3) { 
			return;
		}

		for (size_t i = 0; i < (capacity >> 2); ++i) {
			size_t size = t==TABLE::T0 ? (slot + 1) << (3) : 
				TABLE_UP_BOUND[t] + ((slot + 1) << (t + 4));

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
		uptr = (u8_t*)netp::aligned_malloc(size, align_size);
		if (NETP_LIKELY(uptr != 0)) {
			*(uptr - 2) = ((t << 4) | (slot & 0xf));
		}
		return uptr;
	}

	void pool_align_allocator::free(void* ptr) {
		if (NETP_UNLIKELY(ptr == nullptr)) { return; }

		u8_t& slot_ = *((u8_t*)ptr-2);
		u8_t t = (slot_ >>4);
		u8_t slot = (slot_ & 0xf);
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

		u8_t old_slot = *((u8_t*)ptr - 2);
		u8_t old_t = (old_slot >> 4);
		old_slot = (old_slot&0xf);
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