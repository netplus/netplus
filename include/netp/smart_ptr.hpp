#ifndef _NETP_SMARTPTR_HPP_
#define _NETP_SMARTPTR_HPP_

#include <netp/core/compiler.hpp>
#include <netp/exception.hpp>
#include <netp/memory.hpp>
#include <netp/funcs.hpp>

namespace netp {


	struct weak_ptr_lock_faild :
		public netp::exception
	{
		weak_ptr_lock_faild():
			exception(netp::E_MEMORY_ACCESS_ERROR,"weak_ptr_lock_failed", __FILE__, __LINE__,__FUNCTION__)
		{
		}
	};

	struct sp_counter_impl_malloc_failed:
		public netp::exception
	{
		sp_counter_impl_malloc_failed():
			exception(netp::E_MEMORY_ALLOC_FAILED, "sp_counter_impl_malloc_failed", __FILE__, __LINE__,__FUNCTION__)
		{}
	};
	
	template <class T>
	inline void checked_delete(T* p) {
		typedef char is_this_a_complete_type[sizeof(T)?1:-1];
		(void) sizeof(is_this_a_complete_type);
		delete p;
	}


	/*
	 * usage
	 * if you want to make a object that would be shared but not be copyed, you need a ref object
	 * if you want to make a object that would be shared and it can be copyed, you need shared_ptr
	 *
	 */

	/*
	* two reason result in below classes
	* 1, sizeof(netp::shared_ptr) == 4, compared to sizeof(std::shared_ptr)==8 (it is not so important!!!!)
	* 2, safe store in stl's container
	*/
	
	//@notice, the ref is thread safe, but not the __p, user have to place atomic_thread_fence accordingly (write|read)

	//@todo we should work out a better way to access __p, it is possbile!!!
	//@what we want 
	class __sp_atomic_counter_base {
	public:
		void* __p;
	private:
		std::atomic<long> sp_c;
		std::atomic<long> sp_weak_c;

		NETP_DECLARE_NONCOPYABLE(__sp_atomic_counter_base)
	public:
		__sp_atomic_counter_base(void* const& p):
			__p(p),
			sp_c(1),
			sp_weak_c(1)
		{}

		virtual ~__sp_atomic_counter_base() {}
		virtual void dispose() = 0;

		virtual void destroy() {
			delete this;
		}

		inline long sp_count() const { return sp_c.load(std::memory_order_acquire);}
		inline long weak_count() const { return sp_weak_c.load(std::memory_order_acquire);}
		inline void require() {sp_c.fetch_add(1,std::memory_order_acq_rel);}
		inline long require_lock(long const& val) {
			return netp::atomic_incre_if_not_equal(&sp_c,val,std::memory_order_acq_rel, std::memory_order_acquire);
		}
		inline void release() {
			if( sp_c.fetch_sub(1,std::memory_order_acq_rel) == 1) {
				dispose();
				weak_release();
			}
		}
		void weak_require() { sp_weak_c.fetch_add(1,std::memory_order_acq_rel);}
		void weak_release() {
			if( sp_weak_c.fetch_sub(1,std::memory_order_acq_rel) == 1 ) {
				destroy();
			}
		}

		void* operator new(size_t sz) {
			return netp::allocator<char>::malloc(sz);
		}

		void operator delete(void* m) {
			return netp::allocator<char>::free((char*)m);
		}
	};

	class __sp_non_atomic_counter_base {
	public:
		void* __p;
	private:
		long sp_c;
		long sp_weak_c;

		NETP_DECLARE_NONCOPYABLE(__sp_non_atomic_counter_base)
	public:
		__sp_non_atomic_counter_base(void* const& p) :
			__p(p),
			sp_c(1),
			sp_weak_c(1)
		{}

		virtual ~__sp_non_atomic_counter_base() {}
		virtual void dispose() = 0;

		virtual void destroy() {
			delete this;
		}

		inline long sp_count() const { return sp_c; }
		inline long weak_count() const { return sp_weak_c; }
		inline void require() { sp_c +=1; }
		inline long require_lock(long const& val) {
			if (sp_c == val) {
				return sp_c;
			}
			return ++sp_c;
		}

		inline void release() {
			if (sp_c-- == 1) {
				//no competitor assumption
				dispose();
				weak_release();
			}
		}
		void weak_require() { sp_weak_c +=1; }
		void weak_release() {
			if (sp_weak_c-- == 1) {
				destroy();
			}
		}
		void* operator new(size_t sz) {
			return netp::allocator<char>::malloc(sz);
		}

		void operator delete(void* m) {
			return netp::allocator<char>::free((char*)m);
		}
	};

#if defined(_NETP_WIN) && (defined(_NETP_DEBUG) || 1)
	#define NETP_ADD_PT_IN_IMPL_FOR_DEBUG
#endif

	template <class pt, class sp_count_base_t>
	class __sp_counter_impl_p:
		public sp_count_base_t
	{
#ifdef NETP_ADD_PT_IN_IMPL_FOR_DEBUG
		pt* _pt;
#endif
		NETP_DECLARE_NONCOPYABLE(__sp_counter_impl_p)
		typedef __sp_counter_impl_p<pt, sp_count_base_t> this_type;
	public:
		explicit __sp_counter_impl_p(pt* const& p): sp_count_base_t(p)
#ifdef NETP_ADD_PT_IN_IMPL_FOR_DEBUG
			,_pt(p)
#endif
		{}
		virtual void dispose() { netp::checked_delete<pt>(static_cast<pt*>(sp_count_base_t::__p)); sp_count_base_t::__p=0;}
	};

	template <class _sp_counter_base_t>
	struct __sp_weak_counter;

	template <class _sp_counter_base_t>
	struct __sp_counter final {
		
		typedef __sp_weak_counter<_sp_counter_base_t> __sp_weak_counter_t;
		typedef __sp_counter<_sp_counter_base_t> __sp_counter_t;
		_sp_counter_base_t* base;

