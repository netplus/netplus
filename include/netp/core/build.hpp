#ifndef __NETP_CORE_VERSION_HPP
#define __NETP_CORE_VERSION_HPP

#include <netp/core/macros.hpp>

#ifndef __NETP_VERSION_MAJOR
	#define __NETP_VERSION_MAJOR 1
#endif

#ifndef __NETP_VERSION_MINOR
	#define __NETP_VERSION_MINOR 0
#endif

#ifndef __NETP_VERSION_RELEASE
	#define __NETP_VERSION_RELEASE 0
#endif

#ifndef __NETP_BUILD_DATE
	#define __NETP_BUILD_DATE "2012-01-01 00:00:00"
#endif

#define __NETP_VERSION_STRING NETP_VERSION_STRING( NETP_VERSION(__NETP_VERSION_MAJOR,__NETP_VERSION_MINOR, __NETP_VERSION_RELEASE,.) )

#endif