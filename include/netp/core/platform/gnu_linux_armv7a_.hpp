#ifndef _NETP_CORE_PLATFORM_GNU_LINUX_ARMV7A_HPP_
#define _NETP_CORE_PLATFORM_GNU_LINUX_ARMV7A_HPP_

#ifndef UDP_SEGMENT
	//tested on pi4/armv7a/gcc 8.3
	#define UDP_SEGMENT	103	/* Set GSO segmentation size */
#endif
#endif