		__sp_counter():
			base(nullptr)
		{}

		template <class pt>
		explicit __sp_counter(pt* const& p): base(0) {
			try {
				base = new __sp_counter_impl_p<pt, _sp_counter_base_t>(p);
			} catch(...) {
				throw netp::sp_counter_impl_malloc_failed();
			}
		}

		~__sp_counter() _NETP_NOEXCEPT { if( base != nullptr) base->release();}

		inline __sp_counter(__sp_counter_t const& r) _NETP_NOEXCEPT :
			base(r.base)
		{
			if( base != 0 ) base->require();
		}

		inline __sp_counter(__sp_counter_t&& r) _NETP_NOEXCEPT:
			base(r.base)
		{
			r.reset();
		}

		inline __sp_counter(__sp_weak_counter_t const& weak) :
			base(weak->base)
		{
			if (base != 0 && base->require_lock(0)) {
				NETP_ASSERT(base->__p != 0);
				NETP_ASSERT(base->sp_count() > 0);
			} else {
				base = 0;
			}
		}

		inline __sp_counter_t& operator = (__sp_counter_t const& r) _NETP_NOEXCEPT {
			NETP_ASSERT( base == r.base ? base == nullptr: true );
			__sp_counter_t(r).swap( *this );
			return (*this);
		}

		inline __sp_counter_t& operator = (__sp_counter_t&& r) _NETP_NOEXCEPT {
			NETP_ASSERT(base == r.base ? base == nullptr : true);
			if (base != 0) base->release();
			base = r.base;
			r.reset();
			return (*this);
		}

		inline void swap(__sp_counter_t& r ) _NETP_NOEXCEPT {
			std::swap(base,r.base);
		}

		inline void reset() _NETP_NOEXCEPT {
			base = 0;
		}

		inline long sp_count() const {
			if(base) return base->sp_count();
			return 0;
		}

		inline long weak_count() const {
			if (base) return base->sp_count();
			return 0;
		}

		inline bool operator == (__sp_counter_t const& r ) const {
			return base == r.base;
		}

		inline bool operator != (__sp_counter_t const& r ) const {
			return base != r.base;
		}

		inline bool operator < (__sp_counter_t const& r ) const {
			return base < r.base;
		}
		inline bool operator > (__sp_counter_t const& r ) const {
			return base > r.base;
		}
	};

	template <class _sp_counter_base_t>
	struct __sp_weak_counter final {
		typedef __sp_weak_counter<_sp_counter_base_t> __sp_weak_counter_t;
		typedef __sp_counter<_sp_counter_base_t> __sp_counter_t;

		_sp_counter_base_t* base;
		__sp_weak_counter(): base(0) {}
		__sp_weak_counter(__sp_weak_counter_t const& counter ):
			base(counter.base)
		{
			if( base != 0 ) base->weak_require();
		}

		__sp_weak_counter(__sp_counter_t const& counter ):
			base(counter.base)
		{
			if( base != 0 ) base->weak_require();
		}
		~__sp_weak_counter() {
			if( base != 0 ) base->weak_release();
		}

		__sp_weak_counter_t& operator = (__sp_weak_counter_t const& weak_counter ) {
			NETP_ASSERT( base != weak_counter.base );
			__sp_weak_counter_t(weak_counter).swap(*this);
			return *this;
		}

		__sp_weak_counter_t& operator = (__sp_counter_t const& sp_counter) {
			NETP_ASSERT( base != sp_counter.base );
			__sp_weak_counter_t(sp_counter).swap(*this);
			return *this;
		}

		void swap(__sp_weak_counter_t& r ) _NETP_NOEXCEPT {
			std::swap(base,r.base);
		}

		bool operator == (__sp_weak_counter_t const& r ) const {
			return base == r.base;
		}

		bool operator != (__sp_weak_counter_t const& r) const {
			return base != r.base;
		}
		bool operator < (__sp_weak_counter_t const& r) const {
			return base < r.base;
		}
		bool operator > (__sp_weak_counter_t const& r) const {
			return base > r.base;
		}
	};

	template <class T, class __counter_t>
	class __weak_ptr_impl;

	template <class __counter_t>
	struct __shared_maker_impl;

	enum class construct_from_make_shared {};

	template <class T, class __counter_t>
	class __shared_ptr_impl final {

		friend class __weak_ptr_impl<T,__counter_t>;
		friend struct __shared_maker_impl<__counter_t>;

		//___counter_ should be __counter_t, but c++ do not support nested class specialize
		template <class Y, class ___counter_t>
		friend class __shared_ptr_impl;

		typedef typename std::remove_reference<T>::type __ELEMENT_TYPE;
		typedef __ELEMENT_TYPE* POINTER_TYPE;
		typedef __shared_ptr_impl<T,__counter_t> THIS_TYPE;
		typedef __weak_ptr_impl<T, __counter_t> WEAK_POINTER_TYPE;

		template <class _To, class _From>
		friend __shared_ptr_impl<_To,__counter_t> static_pointer_cast(__shared_ptr_impl<_From, __counter_t> const& r);

		template <class _To, class _From>
		friend __shared_ptr_impl<_To, __counter_t> dynamic_pointer_cast(__shared_ptr_impl<_From, __counter_t> const& r);

		template <class _To, class _From>
		friend __shared_ptr_impl<_To, __counter_t> const_pointer_cast(__shared_ptr_impl<_From, __counter_t> const& r);

		__sp_counter<__counter_t> sp_ct;

		//_Tp_rel related type
		/*
		template <typename _Tp_rel
			, class = typename std::enable_if<std::is_convertible<_Tp_rel*, POINTER_TYPE>::value>::type>
			explicit shared_ptr(_Tp_rel* const& r, construct_from_make_shared) :
			sp_ct(r)
		{
		}
		*/

		explicit inline __shared_ptr_impl(POINTER_TYPE const& r, construct_from_make_shared) :
			sp_ct(r)
		{
		}

