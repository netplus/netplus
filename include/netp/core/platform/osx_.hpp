#ifndef _CORE_PLATFORM_NETP_PLATFORM_OSX_HPP_
#define _CORE_PLATFORM_NETP_PLATFORM_OSX_HPP_

//for MacOS
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
#define NETP_USE_PIPE_AS_INTRFD               1

namespace netp {
#define netp_last_errno() NETP_NEGATIVE((int)errno)
#define netp_set_last_errno(e) (errno=e)
#define netp_socket_set_last_errno(e) (errno=e)
#define netp_socket_get_last_errno() NETP_NEGATIVE(netp_last_errno())

    typedef socklen_t socklen_t;
}

#if !defined(SOL_TCP) && defined(IPPROTO_TCP)
#define SOL_TCP IPPROTO_TCP
#endif

#if !defined(TCP_KEEPIDLE) && defined(TCP_KEEPALIVE)
#define TCP_KEEPIDLE TCP_KEEPALIVE
#endif

#endif