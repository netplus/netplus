#ifndef NETP_MEMORY_HPP_
#define NETP_MEMORY_HPP_


#include <vector>
#include <cstring>
#include <type_traits>

#include <netp/core/compiler.hpp>
#include <netp/core/platform.hpp>

#include <netp/exception.hpp>
#include <netp/core/macros.hpp>
#include <netp/tls.hpp>

#define USE_POOL 1
//#define USE_ALIGN_MALLOC 1
//#define USE_STD_MALLOC

namespace netp {

//AVX 32
//SSE16
//@todo

	//ALIGN_SIZE SHOUDL BE LESS THAN 256
	//STORE OFFSET IN PREVIOUS BYTES
	inline void* aligned_malloc(std::size_t size, std::size_t alignment)
	{
		void *original = std::malloc(size + alignment);
		if (original == 0) return 0;
		void *aligned = reinterpret_cast<void*>((reinterpret_cast<std::size_t>(original) & ~(std::size_t(alignment - 1))) + alignment);
		//NETP_ASSERT( (size_t(aligned) - size_t(original)) >sizeof(void*) );
		//NETP_ASSERT( (size_t(aligned) - size_t(original)) < 0xff);
		*(reinterpret_cast<char*>(aligned) - 1) = u8_t(size_t(aligned) - size_t(original));
		return aligned;
	}

	/** \internal Frees memory allocated with handmade_aligned_malloc */
	inline void aligned_free(void *ptr)
	{
		if (ptr) std::free( (char*)ptr - *(reinterpret_cast<char*>(ptr)-1) );
	}

	inline void* aligned_realloc(void* ptr, std::size_t size, std::size_t alignment)
	{
		if (ptr == 0) { return aligned_malloc(size, alignment); }
		const u8_t previous_offset = *(reinterpret_cast<u8_t*>(ptr) - 1);
		void* original = (u8_t*)ptr - previous_offset;
		original = std::realloc(original, size + alignment);
		if (original == 0) {return 0;}
		void *aligned = reinterpret_cast<void*>((reinterpret_cast<std::size_t>(original) & ~(std::size_t(alignment - 1))) + alignment);
		void *previous_aligned = static_cast<u8_t *>(original) + previous_offset;
		if (aligned != previous_aligned) {
			//align data
			std::memmove(aligned, previous_aligned, size);
		}
		//update offset
		*(reinterpret_cast<char*>(aligned) - 1) = u8_t(size_t(aligned) - size_t(original));
		return aligned;
	}

	enum TABLE {
		T0 = 0,
		T1 = 1,
		T2 = 2,
		T3 = 3,
		T4 = 4,
		T5 = 5,
		T6 = 6,
		T7 = 7,
		T_COUNT = 8
	};


	//NOTE: if want to share address with different alignment in the same pool, we need to check alignment and do a re-align if necessary
	class pool_align_allocator {

		std::vector<void*>* m_tables[TABLE::T_COUNT];
		size_t m_entries_limit[TABLE::T_COUNT];

		void set_slot_entries_limit(size_t tidx, size_t capacity) {
			m_entries_limit[tidx] = capacity;
		}

		void init_table_slot(u8_t t, u8_t slot, std::vector<void*>& slotv, size_t capacity);
		void init_table(u8_t t, std::vector<void*>* table);

		void free_table_slot(std::vector<void*>& slot);
		void free_table(u8_t t, std::vector<void*>* table);

		void init();
		void deinit();

		public:
			pool_align_allocator();
			~pool_align_allocator();

			void* malloc(size_t size, size_t alignment = NETP_DEFAULT_ALIGN);
			void free(void* ptr);
			void* realloc(void* ptr, size_t size, size_t alignment = NETP_DEFAULT_ALIGN);
	};

	using pool_align_allocator_t = pool_align_allocator;

	struct tag_allocator_std_malloc {};
	struct tag_allocator_default_new {};
	struct tag_allocator_align_malloc {};
	struct tag_allocator_pool {};