	public:
		_NETP_CONSTEXPR __shared_ptr_impl() _NETP_NOEXCEPT:sp_ct() {}
		_NETP_CONSTEXPR __shared_ptr_impl(std::nullptr_t) _NETP_NOEXCEPT : sp_ct() {}

		~__shared_ptr_impl() _NETP_NOEXCEPT {}
		long use_count() const _NETP_NOEXCEPT { return sp_ct.sp_count(); }

		inline __shared_ptr_impl(THIS_TYPE const& r) _NETP_NOEXCEPT:
			sp_ct(r.sp_ct)
		{}

		inline __shared_ptr_impl(THIS_TYPE&& r) _NETP_NOEXCEPT :
			sp_ct(std::move(r.sp_ct))
		{}

		template <class _From>
		inline __shared_ptr_impl(__shared_ptr_impl<_From, __counter_t> const& r, POINTER_TYPE) _NETP_NOEXCEPT:
			sp_ct(r.sp_ct)
		{ //for cast
		}

		//_Tp_rel related type
		template <typename _Tp_rel
#ifdef _NETP_NO_CXX11_CLASS_TEMPLATE_DEFAULT_TYPE
		>
		shared_ptr( shared_ptr<_Tp_rel> const& r,typename
			std::enable_if<std::is_convertible<_Tp_rel*, ELEMENT_TYPE*>::value>::type ** = 0) _NETP_NOEXCEPT
#else
		, class = typename std::enable_if<std::is_convertible<_Tp_rel*, POINTER_TYPE>::value>::type
		>
		inline __shared_ptr_impl(__shared_ptr_impl<_Tp_rel, __counter_t> const& r) _NETP_NOEXCEPT
#endif
			:sp_ct(r.sp_ct)
		{
		}

		template <typename _Tp_rel
#ifdef _NETP_NO_CXX11_CLASS_TEMPLATE_DEFAULT_TYPE
		>
		shared_ptr(shared_ptr<_Tp_rel>&& r,typename
			std::enable_if<std::is_convertible<_Tp_rel*, ELEMENT_TYPE*>::value>::type ** = 0) _NETP_NOEXCEPT
#else
		, class = typename std::enable_if<std::is_convertible<_Tp_rel*, POINTER_TYPE>::value>::type
		>
		inline __shared_ptr_impl(__shared_ptr_impl<_Tp_rel, __counter_t>&& r) _NETP_NOEXCEPT
#endif
			: sp_ct(std::move(r.sp_ct))
		{
		}

		inline THIS_TYPE& operator=(THIS_TYPE const& r) _NETP_NOEXCEPT
		{
			THIS_TYPE(r).swap(*this);
			return (*this);
		}

		inline THIS_TYPE& operator=(THIS_TYPE&& r) _NETP_NOEXCEPT
		{
			sp_ct = std::move(r.sp_ct);
			return (*this);
		}

		template <typename _Tp_rel>
		inline THIS_TYPE& operator= (__shared_ptr_impl<_Tp_rel, __counter_t> const& r ) _NETP_NOEXCEPT
		{
			THIS_TYPE(r).swap(*this);
			return (*this) ;
		}

		template <typename _Tp_rel>
		inline THIS_TYPE& operator= (__shared_ptr_impl<_Tp_rel, __counter_t>&& r) _NETP_NOEXCEPT
		{
			sp_ct = std::move(r.sp_ct);
			return (*this);
		}

		inline void swap( THIS_TYPE& r ) _NETP_NOEXCEPT {
			std::swap( sp_ct, r.sp_ct );
		}

		inline __shared_ptr_impl(WEAK_POINTER_TYPE const& weak) _NETP_NOEXCEPT:
			sp_ct(weak.weak_ct)
		{
		}

		/*
		* @note: for operator-> and operator*
		*
		* to follow standard c++ impl, we have to set T const& -> T&, and remove the none const version
		*/

		__NETP_FORCE_INLINE POINTER_TYPE operator -> () const {
#ifdef _NETP_DEBUG
			NETP_ASSERT( sp_ct.base != 0 );
			NETP_ASSERT( sp_ct.base->__p != 0 );
#endif
			return static_cast<POINTER_TYPE>(sp_ct.base->__p);
		}

		__NETP_FORCE_INLINE T& operator * () const {
#ifdef _NETP_DEBUG
			NETP_ASSERT(sp_ct.base != 0);
			NETP_ASSERT(sp_ct.base->__p != 0);
#endif
			return *(static_cast<POINTER_TYPE>(sp_ct.base->__p));
		}
		/*
		inline T& operator * () {
			NETP_ASSERT(sp_ct.base != 0);
			NETP_ASSERT(sp_ct.base->__p != 0);
			return *(static_cast<POINTER_TYPE>(sp_ct.base->__p));
		}
		*/

		inline POINTER_TYPE get() const {
			if (sp_ct.base == 0) return 0;
			return static_cast<POINTER_TYPE>(sp_ct.base->__p);
		}

		inline void reset() { sp_ct.reset(); }

		inline bool operator ==(THIS_TYPE const& r) const { return sp_ct == r.sp_ct; }
		inline bool operator !=(THIS_TYPE const& r) const { return sp_ct != r.sp_ct; }

		inline bool operator < (THIS_TYPE const& r) const { return sp_ct < r.sp_ct; }
		inline bool operator > (THIS_TYPE const& r) const { return sp_ct > r.sp_ct; }

		inline bool operator ==(POINTER_TYPE const& rp) const { return get() == rp; }
		inline bool operator !=(POINTER_TYPE const& rp) const { return get() != rp; }
	};

	template <class T, class _counter_t>
	class __weak_ptr_impl final {
		friend class __shared_ptr_impl<T, _counter_t>;

		typedef typename std::remove_reference<T>::type __ELEMENT_TYPE;
		typedef __ELEMENT_TYPE* POINTER_TYPE;
		typedef __weak_ptr_impl<T, _counter_t> THIS_TYPE;
		typedef __shared_ptr_impl<T, _counter_t> SHARED_POINTER_TYPE;
		
