#include <netp/memory.hpp>

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
	static_assert( sizeof(aligned_hdr) == 8, "check sizeof(aligned_hdr) failed");
	static_assert(alignof(std::max_align_t) <= NETP_DEFAULT_ALIGN, "check default aligne failed");

	//@note: msvc debug compiler would not inline these func, but release version do
	__NETP_FORCE_INLINE static u8_t __AH_UPDATE_OFFSET__(aligned_hdr* a_hdr, size_t alignment) {
		//@note: pls refer to https://en.wikipedia.org/wiki/Data_structure_alignment 
		const u8_t offset = u8_t(sizeof(aligned_hdr)) + ((alignment == alignof(std::max_align_t)) ? 0 : u8_t((~(std::size_t(a_hdr) + sizeof(aligned_hdr) - 1)) & ((alignment)-1)));
		NETP_ASSERT(alignment<=32 && offset <= 32 && offset>=sizeof(aligned_hdr));
		
		a_hdr->hdr.AH_4_7.alignment = u8_t(alignment);
		a_hdr->hdr.AH_4_7.offset = (offset);
		/*in case if the offset is not sizeof(aligned_hdr), no cmp, just set */
		*(reinterpret_cast<u8_t*>(a_hdr) + (offset) - 1) = (offset);
		return offset;
	}

	__NETP_FORCE_INLINE static void __AH_UPDATE_SIZE(aligned_hdr* a_hdr, size_t size) {
		a_hdr->hdr.size_L = size& 0xffffffff;
#ifdef _NETP_AM64
		a_hdr->hdr.AH_4_7.size_H = ((size >> 32) & 0xff);
#endif
	}

	__NETP_FORCE_INLINE static size_t __AH_SIZE(aligned_hdr* a_hdr) {
		size_t size_ = a_hdr->hdr.size_L;
#ifdef _NETP_AM64
		size_ |= (size_t(a_hdr->hdr.AH_4_7.size_H)<<32);
#endif
		return size_;
	}

	//@deprecated
	//data bit sequence
	//[size:48][might be empty, it depends][reserver:8][offset:8][returned ptr]
	/*
	#define __NETP_ALIGNED_HEAD (sizeof(u64_t))
	//@deprecated
	inline void* aligned_malloc(std::size_t size, std::size_t alignment)
	{
		NETP_ASSERT((alignment <= 32) && (size < (1ULL << 48)));

		void* original = std::malloc(size + alignment + __NETP_ALIGNED_HEAD);
		if (original == 0) return 0;
		//		void *aligned = reinterpret_cast<void*>(((reinterpret_cast<std::size_t>(original) + __NETP_ALIGNED_HEAD ) & ~(std::size_t(alignment - 1))) + alignment );
				//this one is better, if the address is aligned already, offset is zero
		void* aligned = reinterpret_cast<void*>((reinterpret_cast<std::size_t>(original) + __NETP_ALIGNED_HEAD + (alignment - 1)) & ~(alignment - 1));

		*(reinterpret_cast<char*>(aligned) - 1) = u8_t(size_t(aligned) - size_t(original));

		//the value would only be used by realloc
		*(reinterpret_cast<u32_t*>(original)) = (size & 0xffffffff);// L:32

#ifdef _NETP_AM64
		* (reinterpret_cast<u16_t*>(((u8_t*)original) + 4)) = ((size >> 32) & 0xffff); //H:16
#endif

		return aligned;
	}

	//internal Frees memory allocated with handmade_aligned_malloc /
	//@deprecated
	inline void aligned_free(void* ptr)
	{
		if (ptr) std::free((char*)ptr - *(reinterpret_cast<char*>(ptr) - 1));
	}

	//@deprecated
	inline void* aligned_realloc(void* ptr, std::size_t size, std::size_t alignment)
	{
		if (ptr == 0) { return aligned_malloc(size, alignment); }

		//void *previous_aligned = static_cast<u8_t *>(original) + previous_offset;
		//align data
		void* original = (u8_t*)ptr - *(reinterpret_cast<u8_t*>(ptr) - 1);
		size_t old_size = *(reinterpret_cast<u32_t*>(original));
#ifdef _NETP_AM64
		size_t old_size_H = *(reinterpret_cast<u16_t*>(((u8_t*)original) + 4));
		if (old_size_H > 0) {
			old_size |= (old_size_H << 32);
		}
#endif
		//refer to: https://en.cppreference.com/w/cpp/memory/c/realloc
		//The contents of the area remain unchanged up to the lesser of the new and old sizes.
		if (old_size == size) {
			return ptr;
		}

		void* new_ptr = aligned_malloc(size, alignment);
		if (new_ptr == 0) {
			aligned_free(ptr);
			return 0;
		}

		std::memcpy(new_ptr, ptr, NETP_MIN(old_size, size));
		aligned_free(ptr);
		return new_ptr;
	}
	*/

