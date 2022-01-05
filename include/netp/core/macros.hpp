#ifndef _NETP_CORE_MACROS_H_
#define _NETP_CORE_MACROS_H_

#include <cassert>
#include <netp/core/compiler.hpp>
#include <netp/core/platform.hpp>

#ifndef IPTOS_TOS_MASK
	#define IPTOS_TOS_MASK		0x1E
#endif

#ifndef IPTOS_TOS
	#define IPTOS_TOS(tos)		((tos)&IPTOS_TOS_MASK)
#endif

#ifndef IPTOS_LOWDELAY
	#define	IPTOS_LOWDELAY	0x10
#endif

#ifndef IPTOS_THROUGHPUT
	#define	IPTOS_THROUGHPUT	0x08
#endif

#ifndef IPTOS_RELIABILITY
	#define	IPTOS_RELIABILITY	0x04
#endif

#ifndef IPTOS_MINCOST
	#define	IPTOS_MINCOST		0x02
#endif


namespace netp {
	extern void assert_failed(const char* error, const char* file, int line, const char* function, ... );
}

#define NETP_USE_NETP_ASSERT

#ifndef NULL
	#define NULL 0
#endif

#if defined(_DEBUG) || defined(DEBUG)
	#ifndef _NETP_DEBUG
		#define _NETP_DEBUG
	#endif
#endif

#define NETP_MIN2(A,B)		((A)>(B)?(B):(A))
#define NETP_MIN3(A,B,C)		NETP_MIN2((NETP_MIN2((A),(B))),(C))
#define NETP_MAX2(A,B)		((A)>(B)?(A):(B))
#define NETP_MAX3(A,B,C)		NETP_MAX2((NETP_MAX2((A),(B))),(C))

#define NETP_MIN(A,B)		NETP_MIN2((A),(B))
#define NETP_MAX(A,B)		NETP_MAX2((A),(B))

#define NETP_ABS2(A,B)		(((A)>(B))?((A)-(B)):((B)-(A)))
#define NETP_ABS(A)			(NETP_ABS2(A,0))
#define NETP_NEGATIVE(A)	((A<0)?(A):(-(A)))

#define NETP_FLOAT_EQUAL3(A,B,PRECISION) (NETP_ABS(((A)-(B)))<=(PRECISION))
#define NETP_FLOAT_EQUAL(A,B) NETP_FLOAT_EQUAL3(A,B,(0.000001f))

#define NETP_STRINGIFY( x ) #x
#define NETP_QUOTE( x ) NETP_STRINGIFY( x )

#ifndef NETP_USE_NETP_ASSERT
	#define NETP_ASSERT(x, ...) assert(x)
#else
	#define NETP_ASSERT(x, ...) ( ((x) ? (void)0 : netp::assert_failed (#x, __FILE__, __LINE__ , __FUNCTION__, "" __VA_ARGS__)))
#endif

#define NETP_DELETE(PTR) {do { if( PTR != 0 ) {delete PTR; PTR=nullptr;} } while(false);}

#ifdef _NETP_NO_CXX11_DELETED_FUNC
#define NETP_DECLARE_NONCOPYABLE(__classname__) \
		private: \
		__classname__(__classname__ const&) ; \
		__classname__& operator=(__classname__ const&) ; \
		private:
#else
#define NETP_DECLARE_NONCOPYABLE(__classname__) \
	protected: \
	__classname__(__classname__ const&) = delete; \
	__classname__& operator=(__classname__ const&) = delete; \
	private:
#endif

#define NETP_RETURN_V_IF_MATCH(v,condition) \
	do { \
		if( NETP_LIKELY(condition) ) { return (v);} \
	} while(0)

#define NETP_RETURN_V_IF_NOT_MATCH(v,condition) NETP_RETURN_V_IF_MATCH(v, (!(condition)) )

#define NETP_RETURN_IF_MATCH(condition) \
	do { \
		if( NETP_LIKELY(condition) ) { return ;} \
	} while(0)

#define NETP_RETURN_IF_NOT_MATCH(v,condition) NETP_RETURN_IF_MATCH(v, (!(condition)) )


#define NETP_ARR_ELEMENTS(_array_) (sizeof(_array_) / sizeof(_array_[0]))

#endif