		__sp_weak_counter<_counter_t> weak_ct;

	public:
		__weak_ptr_impl() :weak_ct() {}
		explicit __weak_ptr_impl(SHARED_POINTER_TYPE const& auto_point) :
			weak_ct(auto_point.sp_ct)
		{
		}
		~__weak_ptr_impl() {}

		__weak_ptr_impl(THIS_TYPE const& r) :
			weak_ct(r.weak_ct)
		{
		}

		void swap(THIS_TYPE& r) {
			std::swap(r.weak_ct, weak_ct);
		}

		THIS_TYPE& operator = (THIS_TYPE const& r) {
			weak_ptr(r).swap(*this);
			return *this;
		}

		THIS_TYPE& operator = (SHARED_POINTER_TYPE const& r) {
			weak_ptr(r).swap(*this);
			return *this;
		}

		//return shared_ptr if lock succeed, otherwise return std::shared_ptr<T,_counter>(nullptr);
		inline SHARED_POINTER_TYPE lock() const {
			return SHARED_POINTER_TYPE(*this);
		}

		inline bool operator == (THIS_TYPE const& r) const { return weak_ct == r.weak_ct; }
		inline bool operator != (THIS_TYPE const& r) const { return weak_ct != r.weak_ct; }
	};

	template<class T>
	using shared_ptr = netp::__shared_ptr_impl<T, netp::__sp_atomic_counter_base>;

	template <class T>
	using weak_ptr = netp::__weak_ptr_impl<T, netp::__sp_atomic_counter_base>;

	template<class T>
	using non_atomic_shared_ptr = netp::__shared_ptr_impl<T, netp::__sp_non_atomic_counter_base>;

	template <class T>
	using non_atomic_weak_ptr = netp::__weak_ptr_impl<T, netp::__sp_non_atomic_counter_base>;

#ifdef _NETP_NO_CXX11_TEMPLATE_VARIADIC_ARGS
#define _ALLOCATE_MAKE_SHARED( \
	TEMPLATE_LIST, PADDING_LIST, LIST, COMMA, X1, X2, X3, X4) \
template<class _Ty COMMA LIST(_CLASS_TYPE)> inline \
	shared_ptr<_Ty> make_shared(LIST(_TYPE_REFREF_ARG)) \
	{	/* make a shared_ptr */ \
		_Ty *_Rx = \
		new _Ty(LIST(_FORWARD_ARG)); \
		NETP_ALLOC_CHECK(_Rx); \
		return shared_ptr<_Ty>(_Rx); \
	}
_VARIADIC_EXPAND_0X(_ALLOCATE_MAKE_SHARED, , , , )
#undef _ALLOCATE_MAKE_SHARED
#else

	template <class _counter_t>
	struct __shared_maker_impl {
		template <class _TTM, typename... _Args>
		inline static __shared_ptr_impl<_TTM, _counter_t> make(_Args&&... args)
		{
			//@@pls note that , if we use ::new here, the operator new would be skiped
			_TTM* t = new _TTM(std::forward<_Args>(args)...);
			//NETP_ALLOC_CHECK(t,sizeof(_TTM));
			return __shared_ptr_impl<_TTM, _counter_t>(t, construct_from_make_shared());
		}
	};

	using atomic_shared = __shared_maker_impl<netp::__sp_atomic_counter_base>;
	using non_atomic_shared = __shared_maker_impl<netp::__sp_non_atomic_counter_base>;

	template <class _TTM, typename... _Args>
	inline static shared_ptr<_TTM> make_shared(_Args&&... args)
	{
		return atomic_shared::make<_TTM>(std::forward<_Args>(args)...);
	}

	template <class _TTM, typename... _Args>
	inline static non_atomic_shared_ptr<_TTM> make_non_atomic_shared(_Args&&... args)
	{
		return non_atomic_shared::make<_TTM>(std::forward<_Args>(args)...);
	}