#define NETP_ALIGNED_ALLOCATOR_16_SLOT_EDGE_T size_t(T3)
#define NETP_ALIGNED_ALLOCATOR_SLOT_MAX(t) ( (t < NETP_ALIGNED_ALLOCATOR_16_SLOT_EDGE_T) ? 16 : 8)

	enum SLOT_ENTRIES_SIZE_LEVEL {
		L_DISABLED,
		L_EXTREM_LOW,
		L_LOW,
		L_MEDIUM,
		L_LARGE,
		L_EXTREM_LARGE,
		L_MAX
	};

	static SLOT_ENTRIES_SIZE_LEVEL g_memory_pool_slot_entries_size_level = L_EXTREM_LARGE;
	void set_memory_pool_slot_entries_size_level(int l) {
		if (l < L_EXTREM_LOW) {
			l = L_EXTREM_LOW;
		}
		else if (l > L_EXTREM_LARGE) {
			l = L_EXTREM_LARGE;
		}
		g_memory_pool_slot_entries_size_level = SLOT_ENTRIES_SIZE_LEVEL(l);
	}

	//object pool does not suit for large memory gap objects
	const u32_t TABLE_SLOT_ENTRIES_INIT_LIMIT[SLOT_ENTRIES_SIZE_LEVEL::L_MAX][TABLE::T_COUNT][16] = {
		{//L_DISABLED
			{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, //(0--128] 128/16 Byte
			{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, //(128--128+256]  256/16 Byte
			{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, //(128+256--128+256+512] 512/16
			{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},//(128+256+512--128+256+512+1024] [zzz] 1024/8
			{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},//128+256+512+1024--128+256+512+1024+2048 2048/8
			{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},//4--8K /8
			{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},//8-16K
			{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},//16-32K
			{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},//32-64K
			{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},//64-128K
			{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},//128-256K
			{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},//256-512K
			//4,//512-1M
			//2,//1M-2M
			//1//
		},
		{//L_EXTREM_LOW
			{128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128}, //(0--128] 128/16 Byte
			{64,64,64,64,64,64,64,64,32,32,32,32,32,32,32,32}, //(128--128+256]  256/16 Byte
			{16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16}, //(128+256--128+256+512] 512/16
			{16,16,16,64,64,64,16,16,0,0,0,0,0,0,0,0},//(128+256+512--128+256+512+1024] [zzz] 1024/8
			{16,16,16,8,8,8,8,16,0,0,0,0,0,0,0,0},//128+256+512+1024--128+256+512+1024+2048 2048/8
			{8,8,8,8,8,8,8,8,0,0,0,0,0,0,0,0},//4--8K /8
			{8,8,8,8,4,4,4,4,0,0,0,0,0,0,0,0},//8-16K
			{2,2,2,2,0,0,0,0,0,0,0,0,0,0,0,0},//16-32K
			{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},//32-64K
			{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},//64-128K
			{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},//128-256K
			{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},//256-512K
		},
		{//L_LOW
			{256,256,256,256,256,256,256,256,256,256,256,256,256,256,256,256}, //(0--128] 128/16 Byte
			{128,128,128,128,128,128,128,128,64,64,64,64,64,64,64,64}, //(128--128+256]  256/16 Byte
			{32,32,32,32,32,32,32,32,16,16,16,16,16,16,16,16}, //(128+256--128+256+512] 512/16
			{16,16,16,128,128,128,16,16,0,0,0,0,0,0,0,0},//(128+256+512--128+256+512+1024] [zzz] 1024/8
			{16,16,16,16,16,16,16,16,0,0,0,0,0,0,0,0},//128+256+512+1024--128+256+512+1024+2048 2048/8
			{16,16,16,16,8,8,8,8,0,0,0,0,0,0,0,0},//4--8K /8
			{8,8,8,8,4,4,4,4,0,0,0,0,0,0,0,0},//8-16K
			{4,4,4,4,0,0,0,0,0,0,0,0,0,0,0,0},//16-32K
			{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},//32-64K
			{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},//64-128K
			{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},//128-256K
			{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},//256-512K
		},
		{//L_MEDIUM
			{512,512,512,512,512,512,512,512,256,256,256,256,256,256,256,256}, //(0--128]  128/16 Byte
			{256,256,256,256,256,256,256,256,128,128,128,128,128,128,128,128}, //(128--128+256]  256/16 Byte
			{64,64,64,64,64,64,64,64,32,32,32,32,32,32,32,32}, //(128+256--128+256+512] 512/16
			{32,32,32,512,512,512,32,32,0,0,0,0,0,0,0,0},//(128+256+512--128+256+512+1024] [zzz] 1024/8
			{32,32,32,32,16,16,16,16,0,0,0,0,0,0,0,0},//128+256+512+1024--128+256+512+1024+2048 2048/8
			{16,16,16,16,16,16,16,16,0,0,0,0,0,0,0,0},//4--8K /8
			{16,16,16,16,16,16,16,16,0,0,0,0,0,0,0,0},//8-16K
			{8,8,8,8,8,8,8,8,0,0,0,0,0,0,0,0},//16-32K
			{8,8,8,8,8,8,8,8,0,0,0,0,0,0,0,0},//32-64K
			{4,4,4,4,4,4,4,4,0,0,0,0,0,0,0,0},//64-128K
			{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},//128-256K
			{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},//256-512K
		},
		{//L_LARGE
			{512,512,512,512,512,512,512,512,512,512,512,512,512,512,512,512}, //(0--128] /16 Byte
			{256,256,256,256,256,256,256,256,256,256,256,256,256,256,256,256}, //(128--128+256] /16 Byte
			{128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128}, //(128+256--128+256+512] /16		
			{128,128,128,1024,1024,1024,128,128,0,0,0,0,0,0,0,0},//(128+256+512--128+256+512+1024] [zzz] /8
			{128,128,128,128,128,128,128,128,0,0,0,0,0,0,0,0},//128+256+512+1024--128+256+512+1024+2048 /8
			{64,64,64,64,64,64,64,64,0,0,0,0,0,0,0,0},//4--8K /8
			{64,64,64,64,64,64,64,64,0,0,0,0,0,0,0,0},//8-16K
			{32,32,32,32,32,32,32,32,0,0,0,0,0,0,0,0},//16-32K
			{16,16,16,16,16,16,16,16,0,0,0,0,0,0,0,0},//32-64K
			{8,8,8,8,8,8,8,8,0,0,0,0,0,0,0,0},//64-128K
			{4,4,4,4,4,4,4,4,0,0,0,0,0,0,0,0},//128-256K
			{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},//256-512K
		},
		{//L_EXTREM_LARGE
			{1024,1024,1024,1024,1024,1024,1024,1024,1024,1024,1024,1024,1024,1024,1024,1024}, //(0--128] /16 Byte
			{512,512,512,512,512,512,512,512,512,512,512,512,512,512,512,512}, //(128--128+256] /16 Byte
			{256,256,256,256,256,256,256,256,256,256,256,256,256,256,256,256}, //(128+256--128+256+512] /16		
			{256,256,256,2048,2048,2048,256,256,0,0,0,0,0,0,0,0},//(128+256+512--128+256+512+1024] [zzz] /8
			{256,256,256,256,256,256,256,256,0,0,0,0,0,0,0,0},//128+256+512+1024--128+256+512+1024+2048 /8
			{128,128,128,128,128,128,128,128,0,0,0,0,0,0,0,0},//4--8K /8
			{128,128,128,128,128,128,128,128,0,0,0,0,0,0,0,0},//8-16K
			{64,64,64,64,64,64,64,64,0,0,0,0,0,0,0,0},//16-32K
			{32,32,32,32,32,32,32,32,0,0,0,0,0,0,0,0},//32-64K
			{16,16,16,16,16,16,16,16,0,0,0,0,0,0,0,0},//64-128K
			{8,8,8,8,8,8,8,8,0,0,0,0,0,0,0,0},//128-256K
			{4,4,4,4,4,4,4,4,0,0,0,0,0,0,0,0},//256-512K
		}
	};

	 const u32_t TABLE_BOUND[TABLE::T_COUNT+1] = {
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
//		128 + 256 + 512 + 1024 + 2048 + 4096 + 8192 + 16384 + 32768 + 65536 + 131072 + 262144 + 524288,//
//		128 + 256 + 512 + 1024 + 2048 + 4096 + 8192 + 16384 + 32768 + 65536 + 131072 + 262144 + 524288 + 1048576,//
//		128 + 256 + 512 + 1024 + 2048 + 4096 + 8192 + 16384 + 32768 + 65536 + 131072 + 262144 + 524288 + 1048576*2//
	 };

	#define calc_F_by_slot(s) ((s<NETP_ALIGNED_ALLOCATOR_16_SLOT_EDGE_T)?3:4)
	#define calc_SIZE_by_TABLE_SLOT(t,f,s) (TABLE_BOUND[t] + ((1ULL<<((t) + (f)))) * (((s) + 1)))

	#define calc_TABLE__(size, t) do { \
		for (u8_t ti = 1; ti < (TABLE::T_COUNT+1); ++ti) { \
			if (size <= TABLE_BOUND[ti]) { \
				t = (--ti); \
				break; \
			} \
		} \
	}while(false);\

#define calc_TABLE(size,t) do { \
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

	void pool_aligned_allocator::preallocate_table_slot_item(table_slot_t* tst, u8_t t, u8_t slot, size_t item_count) {
		(void)tst;
		u8_t** ptr = (u8_t**)std::malloc(sizeof(u8_t*) * (item_count));
		size_t size = calc_SIZE_by_TABLE_SLOT(t, calc_F_by_slot(t), slot);
		size_t i;
		for (i = 0; i < (item_count); ++i) {
			ptr[i] = (u8_t*) pool_aligned_allocator::malloc((size), NETP_DEFAULT_ALIGN);
		}
		for (size_t j = 0; j < i; ++j) {
			pool_aligned_allocator::free(ptr[j]);
		}
		std::free(ptr);
	}

	void pool_aligned_allocator::deallocate_table_slot_item(table_slot_t* tst) {
		while (tst->count) {//stop at 0
			std::free( (void*)(tst->ptr[--tst->count]) );
		}
	}

	void pool_aligned_allocator::init( bool preallocate ) {
		for (u8_t t = 0; t < TABLE::T_COUNT; ++t) {
			const u8_t slot_max = NETP_ALIGNED_ALLOCATOR_SLOT_MAX(t);
			m_tables[t] = (table_slot_t**) std::malloc(sizeof(table_slot_t**) * slot_max);
			for (u8_t s = 0; s < slot_max; ++s) {
				//NETP_ASSERT(TABLE_SLOT_ENTRIES_INIT_LIMIT[t][s] >0 );
				u8_t* __ptr = (u8_t*) std::malloc(sizeof(table_slot_t) + (sizeof(u8_t*) * TABLE_SLOT_ENTRIES_INIT_LIMIT[g_memory_pool_slot_entries_size_level][t][s]));
				m_tables[t][s] = (table_slot_t*)__ptr;
				m_tables[t][s]->max = TABLE_SLOT_ENTRIES_INIT_LIMIT[g_memory_pool_slot_entries_size_level][t][s];
				m_tables[t][s]->count = 0;
				m_tables[t][s]->ptr = (u8_t**)(__ptr + (sizeof(table_slot_t)));

				if (preallocate && (TABLE_SLOT_ENTRIES_INIT_LIMIT[g_memory_pool_slot_entries_size_level][t][s] > 0) && (t< NETP_ALIGNED_ALLOCATOR_16_SLOT_EDGE_T /*<1k*/) ) {
					preallocate_table_slot_item(m_tables[t][s], t, s, (TABLE_SLOT_ENTRIES_INIT_LIMIT[g_memory_pool_slot_entries_size_level][t][s]>>1) );
				}
			}
		}
	}

	void pool_aligned_allocator::deinit() {
		for (u8_t t = 0; t < TABLE::T_COUNT; ++t) {
			const u8_t slot_max = NETP_ALIGNED_ALLOCATOR_SLOT_MAX(t);
			for (u8_t s = 0; s < slot_max; ++s) {
				deallocate_table_slot_item(m_tables[t][s]);
				//deallocate_table_slot(m_tables[t][s]);
				std::free(m_tables[t][s]);
			}
			std::free(m_tables[t]);
		}
	}

	pool_aligned_allocator::pool_aligned_allocator( bool preallocate ) {
		init(preallocate);
	}

	pool_aligned_allocator::~pool_aligned_allocator() {
		deinit();
	}

	void* pool_aligned_allocator::malloc(size_t size, size_t alignment) {
		NETP_ASSERT( size < _NETP_ALIGN_MALLOC_SIZE_MAX );
		NETP_ASSERT( (alignment%alignof(std::max_align_t)) == 0 && alignment >= alignof(std::max_align_t) );
		u8_t t = T_COUNT;
		u8_t s = u8_t(-1);

		aligned_hdr* a_hdr;

		//assume that the std::malloc always return address aligned to alignof(std::max_align_t)
		size_t slot_size = (alignment == alignof(std::max_align_t)) ? size : (size +alignment);
		calc_TABLE(slot_size,t);

		if (NETP_LIKELY(t<T_COUNT) ) {

			slot_size -= TABLE_BOUND[t];
			u8_t f = calc_F_by_slot(t);
			s = u8_t(slot_size >> (t + f));
			(slot_size % ((1ULL << (t + f)))) == 0 ? --s : 0;

#ifdef _NETP_DEBUG
			NETP_ASSERT((t < NETP_ALIGNED_ALLOCATOR_16_SLOT_EDGE_T) ? (s < 16) : (s < 8));
#endif
			table_slot_t*& tst = (m_tables[t][s]);

			if (tst->count) {
__fast_path:
	#ifdef _NETP_DEBUG
				NETP_ASSERT(TABLE_SLOT_ENTRIES_INIT_LIMIT[g_memory_pool_slot_entries_size_level][t][s]>0);
				NETP_ASSERT(tst->ptr[tst->count-1] != 0);
				NETP_ASSERT(calc_SIZE_by_TABLE_SLOT(t, f, s) >= slot_size);
	#endif
				 a_hdr = (aligned_hdr*) (tst->ptr[--tst->count]);

				 //update new size
				 __AH_UPDATE_SIZE(a_hdr, size);
				 return (u8_t*)a_hdr + __AH_UPDATE_OFFSET__(a_hdr, alignment);
			}

			if (tst->max) {
				//tst->max ==0 means no pool object allowed in this slot
				//borrow
				size_t c = global_pool_aligned_allocator::instance()->borrow(t, s, tst, (tst->max) >> 1);
				NETP_ASSERT(c == tst->count);
				if (c != 0) {
					goto __fast_path;
				}
			}

			//apply to upper edge of that slot
			slot_size = calc_SIZE_by_TABLE_SLOT(t,f,s);
		}

		//std::malloc alwasy return ptr aligned to alignof(std::max_align_t), so ,we do not need to worry about the hdr access
		a_hdr = (aligned_hdr*)std::malloc(sizeof(aligned_hdr)+ slot_size );
		if (NETP_UNLIKELY(a_hdr == 0)) {
			return 0;
		}

		//size is used by realloc
		__AH_UPDATE_SIZE(a_hdr, size);
		a_hdr->hdr.AH_4_7.t = t;
		a_hdr->hdr.AH_4_7.s = s;
		return (u8_t*)a_hdr + __AH_UPDATE_OFFSET__(a_hdr, alignment);
	}

	void pool_aligned_allocator::free(void* ptr) {
		if (NETP_UNLIKELY(ptr == 0)) { return; }

		u8_t offset = *(reinterpret_cast<u8_t*>(ptr) - 1);
		aligned_hdr* a_hdr = (aligned_hdr*)((u8_t*)ptr - offset);
		NETP_ASSERT( a_hdr->hdr.AH_4_7.offset == offset );
		u8_t t = a_hdr->hdr.AH_4_7.t;
		u8_t s = a_hdr->hdr.AH_4_7.s;
		table_slot_t*& tst = (m_tables[t][s]);

		//if tst->max == tst->count ==0, skiped
		if ((t < T_COUNT) && (tst->count<tst->max) ) {
			tst->ptr[tst->count++] = (u8_t*)a_hdr;
			if (tst->count == tst->max) {
				global_pool_aligned_allocator::instance()->commit(t, s, tst, (tst->max) >> 1);
			}
			return;
		}
		std::free((void*)a_hdr);
	}


	//alloc, then copy
	void* pool_aligned_allocator::realloc(void* old_ptr, size_t size, size_t alignment) {
		//align_alloc first
		if (old_ptr == 0) { return pool_aligned_allocator::malloc(size, alignment); }

		u8_t* newptr = (u8_t*)pool_aligned_allocator::malloc(size, alignment);
		//we would never get old ptr, cuz old ptr have not yet been returned
		NETP_ASSERT(newptr != old_ptr);

		u8_t old_offset = *(reinterpret_cast<u8_t*>(old_ptr) - 1);
		aligned_hdr* old_a_hdr = (aligned_hdr*)((u8_t*)old_ptr - old_offset);
		NETP_ASSERT(old_a_hdr->hdr.AH_4_7.offset == old_offset);

		size_t old_size = __AH_SIZE(old_a_hdr);
		//do copy
		std::memcpy(newptr, old_ptr, NETP_MIN(size, old_size));

		//free ptr
		pool_aligned_allocator::free(old_ptr);
		return newptr;
	}

	global_pool_aligned_allocator::global_pool_aligned_allocator():
		pool_aligned_allocator(false)
	{
		for (size_t t = 0; t < sizeof(m_tables) / sizeof(m_tables[0]); ++t) {
			m_table_slots_mtx[t] = ::new spin_mutex[NETP_ALIGNED_ALLOCATOR_SLOT_MAX(t)];
		}
	}

	global_pool_aligned_allocator::~global_pool_aligned_allocator() {
		for (size_t t = 0; t < sizeof(m_tables) / sizeof(m_tables[0]); ++t) {
			for (size_t s = 0; s < NETP_ALIGNED_ALLOCATOR_SLOT_MAX(t); ++s) {
				lock_guard<spin_mutex> lg(m_table_slots_mtx[t][s]);
				u32_t& _gcount = m_tables[t][s]->count;
				while ( _gcount>0) {
					std::free(m_tables[t][s]->ptr[ --_gcount ]);
				}
			}
		}
		//deallocate mutex
		for (size_t t = 0; t < sizeof(m_tables) / sizeof(m_tables[0]); ++t) {
			::delete[] m_table_slots_mtx[t];
		}
	}

	//default: one thread -- main thread
	void global_pool_aligned_allocator::incre_thread_count() {
		for (size_t t = 0; t < sizeof(m_tables) / sizeof(m_tables[0]); ++t) {
			for (size_t s = 0; s < NETP_ALIGNED_ALLOCATOR_SLOT_MAX(t); ++s) {
				lock_guard<spin_mutex> lg(m_table_slots_mtx[t][s] );
				if (TABLE_SLOT_ENTRIES_INIT_LIMIT[g_memory_pool_slot_entries_size_level][t][s] == 0) {
					NETP_ASSERT(m_tables[t][s]->max == 0);
					continue;
				}
				u32_t max_n = m_tables[t][s]->max + TABLE_SLOT_ENTRIES_INIT_LIMIT[g_memory_pool_slot_entries_size_level][t][s];
				u8_t* __ptr = (u8_t*)(std::realloc(m_tables[t][s], sizeof(table_slot_t) + sizeof(u8_t*) * max_n));
				m_tables[t][s] = (table_slot_t*)__ptr;
				m_tables[t][s]->max = max_n;
				m_tables[t][s]->ptr = (u8_t**) (__ptr + sizeof(table_slot_t));
			}
		}
	}

	void global_pool_aligned_allocator::decre_thread_count() {
		for (size_t t = 0; t < sizeof(m_tables) / sizeof(m_tables[0]); ++t) {
			for (size_t s = 0; s < NETP_ALIGNED_ALLOCATOR_SLOT_MAX(t); ++s) {
				//NETP_ASSERT(TABLE_SLOT_ENTRIES_INIT_LIMIT[t][s] > 0);
				lock_guard<spin_mutex> lg(m_table_slots_mtx[t][s]);
				if (TABLE_SLOT_ENTRIES_INIT_LIMIT[g_memory_pool_slot_entries_size_level][t][s] == 0) {
					NETP_ASSERT(m_tables[t][s]->max == 0);
					continue;
				}

				NETP_ASSERT(m_tables[t][s]->max >= TABLE_SLOT_ENTRIES_INIT_LIMIT[g_memory_pool_slot_entries_size_level][t][s]);
				u32_t max_n = m_tables[t][s]->max - TABLE_SLOT_ENTRIES_INIT_LIMIT[g_memory_pool_slot_entries_size_level][t][s];
				u32_t& _gcount = m_tables[t][s]->count;
				//purge exceed count first
				while (_gcount > max_n) {
					std::free(m_tables[t][s]->ptr[--_gcount]);
				}
				u8_t* __ptr = (u8_t*)(std::realloc(m_tables[t][s], sizeof(table_slot_t) + sizeof(u8_t*) * max_n));
				m_tables[t][s] = (table_slot_t*)__ptr;
				m_tables[t][s]->max = max_n;
				m_tables[t][s]->ptr = (u8_t**)(__ptr + sizeof(table_slot_t));
			}
		}
	}

	u32_t global_pool_aligned_allocator::commit(u8_t t, u8_t s, table_slot_t* tst, u32_t commit_count) {
		NETP_ASSERT( commit_count <= tst->count );
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

	u32_t global_pool_aligned_allocator::borrow(u8_t t, u8_t s, table_slot_t* tst, u32_t borrow_count) {
		NETP_ASSERT(tst->count ==0 );
		NETP_ASSERT(tst->max > 0);
		lock_guard<spin_mutex> lg(m_table_slots_mtx[t][s]);
		u32_t& _gcount = m_tables[t][s]->count;
		u32_t& _tcount = tst->count;
		while ((_gcount > 0) && (_tcount < borrow_count)) {
			tst->ptr[_tcount++] = m_tables[t][s]->ptr[--_gcount];
		}
		return _tcount;
	}
}