	//std::allocator<T> AA;
	template<class allocator_t>
	struct allocator_wrapper {
		static inline void* malloc(size_t n, size_t alignment = NETP_DEFAULT_ALIGN) {
			return 0;
		}
		static inline void free(void* p) {
		}
		static inline void* realloc(void* ptr, size_t size, size_t alignment = NETP_DEFAULT_ALIGN) {
			return 0;
		}
	};

	template<>
	struct allocator_wrapper<tag_allocator_std_malloc> {
		static inline void* malloc(size_t n, size_t alignment = NETP_DEFAULT_ALIGN) {
			(void)alignment;
			return std::malloc(n);
		}
		static inline void free(void* p) {
			std::free(p);
		}
		static inline void* realloc(void* ptr, size_t size, size_t alignment = NETP_DEFAULT_ALIGN) {
			(void)(alignment);
			return std::realloc(ptr, size);
		}
	};

	template<>
	struct allocator_wrapper<tag_allocator_default_new> {
		static inline void* malloc(size_t n, size_t alignment = NETP_DEFAULT_ALIGN) {
			(void)(alignment);
			return static_cast<void*>(::operator new(n));
		}
		static inline void free(void* p) {
			::operator delete(p);
		}
		static inline void* realloc(void* ptr, size_t size, size_t alignment = NETP_DEFAULT_ALIGN) {
			(void)(alignment);
			(void)ptr;
			(void)size;
			throw std::bad_alloc();
		}
	};

	template<>
	struct allocator_wrapper<tag_allocator_align_malloc> {
		static inline void* malloc(size_t n, size_t alignment = NETP_DEFAULT_ALIGN) {
			return netp::aligned_malloc(n, alignment);
		}
		static inline void free(void* p) {
			netp::aligned_free(p);
		}
		static inline void* realloc(void* ptr, size_t size, size_t alignment = NETP_DEFAULT_ALIGN) {
			return netp::aligned_realloc(ptr, size, alignment);
		}
	};

	template<>
	struct allocator_wrapper<tag_allocator_pool> {
		static inline void* malloc(size_t n, size_t alignment = NETP_DEFAULT_ALIGN) {
			return tls_get<netp::pool_align_allocator_t>()->malloc(n, alignment);
		}
		static inline void free(void* p) {
			tls_get<netp::pool_align_allocator_t>()->free(p);
		}
		static inline void* realloc(void* ptr, size_t size, size_t alignment = NETP_DEFAULT_ALIGN) {
			return tls_get<netp::pool_align_allocator_t>()->realloc(ptr, size, alignment);
		}
	};

	template<typename T, class allocator_wrapper_t>
	struct allocator_base:
		private allocator_wrapper_t
	{
		typedef allocator_base<T, allocator_wrapper_t> allocator_base_t;
		typedef std::size_t size_type;
		typedef std::ptrdiff_t difference_type;
		typedef T* pointer;
		typedef const T* const_pointer;
		typedef T& reference;
		typedef const T& const_reference;
		typedef T value_type;

		//these three api for convenience purpose
		inline static pointer malloc(size_t n, size_t alignment= NETP_DEFAULT_ALIGN) {
			return static_cast<pointer>(allocator_wrapper_t::malloc(sizeof(value_type) * n, alignment));
		}
		inline static void free(pointer p) {
			allocator_wrapper_t::free(p);
		}
		inline static pointer realloc(pointer ptr, size_t size, size_t alignment = NETP_DEFAULT_ALIGN) {
			return static_cast<pointer>(allocator_wrapper_t::realloc(ptr,size,alignment));
		}

		//for container compliance below
		inline pointer allocate(size_type n) {
			return allocator_base_t::malloc(n);
		}

		inline void deallocate(pointer p, size_type n) {
			allocator_base_t::free(p);
			(void)n;
		}

		template<class _Objty,
			class... _Types>
			inline void construct(_Objty* _Ptr, _Types&&... _Args)
		{	// construct _Objty(_Types...) at _Ptr
			//all the object created instanced by operator placement new must not be called by operate delete.
			//we have to call destructor by ourself first, then do memory free by ourown logic
			//so, construct&destroy must be paired
			::new ((void*)_Ptr) _Objty( std::forward<_Types>(_Args)...);
		}
		template<class _Uty>
		inline void destroy(_Uty* _Ptr)
		{	// destroy object at _Ptr
			_Ptr->~_Uty();
			(void)_Ptr;//win32 compiler would prompt with _Ptr unreferenced without this line
		}
	};

