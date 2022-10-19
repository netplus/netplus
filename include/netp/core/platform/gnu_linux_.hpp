#pragma once
#ifndef _CORE_PLATFORM_NETP_PLATFORM_GNU_LINUX_HPP_
#define _CORE_PLATFORM_NETP_PLATFORM_GNU_LINUX_HPP_

//@note: DO NOT INCLUDE linux/*.h from user space app directlyl, IIF YOU KNOW WHAT YOU ARE DOING
// if there is a missing MACRO in netinet/x.h FILE, but it do exists in linux/x.h for the target host kernel, we could define that MACRO by mannual
//for gnu linux
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <stdio.h>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <sys/socket.h>
#include <sys/un.h> //sockaddr_un
#include <sys/uio.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <linux/version.h>
//SO_INCOMING_CPU (gettable since Linux 3.19, settable since Linux 4.4)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,4,0)
	#define __NETP_ENABLE_SO_INCOMING_CPU
#endif

#define NETP_CLOSE_SOCKET	::close
#define NETP_DUP						dup
#define NETP_DUP2					dup2
#define NETP_USE_PIPE_AS_INTRFD				1

#define netp_last_errno() NETP_NEGATIVE((int)errno)
#define netp_set_last_errno(e) (errno=(NETP_ABS(e)))
#define netp_socket_set_last_errno(e) (errno=NETP_ABS(e))
#define netp_socket_get_last_errno() (netp_last_errno())

namespace netp {
	typedef socklen_t socklen_t;
}

#endif