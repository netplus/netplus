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

#define NETP_CLOSE_SOCKET	::close
#define NETP_DUP						dup
#define NETP_DUP2					dup2

namespace netp {
	#define netp_last_errno() NETP_NEGATIVE((int)errno)
	#define netp_set_last_errno(e) (errno=e)
	#define netp_socket_set_last_errno(e) (errno=e)
	#define netp_socket_get_last_errno() NETP_NEGATIVE(netp_last_errno())

	typedef socklen_t socklen_t;
}

#endif