	template <class T>
	struct allocator_std_malloc :
		public allocator_base<T, allocator_wrapper<tag_allocator_std_malloc>>
	{
		typedef  allocator_base<T, allocator_wrapper<tag_allocator_std_malloc>> allocator_base_t;
		typedef allocator_std_malloc<T> allocator_t;

		typedef typename allocator_base_t::size_type size_type;
		typedef typename allocator_base_t::difference_type difference_type;
		typedef typename allocator_base_t::pointer pointer;
		typedef typename allocator_base_t::const_pointer const_pointer;
		typedef typename allocator_base_t::reference reference;
		typedef typename allocator_base_t::const_reference const_reference;
		typedef typename allocator_base_t::value_type value_type;

		//cpp 17
		//typedef true_type is_always_equal;

		template <class U>
		struct rebind {
			typedef allocator_std_malloc<U> other;
		};

#if __cplusplus >= 201103L
		typedef std::true_type propagate_on_container_move_assignment;
#endif

		allocator_std_malloc() _NETP_NOEXCEPT
		{}
		allocator_std_malloc(const allocator_t&) _NETP_NOEXCEPT
		{}
		~allocator_std_malloc() _NETP_NOEXCEPT
		{}

		//hint for kinds of container construct proxy
		template <class U>
		allocator_std_malloc(const allocator_std_malloc<U>&) _NETP_NOEXCEPT
		{
		}
	};

	//thread safe
	template <class T>
	struct allocator_default_new:
		public allocator_base<T, allocator_wrapper<tag_allocator_default_new>>
	{
		typedef  allocator_base<T, allocator_wrapper<tag_allocator_default_new>> allocator_base_t;
		typedef allocator_default_new<T> allocator_t;

		typedef typename allocator_base_t::size_type size_type;
		typedef typename allocator_base_t::difference_type difference_type;
		typedef typename allocator_base_t::pointer pointer;
		typedef typename allocator_base_t::const_pointer const_pointer;
		typedef typename allocator_base_t::reference reference;
		typedef typename allocator_base_t::const_reference const_reference;
		typedef typename allocator_base_t::value_type value_type;

		//cpp 17
		//typedef true_type is_always_equal;

		template <class U>
		struct rebind {
			typedef allocator_default_new<U> other;
		};

 #if __cplusplus >= 201103L
		typedef std::true_type propagate_on_container_move_assignment;
#endif

		allocator_default_new() _NETP_NOEXCEPT
		{}
		allocator_default_new(const allocator_t&) _NETP_NOEXCEPT
		{}
		~allocator_default_new() _NETP_NOEXCEPT
		{}

		//hint for kinds of container construct proxy
		template <class U>
		allocator_default_new( const allocator_default_new<U>& ) _NETP_NOEXCEPT
		{
		}
	};