#endif


	/*
	 * test case for the following three functions

	 struct Base {
	 int i;
	 Base() :i(0) {}
	 Base(int ii) :i(ii) {}
	 virtual ~Base() {
	 std::cout << "Base(" << i << ") destruct" << std::endl;
	 }
	 };
	 struct Sub : public Base {
	 Sub() :Base(0) {}
	 Sub(int s) :Base(s) {}
	 };

	 std::shared_ptr<Sub> sub;
	 std::shared_ptr<Sub> sub_d;
	 std::shared_ptr<Base> base = std::make_shared<Sub>();
	 sub = std::static_pointer_cast<Sub, Base>(base);
	 sub_d = std::dynamic_pointer_cast<Sub, Base>(base);

	 std::shared_ptr<const Sub> csub;
	 csub = std::static_pointer_cast<Sub, Base>(base);
	 //csub->i = 10;

	 std::shared_ptr<Sub> csub_after_const_cast = std::const_pointer_cast<Sub,const Sub>(csub);
	 csub_after_const_cast->i = 10;


	 //std::shared_ptr<int> intx = std::make_shared<int>();
	 //sub_d = std::dynamic_pointer_cast<Sub, int>(intx);

	 //Sub* sub;
	 //Base* base = new Sub();
	 //sub = base;


	 struct Rep : public netp::ref_base {
	 int i;
	 Rep() {}
	 ~Rep() {}
	 };
	 struct RepSub : public Rep {
	 ~RepSub() {};
	 };

	 netp::shared_ptr<Sub> wsub;
	 netp::shared_ptr<Sub> wsub_d;
	 netp::shared_ptr<const Sub> wcsub;
	 netp::shared_ptr<Base> wbase = netp::make_shared<Sub>();
	 wsub = netp::static_pointer_cast<Sub, Base>(wbase);
	 wsub_d = netp::dynamic_pointer_cast<Sub, Base>(wbase);

	 wcsub = netp::static_pointer_cast<Sub, Base>(wbase);
	 netp::shared_ptr<Sub> wcsub_after_const_cast = netp::const_pointer_cast<Sub, const Sub>(wcsub);
	 wcsub_after_const_cast->i = 10;

	 //netp::shared_ptr<int> wintx = netp::make_shared<int>();
	 //wsub_d = netp::dynamic_pointer_cast<Sub, int>(wintx);
	 *
	 */

	template <class _To, class _From>
	inline shared_ptr<_To> static_pointer_cast(shared_ptr<_From> const& r )
	{
		//check castable
		typename shared_ptr<_To>::POINTER_TYPE p= static_cast< typename shared_ptr<_To>::POINTER_TYPE >( r.get() );
		return shared_ptr<_To>(r,p);
	}

	template <class _To, class _From>
	inline shared_ptr<_To> dynamic_pointer_cast( shared_ptr<_From> const& r )
	{
		//check castable
		typename shared_ptr<_To>::POINTER_TYPE p= dynamic_cast< typename shared_ptr<_To>::POINTER_TYPE >( r.get() );
		if(NETP_LIKELY(p)) return shared_ptr<_To>(r,p);
		return shared_ptr<_To>();
	}

	template <class _To, class _From>
	shared_ptr<_To> const_pointer_cast(shared_ptr<_From> const& r )
	{
		typename shared_ptr<_To>::POINTER_TYPE p= const_cast< typename shared_ptr<_To>::POINTER_TYPE >( r.get() );
		return shared_ptr<_To>(r,p);
	}

	template <class _To, class _From>
	inline non_atomic_shared_ptr<_To> static_pointer_cast(non_atomic_shared_ptr<_From> const& r)
	{
		//check castable
		typename non_atomic_shared_ptr<_To>::POINTER_TYPE p = static_cast<typename non_atomic_shared_ptr<_To>::POINTER_TYPE>(r.get());
		return non_atomic_shared_ptr<_To>(r, p);
	}

	template <class _To, class _From>
	inline non_atomic_shared_ptr<_To> dynamic_pointer_cast(non_atomic_shared_ptr<_From> const& r)
	{
		//check castable
		typename non_atomic_shared_ptr<_To>::POINTER_TYPE p = dynamic_cast<typename non_atomic_shared_ptr<_To>::POINTER_TYPE>(r.get());
		if (NETP_LIKELY(p)) return non_atomic_shared_ptr<_To>(r, p);
		return non_atomic_shared_ptr<_To>();
	}

	template <class _To, class _From>
	non_atomic_shared_ptr<_To> const_pointer_cast(non_atomic_shared_ptr<_From> const& r)
	{
		typename non_atomic_shared_ptr<_To>::POINTER_TYPE p = const_cast<typename non_atomic_shared_ptr<_To>::POINTER_TYPE>(r.get());
		return non_atomic_shared_ptr<_To>(r, p);
	}
}


#define NSP netp::shared_ptr
#define NWP netp::weak_ptr

#define NNASP netp::non_atomic_shared_ptr
#define NNAWP netp::non_atomic_weak_ptr


#include <type_traits>

namespace netp {

	template <class _Ref_t>
	class ref_ptr;

	/**
 * @Warning: the following behaviour is undefined
 *
 * Access ref_ptr<T> from multi-threads prior to keep a copy of ref_ptr<T> object
 */

	struct __atomic_counter:
		private std::atomic<long>
	{
		__atomic_counter():
			atomic<long>(1)
		{}
		typedef std::atomic<long> __atomic_p_t;
		__NETP_FORCE_INLINE void __ref_grab() {
			// @QUESTION: should we add a release barrier here to sure all modification happens before this line (assigning a ref_ptr to another object) be flushed to other cpus, and stop compiler to reorder across this line ?
			
			// @NOTE, the currently impl of __atomic_counter
			// it is just a counter, the only gurantee is that the couting is thead safe
			// if we use it to impl a ref based object as below <this is what netp do>:
			// struct ref_obj : public __atomic_counter {
			//		int i;
			//		void bar() { ++i; }
			//		...
			// };
			// 
			// thread 1 :
			// ref_obj* ptr = new ref_obj();
			//
			// then pass ptr to other thread 2, and call a member function as below
			// 	thread 2: 
			// ptr->bar();
			// 
			// It is absolutely a ub, cuz the the cache on thead2's cpu might not be ready (still in the origin cpu's store buffer)
			//		1, ptr itself
			//		2, object's member if any (here is ref_obj::i)
			// 
			// to fix this, we have to do as the following steps
			// (1) place a release barrier right after the line of opeation new 
			// (2) place a acquire barrier right before dereferencing of ptr

			//@NOTE: new impl utilize std::memory_order_acq_rel, the step 1 could be saved, but acquire barrier is still needed on the other thread
			// 
			// the situation goes even more complicated in the following scenario

			//step 1:
			//thread 1
			// obj* ptr = new ref_obj();
			// std::atomic_memory_fence(std::memory_order_release);

			//step 2:
			//pass the ptr to thread 2, and call bar on thread 2
			// std::atomic_memory_fence(std::memory_order_acquire);
			// ptr->bar();

			//step 3:
			//thread 1, we update value of i
			//ptr->i = 3;
			//again, we have to place a release barrier to enable the visibility of the latest i for thread 2
			//@NOTE: memory_order_release is just nop on x86 cpu, but it can stops compiler's reorder
			//std::atomic_thread_fence(std::memory_order_release);

			//step4:
			//thread2 , call bar again
			//@NOTE: memory_order_acquire is just nop on x86 cpu, but it can stops compiler's reorder
			//std::atomic_thread_fence(std::memory_order_acquire)
			//ptr->bar();

			//these kind of operation is non-trivial, and erron-prone, do you have a better way here ?
			//netp suggests to use event_loop::execute() to pass messages to other threads always

			//@NOTE: std::memory_order_ralaxed, std::memory_order_acq_rel produce same(lock xadd: it's a full barrier) asm code on x86 [g++ -o2&msvc release]
			//@NOTE: std::memory_order_ralaxed, std::memory_order_acq_rel produce same asm code(with dmb, it's a full barrier) code on armv7a g++ -o2
			// 
			//but we might miss a compiler barrier on other platform according to https://en.cppreference.com/w/cpp/atomic/memory_order
			//in short: Relaxed operation: there are no synchronization or ordering constraints imposed on other reads or writes, only this operation's atomicity is guaranteed (see Relaxed ordering below)
			//! to sure compiler's offensive optimization,,, we force a std::atomic_signal_fence() here again 
			//! it is used to prevent any construct code be moved below the ref_ptr's assigning operation

			//note: std::atomic_signal_fence(std::memory_order_acq_rel) + __atomic_p_t::fetch_add(1, std::memory_order_relaxed) == __atomic_p_t::fetch_add(1, std::memory_order_acq_rel);
			//std::atomic_signal_fence(std::memory_order_acq_rel);
			NETP_ASSERT(0 < __atomic_p_t::load(std::memory_order_relaxed));
			//__atomic_p_t::fetch_add(1, std::memory_order_relaxed);
			__atomic_p_t::fetch_add(1, std::memory_order_acq_rel);
		}

		__NETP_FORCE_INLINE bool __ref_drop() {
			NETP_ASSERT(0 < __atomic_p_t::load(std::memory_order_relaxed));
			//@acq_rel guard delete operation,
			//@it make sure all modification happens before delete be synchronized to other cpu, and make sure there is no compiler reorder across decre
			return (__atomic_p_t::fetch_sub(1, std::memory_order_acq_rel) == 1);
		}
		__NETP_FORCE_INLINE long __ref_count() const { 
			//force a acq compiler fence to stop reorder in between atomic[volatile] and non-atomic access
			//note: volatile and compiler fence does not prevent cpu reorder
			//note: std::atomic_signal_fence(std::memory_order_acquire) + __atomic_p_t::load(std::memory_order_relaxed) == should equal std::atomic<>::load(std::memory_order_acquire); 
			//so just update it to load with acquire order
			
			//std::atomic_signal_fence(std::memory_order_acquire);
			//return __atomic_p_t::load(std::memory_order_relaxed); 
			return __atomic_p_t::load(std::memory_order_acquire);
		}
	};

