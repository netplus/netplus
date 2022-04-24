#pragma once
#ifndef _CORE_PLATFORM_NETP_PLATFORM_GNU_LINUX_HPP_
#define _CORE_PLATFORM_NETP_PLATFORM_GNU_LINUX_HPP_

//for gnu linux
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <stdio.h>

#include <netinet/tcp.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/un.h> //sockaddr_un
#include <sys/uio.h>
#include <sys/ioctl.h>

#include <linux/version.h>

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