	//thread safe
	template <class T>
	struct allocator_align_malloc :
		public allocator_base<T, allocator_wrapper<tag_allocator_align_malloc>>
	{
		typedef  allocator_base<T, allocator_wrapper<tag_allocator_align_malloc>> allocator_base_t;
		typedef allocator_align_malloc<T> allocator_t;

		typedef typename allocator_base_t::size_type size_type;
		typedef typename allocator_base_t::difference_type difference_type;
		typedef typename allocator_base_t::pointer pointer;
		typedef typename allocator_base_t::const_pointer const_pointer;
		typedef typename allocator_base_t::reference reference;
		typedef typename allocator_base_t::const_reference const_reference;
		typedef typename allocator_base_t::value_type value_type;

		//cpp 17
		//typedef true_type is_always_equal;

		template <class U>
		struct rebind {
			typedef allocator_align_malloc<U> other;
		};

#if __cplusplus >= 201103L
		typedef std::true_type propagate_on_container_move_assignment;
#endif

		allocator_align_malloc() _NETP_NOEXCEPT
		{}
		allocator_align_malloc(const allocator_t&) _NETP_NOEXCEPT
		{}
		~allocator_align_malloc() _NETP_NOEXCEPT
		{}

		//hint for kinds of container construct proxy
		template <class U>
		allocator_align_malloc(const allocator_align_malloc<U>&) _NETP_NOEXCEPT
		{
		}
	};

	//thread safe
	template <class T>
	struct allocator_pool :
		public allocator_base<T, allocator_wrapper<tag_allocator_pool>>
	{
		typedef  allocator_base<T, allocator_wrapper<tag_allocator_pool>> allocator_base_t;
		typedef allocator_pool<T> allocator_t;

		typedef typename allocator_base_t::size_type size_type;
		typedef typename allocator_base_t::difference_type difference_type;
		typedef typename allocator_base_t::pointer pointer;
		typedef typename allocator_base_t::const_pointer const_pointer;
		typedef typename allocator_base_t::reference reference;
		typedef typename allocator_base_t::const_reference const_reference;
		typedef typename allocator_base_t::value_type value_type;

		//cpp 17
		//typedef true_type is_always_equal;

		template <class U>
		struct rebind {
			typedef allocator_pool<U> other;
		};

#if __cplusplus >= 201103L
		typedef std::true_type propagate_on_container_move_assignment;
#endif

		allocator_pool() _NETP_NOEXCEPT
		{}
		allocator_pool(const allocator_t&) _NETP_NOEXCEPT
		{}
		~allocator_pool() _NETP_NOEXCEPT
		{}

		//hint for kinds of container construct proxy
		template <class U>
		allocator_pool(const allocator_pool<U>&) _NETP_NOEXCEPT
		{
		}
	};

	template<class T>
#ifdef USE_POOL
	using allocator = netp::allocator_pool<T>;
#elif defined(USE_ALIGN_MALLOC)
	using allocator = netp::allocator_align_malloc<T>;
#else
	using allocator = netp::allocator_std_malloc<T>;
#endif

	template<typename _T1, typename _T2>
	inline bool operator==(const allocator<_T1>&, const allocator<_T2>&)
	{
		return true;
	}

	template<typename _Tp>
	inline bool operator==(const allocator<_Tp>&, const allocator<_Tp>&)
	{
		return true;
	}

	template<typename _T1, typename _T2>
	inline bool operator!=(const allocator<_T1>&, const allocator<_T2>&)
	{
		return false;
	}

	template<typename _Tp>
	inline bool operator!=(const allocator<_Tp>&, const allocator<_Tp>&)
	{
		return false;
	}

	template<class T>
	struct new_array_trait {
		//this is track
		//function template does not support partial specialised
		inline static T* new_array(size_t n, size_t alignment = NETP_DEFAULT_ALIGN) {
			T* ptr = netp::allocator<T>::malloc(n,alignment);
			for (size_t i = 0; i < n; ++i) {
				//TODO: might throw exception
				//for standard impl, compiler would wrap these line with try...catch...delete
				//NOTE: refer to https://isocpp.org/wiki/faq/dtors
				//placement new have no related delete operation, we have to call destructor and free the memory by ourself
				new((void*)(ptr+i)) T();
			}
			return ptr;
		}
		inline static void delete_array(T* ptr, size_t n) {
			for (size_t i = 0; i < n; ++i) {
				ptr[i].~T();
			}
			netp::allocator<T>::free(ptr);
		}
	};