	struct __non_atomic_counter
	{
		long __counter;
		__non_atomic_counter() :
			__counter(1)
		{}

		__NETP_FORCE_INLINE void __ref_grab() {
			NETP_ASSERT(0 < __counter);
			++__counter;
		}
		__NETP_FORCE_INLINE bool __ref_drop() {
			NETP_ASSERT(0 < __counter);
			return (__counter-- == 1);
		}
		__NETP_FORCE_INLINE long __ref_count() const { return __counter; }
	};
	
	template<class ref_counter>
	class ref_base_internal:
		private ref_counter
	{
		NETP_DECLARE_NONCOPYABLE(ref_base_internal)

		template <class _Ref_t>
		friend class ref_ptr;
		
#ifdef _NETP_NO_CXX11_TEMPLATE_VARIADIC_ARGS
#define _ALLOCATE_MAKE_REF( \
	TEMPLATE_LIST, PADDING_LIST, LIST, COMMA, X1, X2, X3, X4) \
template<class _Ty COMMA LIST(_CLASS_TYPE)> \
	friend ref_ptr<_Ty> make_ref(LIST(_TYPE_REFREF_ARG)) ;
		_VARIADIC_EXPAND_0X(_ALLOCATE_MAKE_REF, , , , )
#undef _ALLOCATE_MAKE_REF
#else
		template <class _Ref_ty, typename... _Args>
		friend ref_ptr<_Ref_ty> make_ref(_Args&&... args);
#endif

		typedef ref_counter ref_counter_t;
	protected:
		ref_base_internal()
			: ref_counter()
		{}
		virtual ~ref_base_internal() {}
	private:
		__NETP_FORCE_INLINE void _ref_grab() {
			ref_counter_t::__ref_grab();
		}
		__NETP_FORCE_INLINE void _ref_drop() {
			if (ref_counter::__ref_drop()) { delete this; }
		}
		__NETP_FORCE_INLINE long _ref_count() const { return ref_counter::__ref_count(); }

