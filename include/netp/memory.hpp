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

#include <netp/thread_impl/spin_mutex.hpp>
#include <netp/singleton.hpp>

#define NETP_MEMORY_USE_TLS_POOL 1
//#define NETP_MEMORY_USE_ALIGN_MALLOC 1
//#define NETP_MEMORY_USE_STD_MALLOC

namespace netp {

#define SLOT_MAX (8)
	enum TABLE {
		T0 = 0,
		T1,
		T2,
		T3,
		T4,
		T5,
		T6,
		T7,
		T8,
		T9,
		T10,
		T11,
		T12,
		T13,
		T_COUNT
	};

	//be careful, ptr should better aligned to 8bytes
	struct table_slot_t {
		u32_t max;
		u32_t count;
		//pointer to sizeof(u8_t*) * slot_max;
		u8_t** ptr; //

		//if count == slot_max, we move half of count into global
		//if count ==0, we borrow half of slot_max from global 
	};

#define TABLE_SLOT_COUNT(tst) (tst->count)
#define TABLE_SLOT_POP(tst) ( tst->ptr + sizeof(u8_t*) * (--tst->count)))
#define TABLE_SLOT_PUSH(tst,ptr) (tst->ptr + sizeof(u8_t*) * (tst->count++)))

	typedef table_slot_t* table_t;

	//NOTE: if want to share address with different alignment in the same pool, we need to check alignment and do a re-align if necessary
	class pool_aligned_allocator {

	protected:
		//pointer to the first table slot
		//not all the table has seem size
		table_slot_t* m_tables[TABLE::T_COUNT][SLOT_MAX];

		void preallocate_table_slot_item(table_slot_t* tst, u8_t t, u8_t slot, size_t item_count);
		void deallocate_table_slot_item(table_slot_t* tst);
		
		void init(bool preallocate);
		void deinit();


		public:
			pool_aligned_allocator( bool preallocate = true );
			virtual ~pool_aligned_allocator();

			void* malloc(size_t size, size_t alignment );
			void free(void* ptr);
			void* realloc(void* ptr, size_t size, size_t alignment);
	};

	class global_pool_aligned_allocator final :
		public pool_aligned_allocator,
		public singleton<global_pool_aligned_allocator>
	{
		spin_mutex m_table_slots_mtx[TABLE::T_COUNT][SLOT_MAX];
	public:
		global_pool_aligned_allocator();
		virtual ~global_pool_aligned_allocator();
		void incre_thread_count();
		void decre_thread_count();
		u32_t commit(u8_t t, u8_t slot, table_slot_t* tst);
		u32_t borrow(u8_t t, u8_t slot, table_slot_t* tst);
	};

	using pool_aligned_allocator_t = pool_aligned_allocator;

	struct tag_allocator_std_malloc {};
	struct tag_allocator_default_new {};
	struct tag_allocator_tls_pool {};

	//std::allocator<T> AA;
	template<class allocator_t>
	struct allocator_wrapper {
		__NETP_FORCE_INLINE static void* malloc(size_t n, size_t alignment = NETP_DEFAULT_ALIGN) {
			return 0;
		}
		__NETP_FORCE_INLINE static void* calloc(size_t n, size_t alignment = NETP_DEFAULT_ALIGN) {
			return 0;
		}
		__NETP_FORCE_INLINE static void free(void* p) {
		}
		__NETP_FORCE_INLINE static void* realloc(void* ptr, size_t size, size_t alignment = NETP_DEFAULT_ALIGN) {
			return 0;
		}
	};

	template<>
	struct allocator_wrapper<tag_allocator_std_malloc> {
		__NETP_FORCE_INLINE static void* malloc(size_t n, size_t alignment = NETP_DEFAULT_ALIGN) {
			(void)alignment;
			return std::malloc(n);
		}
		__NETP_FORCE_INLINE static void* calloc(size_t n, size_t alignment = NETP_DEFAULT_ALIGN) {
			(void)alignment;
			return std::calloc(1,n);
		}
		__NETP_FORCE_INLINE static void free(void* p) {
			std::free(p);
		}
		__NETP_FORCE_INLINE static void* realloc(void* ptr, size_t size, size_t alignment = NETP_DEFAULT_ALIGN) {
			(void)(alignment);
			return std::realloc(ptr, size);
		}
	};