	template<>
	struct new_array_trait<byte_t> {
		//this is a track
		//function template does not support partial specialised
		inline static byte_t* new_array(size_t n, size_t alignment = NETP_DEFAULT_ALIGN) {
			return netp::allocator<byte_t>::malloc(n, alignment);
		}
		inline static void delete_array(byte_t* ptr, size_t n) {
			netp::allocator<byte_t>::free(ptr);
			(void)n;
		}
	};

	template<>
	struct new_array_trait<u16_t> {
		//this is track
		//function template does not support partial specialised
		inline static u16_t* new_array(size_t n, size_t alignment = NETP_DEFAULT_ALIGN) {
			return netp::allocator<u16_t>::malloc(n,alignment);
		}
		inline static void delete_array(u16_t* ptr, size_t n) {
			netp::allocator<u16_t>::free(ptr);
			(void)n;
		}
	};

	template<>
	struct new_array_trait<u32_t> {
		//this is track
		//function template does not support partial specialised
		inline static u32_t* new_array(size_t n, size_t alignment = NETP_DEFAULT_ALIGN) {
			return netp::allocator<u32_t>::malloc(n,alignment);
		}
		inline static void delete_array(u32_t* ptr, size_t n) {
			netp::allocator<u32_t>::free(ptr);
			(void)n;
		}
	};

	template<>
	struct new_array_trait<u64_t> {
		//this is track
		//function template does not support partial specialised
		inline static u64_t* new_array(size_t n, size_t alignment = NETP_DEFAULT_ALIGN) {
			return netp::allocator<u64_t>::malloc(n,alignment);
		}
		inline static void delete_array(u64_t* ptr, size_t n) {
			netp::allocator<u64_t>::free(ptr);
			(void)n;
		}
	};

	template<class T>
	inline T* new_array(size_t n, size_t alignment = NETP_DEFAULT_ALIGN) {
		return new_array_trait<T>::new_array(n,alignment);
	}

	template<class T>
	inline void delete_array(T* ptr, size_t n) {
		new_array_trait<T>::delete_array(ptr,n);
	}
	