	protected:
		void* operator new(std::size_t size) {
			return static_cast<void*>(netp::allocator<char>::malloc(size));
		}
		/*
		 @note
		* when overload operator delete with access level of private
		* on MSVC, it's OK
		* but
		* on GNUGCC, compiler give a access error for ~Sub calling a ~Parent
		* I don't know why!
		*/
	protected:
		void operator delete(void* p) {
			if (p != 0) {
				netp::allocator<char>::free((char*)p);
			}
		}
	private:
#ifdef _NETP_NO_CXX11_DELETED_FUNC
		void* operator new[](std::size_t size);
		void  operator delete[](void* p, std::size_t size);
#else
		void* operator new[](std::size_t size) = delete;
		void  operator delete[](void* p, std::size_t size) = delete;
#endif
	};

	using atomic_ref_base = ref_base_internal<__atomic_counter>;
	using ref_base = atomic_ref_base;
	using non_atomic_ref_base = ref_base_internal<__non_atomic_counter>;

	enum class construct_from_make_ref {};
	template <class _Ref_t>
	class ref_ptr final {

		template <class _Ref_ty, typename... _Args>
		friend ref_ptr<_Ref_ty> make_ref(_Args&&... args);

		template <class _Ref_ty>
		friend class ref_ptr;

		template <class _To, class _From>
		friend ref_ptr<_To> static_pointer_cast(ref_ptr<_From> const& r);

		template <class _To, class _From>
		friend ref_ptr<_To> static_pointer_cast(ref_ptr<_From>&& r);

		template <class _To, class _From>
		friend ref_ptr<_To> dynamic_pointer_cast(ref_ptr<_From> const& r);

		template <class _To, class _From>
		friend ref_ptr<_To> dynamic_pointer_cast(ref_ptr<_From>&& r);

		template <class _To, class _From>
		friend ref_ptr<_To> const_pointer_cast(ref_ptr<_From> const& r);

		template <class _To, class _From>
		friend ref_ptr<_To> const_pointer_cast(ref_ptr<_From>&& r);

	private:
		typedef typename std::remove_reference<_Ref_t>::type __ELEMENT_TYPE;
		typedef __ELEMENT_TYPE* POINTER_TYPE;
		typedef ref_ptr<_Ref_t> THIS_TYPE;
		
		//for bool 
		//refer to: https://www.artima.com/cppsource/safebool.html
		typedef void (THIS_TYPE::* bool_type)() const;
		void __no_comparision_operation_support() const {}

		POINTER_TYPE _p;

		/*
		template <typename _Tp_rel
			, class = typename std::enable_if<std::is_convertible<_Tp_rel*, POINTER_TYPE>::value>::type
		>
			inline ref_ptr(_Tp_rel* const& p, construct_from_make_ref)
			:_p(p)
		{
		}
		*/
		explicit inline ref_ptr(POINTER_TYPE const& p, construct_from_make_ref)
			:_p(p)
		{
		}
	public:

		_NETP_CONSTEXPR ref_ptr() _NETP_NOEXCEPT:_p(0) {}
		_NETP_CONSTEXPR ref_ptr(std::nullptr_t) _NETP_NOEXCEPT : _p(0) {}

		~ref_ptr() { if (_p != 0) _p->_ref_drop(); }

		inline ref_ptr(THIS_TYPE const& r) _NETP_NOEXCEPT :
			_p(r._p)
		{
			if (_p != 0) _p->_ref_grab();
		}

		inline ref_ptr(THIS_TYPE&& r) _NETP_NOEXCEPT:
		_p(r._p)
		{
			r._p = 0;
		}

		template <class _From>
		inline ref_ptr(ref_ptr<_From> const& r, POINTER_TYPE const p) _NETP_NOEXCEPT:
		_p(p)
		{//for cast
			if (_p != 0) _p->_ref_grab();
			(void)r;
		}

		template <class _From>
		inline ref_ptr(ref_ptr<_From>&& r, POINTER_TYPE const p) _NETP_NOEXCEPT:
		_p(p)
		{//for cast
			r._p = 0;
			(void)r;
		}

		template <typename _Tp_rel
			, class = typename std::enable_if<std::is_convertible<_Tp_rel*, POINTER_TYPE>::value>::type
		>
			inline ref_ptr(_Tp_rel* const& p)
			:_p(p)
		{
			//static_assert(std::is_convertible<_Tp_rel*, ELEMENT_TYPE*>::value, "convertible check failed");
			if (_p != 0) _p->_ref_grab();
		}
		/*
		template <typename _Tp_rel
			, class = typename std::enable_if<std::is_convertible<_Tp_rel*, POINTER_TYPE>::value>::type
		>
			inline ref_ptr(_Tp_rel* && p)
			:_p(p)
		{
			//static_assert(std::is_convertible<_Tp_rel*, ELEMENT_TYPE*>::value, "convertible check failed");
			//if (_p != 0) _p->_ref_grab();
		}
		*/
		template <typename _Tp_rel
#ifdef _NETP_NO_CXX11_CLASS_TEMPLATE_DEFAULT_TYPE
		>
		ref_ptr(ref_ptr<_Tp_rel> const& r, typename
			std::enable_if<std::is_convertible<_Tp_rel*, ELEMENT_TYPE*>::value>::type ** = 0 )
#else
		, class = typename std::enable_if<std::is_convertible<_Tp_rel*, POINTER_TYPE>::value>::type
		>
		inline ref_ptr( ref_ptr<_Tp_rel> const& r )
#endif
			:_p(r._p)
		{
			if(_p != 0) _p->_ref_grab();
		}

		template <typename _Tp_rel
#ifdef _NETP_NO_CXX11_CLASS_TEMPLATE_DEFAULT_TYPE
		>
		ref_ptr(ref_ptr<_Tp_rel>&& r, typename
			std::enable_if<std::is_convertible<_Tp_rel*, ELEMENT_TYPE*>::value>::type ** = 0 )
#else
			, class = typename std::enable_if<std::is_convertible<_Tp_rel*, POINTER_TYPE>::value>::type
		>
		inline ref_ptr(ref_ptr<_Tp_rel>&& r)
#endif
			:_p(r._p)
		{
			r._p = 0;
		}

		inline ref_ptr& operator= (THIS_TYPE const& r) _NETP_NOEXCEPT
		{
			THIS_TYPE(r).swap(*this);
			return (*this);
		}

		inline ref_ptr& operator= (THIS_TYPE&& r) _NETP_NOEXCEPT
		{
			if (_p != 0) _p->_ref_drop();
			_p = r._p;
			r._p = 0;
			return (*this);
		}

		template <typename _Tp_rel>
		inline ref_ptr& operator= (ref_ptr<_Tp_rel> const& r) _NETP_NOEXCEPT
		{
			THIS_TYPE(r).swap(*this);
			return (*this);
		}

		template <typename _Tp_rel>
		inline ref_ptr& operator= (ref_ptr<_Tp_rel>&& r) _NETP_NOEXCEPT
		{
			if (_p != 0) _p->_ref_drop();
			_p = r._p;
			r._p = 0;
			return (*this);
		}

		template <typename _Tp_rel>
		inline ref_ptr& operator = (_Tp_rel* const& p) _NETP_NOEXCEPT
		{
			THIS_TYPE(p).swap(*this);
			return (*this);
		}

		__NETP_FORCE_INLINE void swap(THIS_TYPE& other) _NETP_NOEXCEPT
		{
			std::swap( _p, other._p );
		}

		inline long ref_count() const _NETP_NOEXCEPT { return (_p==0)?0:_p->_ref_count(); }

		/*
		* @note: for operator-> and operator* 
		*
		* to follow standard c++ impl, we have to set T const& -> T&, and remove the none const version
		*/

		__NETP_FORCE_INLINE _Ref_t* operator -> () const
		{
#ifdef _NETP_DEBUG
			NETP_ASSERT( _p != 0 );
#endif
			return (_p);
		}

		__NETP_FORCE_INLINE _Ref_t& operator * () const
		{
#ifdef _NETP_DEBUG
			NETP_ASSERT( _p != 0 );
#endif
			return (*_p);
		}

		__NETP_FORCE_INLINE operator bool_type() const {
			return _p ? &THIS_TYPE::__no_comparision_operation_support : 0;
		}

		__NETP_FORCE_INLINE POINTER_TYPE get() const {return (_p);}

		__NETP_FORCE_INLINE bool operator == (THIS_TYPE const& r) const { return _p == r._p; }
		__NETP_FORCE_INLINE bool operator != (THIS_TYPE const& r) const { return _p != r._p; }
		__NETP_FORCE_INLINE bool operator > (THIS_TYPE const& r) const { return _p > r._p; }
		__NETP_FORCE_INLINE bool operator < (THIS_TYPE const& r) const { return _p < r._p; }

		__NETP_FORCE_INLINE bool operator == (POINTER_TYPE const& p) const { return _p == p; }
		__NETP_FORCE_INLINE bool operator != (POINTER_TYPE const& p) const { return _p != p; }
		__NETP_FORCE_INLINE bool operator > (POINTER_TYPE const& p) const { return _p > p; }
		__NETP_FORCE_INLINE bool operator < (POINTER_TYPE const& p) const { return _p < p; }
	};