	template<>
	struct allocator_wrapper<tag_allocator_default_new> {
		__NETP_FORCE_INLINE static void* malloc(size_t n, size_t alignment = NETP_DEFAULT_ALIGN) {
			(void)(alignment);
			return static_cast<void*>(::operator new(n));
		}
		__NETP_FORCE_INLINE static void* calloc(size_t , size_t alignment = NETP_DEFAULT_ALIGN) {
			(void)(alignment);
			throw std::bad_alloc();
		}
		__NETP_FORCE_INLINE static void free(void* p) {
			::operator delete(p);
		}
		
		__NETP_FORCE_INLINE static void* realloc(void* ptr, size_t size, size_t alignment = NETP_DEFAULT_ALIGN) {
			(void)(alignment);
			(void)ptr;
			(void)size;
			throw std::bad_alloc();
		}
	};

	template<>
	struct allocator_wrapper<tag_allocator_tls_pool> {
		__NETP_FORCE_INLINE static void* malloc(size_t n, size_t alignment = NETP_DEFAULT_ALIGN) {
			return tls_get<netp::pool_aligned_allocator>()->malloc(n, alignment);
		}
		__NETP_FORCE_INLINE static void* calloc(size_t n, size_t alignment = NETP_DEFAULT_ALIGN) {
			char* p = (char*)tls_get<netp::pool_aligned_allocator>()->malloc(n, alignment);
			//might it be optimized ? let's see
			std::memset(p, 0, n);
			return (void*)p;
		}
		__NETP_FORCE_INLINE static void free(void* p) {
			tls_get<netp::pool_aligned_allocator>()->free(p);
		}
		__NETP_FORCE_INLINE static void* realloc(void* ptr, size_t size, size_t alignment = NETP_DEFAULT_ALIGN) {
			return tls_get<netp::pool_aligned_allocator>()->realloc(ptr, size, alignment);
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
		__NETP_FORCE_INLINE static pointer malloc(size_t n, size_t alignment= NETP_DEFAULT_ALIGN) {
			return static_cast<pointer>(allocator_wrapper_t::malloc(sizeof(value_type) * n, alignment));
		}
		__NETP_FORCE_INLINE static pointer calloc(size_t n, size_t alignment = NETP_DEFAULT_ALIGN) {
			return static_cast<pointer>(allocator_wrapper_t::calloc(sizeof(value_type) * n, alignment));
		}
		__NETP_FORCE_INLINE static void free(pointer p) {
			allocator_wrapper_t::free(p);
		}
		__NETP_FORCE_INLINE static pointer realloc(pointer ptr, size_t size, size_t alignment = NETP_DEFAULT_ALIGN) {
			return static_cast<pointer>(allocator_wrapper_t::realloc(ptr,size,alignment));
		}

		template<class... _Args_t>
		inline static pointer make(_Args_t&&... _Args) {
			pointer p = static_cast<pointer>(allocator_wrapper_t::malloc(sizeof(value_type)));
			if (p != 0) {
				::new ((void*)p)(value_type)(std::forward<_Args_t>(_Args)...);
			}
			return p;
		}

		//no virutal sub destruct supported currently
		inline static void trash(pointer p) {
			if (p != 0) {
				p->~value_type();
				allocator_wrapper_t::free(p);
			}
		}

	private:
		template<class array_ele_t, class _allocator_base_t>
		struct __make_array_trait {
			//this is track
			//function template does not support partial specialised
			inline static array_ele_t* __make_array(size_t n, size_t alignment = NETP_DEFAULT_ALIGN) {
				array_ele_t* ptr = (_allocator_base_t::malloc(n, alignment));
				for (size_t i = 0; i < n; ++i) {
					//TODO: might throw exception
					//for standard impl, compiler would wrap these line with try...catch...delete
					//NOTE: refer to https://isocpp.org/wiki/faq/dtors
					//placement new have no related delete operation, we have to call destructor and free the memory by ourself
					::new((void*)(ptr + i))(array_ele_t)();
				}
				return ptr;
			}
			inline static void __trash_array(array_ele_t* ptr, size_t n) {
				for (size_t i = 0; i < n; ++i) {
					ptr[i].~array_ele_t();
				}
				_allocator_base_t::free(ptr);
			}
		};

		template<class _allocator_base_t>
		struct __make_array_trait<byte_t, _allocator_base_t> {
			//this is a track
			//function template does not support partial specialised
			inline static byte_t* __make_array(size_t n, size_t alignment = NETP_DEFAULT_ALIGN) {
				return (_allocator_base_t::malloc(n, alignment));
			}
			inline static void __trash_array(byte_t* ptr, size_t n) {
				_allocator_base_t::free(ptr);
				(void)n;
			}
		};

		template<class _allocator_base_t>
		struct __make_array_trait<u16_t, _allocator_base_t> {
			//this is track
			//function template does not support partial specialised
			inline static u16_t* __make_array(size_t n, size_t alignment = NETP_DEFAULT_ALIGN) {
				return (_allocator_base_t::malloc(n, alignment));
			}
			inline static void __trash_array(u16_t* ptr, size_t n) {
				_allocator_base_t::free(ptr);
				(void)n;
			}
		};

		template<class _allocator_base_t>
		struct __make_array_trait<u32_t, _allocator_base_t> {
			//this is track
			//function template does not support partial specialised
			inline static u32_t* __make_array(size_t n, size_t alignment = NETP_DEFAULT_ALIGN) {
				return (_allocator_base_t::malloc(n, alignment));
			}
			inline static void __trash_array(u32_t* ptr, size_t n) {
				_allocator_base_t::free(ptr);
				(void)n;
			}
		};

		template<class _allocator_base_t>
		struct __make_array_trait<u64_t, _allocator_base_t> {
			//this is track
			//function template does not support partial specialised
			inline static u64_t* __make_array(size_t n, size_t alignment = NETP_DEFAULT_ALIGN) {
				return (_allocator_base_t::malloc(n, alignment));
			}
			inline static void __trash_array(u64_t* ptr, size_t n) {
				_allocator_base_t::free(ptr);
				(void)n;
			}
		};

	public:
		inline static pointer make_array(size_t n, size_t alignment = NETP_DEFAULT_ALIGN) {
			return __make_array_trait<value_type, allocator_base_t>::__make_array(n, alignment);
		}

		inline static void trash_array(pointer ptr, size_t n) {
			__make_array_trait<value_type, allocator_base_t>::__trash_array(ptr, n);
		}

		//for container compliance below
		inline pointer allocate(size_type n) {
			return allocator_base_t::malloc(n);
		}

		inline void deallocate(pointer p, size_type n) {
			allocator_base_t::free(p);
			(void)n;
		}

		//for stl container
		template<class _Objty,
			class... _Types>
		inline void construct(_Objty* _Ptr, _Types&&... _Args)
		{	// construct _Objty(_Types...) at _Ptr
			//all the object created instanced by operator placement new must not be called by operate delete.
			//we have to call destructor by ourself first, then do memory free by ourown logic
			//so, construct&destroy must be paired
#ifdef _NETP_DEBUG
			NETP_ASSERT(_Ptr != 0);
#endif
			::new ((void*)_Ptr) _Objty( std::forward<_Types>(_Args)...);
		}

		//for stl container
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
	struct allocator_pool :
		public allocator_base<T, allocator_wrapper<tag_allocator_tls_pool>>
	{
		typedef  allocator_base<T, allocator_wrapper<tag_allocator_tls_pool>> allocator_base_t;
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
#ifdef NETP_MEMORY_USE_TLS_POOL
	using allocator = netp::allocator_pool<T>;
#elif defined(NETP_MEMORY_USE_ALIGN_MALLOC)
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