	/*
	namespace __gnugcc_impl {
		template<typename _Tp>
		inline _Tp*
			__addressof(_Tp& __r) _NETP_NOEXCEPT
		{
			return reinterpret_cast<_Tp*>
				(&const_cast<char&>(reinterpret_cast<const volatile char&>(__r)));
		}

		template<typename _Tp>
		class new_allocator
		{
		public:
			typedef size_t     size_type;
			typedef std::ptrdiff_t  difference_type;
			typedef _Tp* pointer;
			typedef const _Tp* const_pointer;
			typedef _Tp& reference;
			typedef const _Tp& const_reference;
			typedef _Tp        value_type;

			template<typename _Tp1>
			struct rebind
			{
				typedef new_allocator<_Tp1> other;
			};

#if __cplusplus >= 201103L
			// _GLIBCXX_RESOLVE_LIB_DEFECTS
			// 2103. propagate_on_container_move_assignment
			typedef std::true_type propagate_on_container_move_assignment;
#endif

			new_allocator() _NETP_NOEXCEPT { }

			new_allocator(const new_allocator&) _NETP_NOEXCEPT { }

			template<typename _Tp1>
			new_allocator(const new_allocator<_Tp1>&) _NETP_NOEXCEPT { }

			~new_allocator() _NETP_NOEXCEPT { }

			pointer address(reference __x) const _NETP_NOEXCEPT
			{
				return __addressof(__x);
			}

			const_pointer address(const_reference __x) const _NETP_NOEXCEPT
			{
				return __addressof(__x);
			}

			// NB: __n is permitted to be 0.  The C++ standard says nothing
			// about what the return value is when __n == 0.
			pointer
				allocate(size_type __n, const void* = 0)
			{
				//if (__n > this->max_size())
				//	std::__throw_bad_alloc();

				return static_cast<_Tp*>(::operator new(__n * sizeof(_Tp)));
			}

			// __p is not permitted to be a null pointer.
			void
				deallocate(pointer __p, size_type)
			{
				::operator delete(__p);
			}

			size_type
				max_size() const _NETP_NOEXCEPT
			{
				return size_t(-1) / sizeof(_Tp);
			}

#if __cplusplus >= 201103L
			template<typename _Up, typename... _Args>
			void
				construct(_Up* __p, _Args&&... __args)
			{
				::new((void*)__p) _Up(std::forward<_Args>(__args)...);
			}

			template<typename _Up>
			void
				destroy(_Up* __p) { __p->~_Up(); }
#else
			// _GLIBCXX_RESOLVE_LIB_DEFECTS
			// 402. wrong new expression in [some_] allocator::construct
			void
				construct(pointer __p, const _Tp& __val)
			{
				::new((void*)__p) _Tp(__val);
			}

			void
				destroy(pointer __p) { __p->~_Tp(); }
#endif
		};

		template<typename _Tp>
		inline bool
			operator==(const new_allocator<_Tp>&, const new_allocator<_Tp>&)
		{
			return true;
		}

		template<typename _Tp>
		inline bool
			operator!=(const new_allocator<_Tp>&, const new_allocator<_Tp>&)
		{
			return false;
		}

		template<typename _Tp>
		using __allocator_base = new_allocator<_Tp>;

		template<typename _Tp>
		class allocator : public __allocator_base<_Tp>
		{
		public:
			typedef size_t     size_type;
			typedef std::ptrdiff_t  difference_type;
			typedef _Tp* pointer;
			typedef const _Tp* const_pointer;
			typedef _Tp& reference;
			typedef const _Tp& const_reference;
			typedef _Tp        value_type;

			template<typename _Tp1>
			struct rebind
			{
				typedef allocator<_Tp1> other;
			};

#if __cplusplus >= 201103L
			// _GLIBCXX_RESOLVE_LIB_DEFECTS
			// 2103. std::allocator propagate_on_container_move_assignment
			typedef std::true_type propagate_on_container_move_assignment;
#endif

			allocator() throw() { }

			allocator(const allocator& __a) throw()
				: __allocator_base<_Tp>(__a) { }

			template<typename _Tp1>
			allocator(const allocator<_Tp1>&) throw() { }
			~allocator() throw() { }
			// Inherit everything else.
		};

		template<typename _T1, typename _T2>
		inline bool
			operator==(const allocator<_T1>&, const allocator<_T2>&)
		{
			return true;
		}

		template<typename _Tp>
		inline bool
			operator==(const allocator<_Tp>&, const allocator<_Tp>&)
		{
			return true;
		}

		template<typename _T1, typename _T2>
		inline bool
			operator!=(const allocator<_T1>&, const allocator<_T2>&)
		{
			return false;
		}

		template<typename _Tp>
		inline bool
			operator!=(const allocator<_Tp>&, const allocator<_Tp>&)
		{
			return false;
		}
	}	
	*/
	//template <class T>
	//using allocator = netp::__gnugcc_impl::allocator<T>;
	//template <class T>
	//using allocator = std::allocator<T>;
	//template <class T>
	//struct allocator :
	//	public std::allocator<T>
	//{};

}

namespace netp {
	struct memory_alloc_failed :
		public netp::exception
	{
		memory_alloc_failed(netp::size_t const& size, const char* const file, const int line, const char* const func) :
			exception(netp_last_errno(), "", file, line, func)
		{
			::memset(_message, 0, NETP_EXCEPTION_MESSAGE_LENGTH_LIMIT);
			int rt = snprintf(_message, NETP_EXCEPTION_MESSAGE_LENGTH_LIMIT, "memory alloc failed, alloc size: %zu", size);
			NETP_ASSERT((rt > 0) && (rt < NETP_EXCEPTION_MESSAGE_LENGTH_LIMIT));
			(void)rt;
		}
	};
}

#define NETP_ALLOC_CHECK(pointer, hintsize) \
	do { \
		if(pointer==nullptr) { \
			throw netp::memory_alloc_failed(hintsize, __FILE__, __LINE__,__FUNCTION__); \
		} \
	} while(0)

#endif