#ifdef _NETP_NO_CXX11_TEMPLATE_VARIADIC_ARGS
#define _ALLOCATE_MAKE_REF( \
	TEMPLATE_LIST, PADDING_LIST, LIST, COMMA, X1, X2, X3, X4) \
template<class _Ty COMMA LIST(_CLASS_TYPE)> \
	inline ref_ptr<_Ty> make_ref(LIST(_TYPE_REFREF_ARG)) \
	{	/* make a shared_ptr */ \
		_Ty *_Rx = \
		new _Ty(LIST(_FORWARD_ARG)); \
		NETP_ALLOC_CHECK(_Rx); \
		return (ref_ptr<_Ty>(_Rx),construct_from_make_ref()); \
	}
	_VARIADIC_EXPAND_0X(_ALLOCATE_MAKE_REF, , , , )
#undef _ALLOCATE_MAKE_REF
#else

	template <class _Ref_t, typename... _Args>
	inline ref_ptr<_Ref_t> make_ref(_Args&&... args)
	{
		_Ref_t* t = new _Ref_t(std::forward<_Args>(args)...);
		return ref_ptr<_Ref_t>(t, construct_from_make_ref());
	}
#endif


	/*
	 * test case for the following three function
		netp::ref_ptr<RepSub> repsub;
		netp::ref_ptr<RepSub> repsub_d;
		netp::ref_ptr<Rep> rp = netp::make_ref<RepSub>();

		repsub = netp::static_pointer_cast<RepSub, Rep>(rp);
		repsub_d = netp::dynamic_pointer_cast<RepSub, Rep>(rp);

		netp::ref_ptr<const Rep> crep = rp;
		netp::ref_ptr<Rep> crep_const_cast = netp::const_pointer_cast<Rep, const Rep>(crep);
		crep_const_cast->i = 10;
	*/

	template <class _To, class _From>
	inline ref_ptr<_To> static_pointer_cast( ref_ptr<_From> const& r )
	{
		auto p = static_cast< typename ref_ptr<_To>::POINTER_TYPE>( r.get() );
		return ref_ptr<_To>(r,p);
	}

	template <class _To, class _From>
	inline ref_ptr<_To> static_pointer_cast(ref_ptr<_From>&& r)
	{
		auto p = static_cast<typename ref_ptr<_To>::POINTER_TYPE>(r.get());
		return ref_ptr<_To>(std::forward<ref_ptr<_From>>(r), p);
	}

	template <class _To, class _From>
	inline ref_ptr<_To> dynamic_pointer_cast(ref_ptr<_From> const& r )
	{
		auto p = dynamic_cast< typename ref_ptr<_To>::POINTER_TYPE>( r.get() );
		if(NETP_LIKELY(p)) return ref_ptr<_To>(r,p);
		return ref_ptr<_To>();
	}

	template <class _To, class _From>
	inline ref_ptr<_To> dynamic_pointer_cast(ref_ptr<_From>&& r)
	{
		auto p = dynamic_cast<typename ref_ptr<_To>::POINTER_TYPE>(r.get());
		if (NETP_LIKELY(p)) return ref_ptr<_To>(std::forward<ref_ptr<_From>>(r), p);
		return ref_ptr<_To>();
	}

	template <class _To, class _From>
	inline ref_ptr<_To> const_pointer_cast(ref_ptr<_From> const& r)
	{
		auto p = const_cast< typename ref_ptr<_To>::POINTER_TYPE>(r.get());
		return ref_ptr<_To>(r,p);
	}

	template <class _To, class _From>
	inline ref_ptr<_To> const_pointer_cast(ref_ptr<_From>&& r)
	{
		auto p = const_cast<typename ref_ptr<_To>::POINTER_TYPE>(r.get());
		return ref_ptr<_To>(std::forward<ref_ptr<_From>>(r), p);
	}
}

//#define NETP_REF_PTR netp::ref_ptr
#define NRP netp::ref_ptr

namespace std {
	//---atomic override
	//std::atomic_thread_fence
}
#endif//_NETP_SMARTPTR_HPP_