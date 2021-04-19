#ifndef _NETP_CORE_COMPILER_GCC_X86_HPP
#define _NETP_CORE_COMPILER_GCC_X86_HPP

//set to maximum possible data type width to sure all these kinds of access is aligned
#define NETP_DEFAULT_ALIGN (8)

//for x86_64, sse2 is on by default for both x32& x64 gcc compiler
//for x86_32, sse2 is off by default for a x32 gcc compiler
//#define NETP_ENABLE_SSE2

#endif