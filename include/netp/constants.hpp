#ifndef _NETP_CONSTANTS_H_
#define _NETP_CONSTANTS_H_

#include <climits>
#include <netp/core/platform.hpp>

#define NETP_SOCKET_ERROR (-1)
#define NETP_INVALID_SOCKET netp::SOCKET(~0)

namespace netp {

	namespace u8 {
		const u32_t MAX = 0xffU;
	}
	namespace i8 {
		const u32_t MAX = 0x7f;
	}

	namespace u16 {
		const u32_t MAX = 0xffffU;
	}
	namespace i16 {
		const u32_t MAX = 0x7fffU;
	}

	namespace u32 {
		const u32_t MAX = 0xffffffffUL;
	}

	namespace i32 {
		const u32_t MAX = 0x7fffffffUL;
	}

	namespace u64 {
		const u64_t MAX = 0xffffffffffffffffULL;
	}

	namespace i64 {
		const u64_t MAX = 0x7fffffffffffffffULL;
	}

	//WSAEWOULDBLOCK
	const int OK = 0;
#if defined(_NETP_APPLE)
	const int  E_EPERM = -1;               /* Operation not permitted */
	const int  E_ENOENT = -2;               /* No such file or directory */
	const int  E_ESRCH = -3;               /* No such process */
	const int  E_EINTR =-4;               /* Interrupted system call */
	const int  E_EIO = -5;               /* Input/output error */
	const int  E_ENXIO = -6;               /* Device not configured */
	const int  E_E2BIG = -7;               /* Argument list too long */
	const int  E_ENOEXEC = -8;               /* Exec format error */
	const int  E_EBADF = -9;               /* Bad file descriptor */
	const int  E_ECHILD = -10;              /* No child processes */
	const int  E_EDEADLK = -11;              /* Resource deadlock avoided */
	/* 11 was EAGAIN */
	const int  E_ENOMEM = -12;              /* Cannot allocate memory */
	const int  E_EACCES = -13;              /* Permission denied */
	const int  E_EFAULT = -14;          /* Bad address */
#if __DARWIN_C_LEVEL >= __DARWIN_C_FULL
	const int  E_ENOTBLK = -15; ;         /* Block device required */
#endif
	const int  E_EBUSY = -16;          /* Device / Resource busy */
	const int  E_EEXIST          =-17;          /* File exists */
	const int  E_EXDEV = -18;        /* Cross-device link */
	const int  E_ENODEV = -19;         /* Operation not supported by device */
	const int  E_ENOTDIR = -20;         /* Not a directory */
	const int  E_EISDIR = -21;       /* Is a directory */
	const int  E_EINVAL = -22;         /* Invalid argument */
	const int  E_ENFILE = -23;        /* Too many open files in system */
	const int  E_EMFILE = -24;         /* Too many open files */
	const int  E_ENOTTY = -25;           /* Inappropriate ioctl for device */
	const int  E_ETXTBSY = 26;          /* Text file busy */
	const int  E_EFBIG = -27;         /* File too large */
	const int  E_ENOSPC = -28;          /* No space left on device */
	const int  E_ESPIPE = -29;        /* Illegal seek */
	const int  E_EROFS = -30;         /* Read-only file system */
	const int  E_EMLINK = -31;           /* Too many links */
	const int  E_EPIPE = -32;           /* Broken pipe */

/* math software */
const int  E_EDOM = -33;         /* Numerical argument out of domain */
const int  E_ERANGE = -34;         /* Result too large */

	/* non-blocking and interrupt i/o */
	const int  E_EAGAIN = -35;         /* Resource temporarily unavailable */
	const int  E_EWOULDBLOCK = E_EAGAIN;     /* Operation would block */
	const int  E_EINPROGRESS = -36;          /* Operation now in progress */
	const int  E_EALREADY = -37;           /* Operation already in progress */

	/* ipc/network software -- argument errors */
	const int  E_ENOTSOCK = -38;         /* Socket operation on non-socket */
	const int  E_EDESTADDRREQ = -39;         /* Destination address required */
	const int  E_EMSGSIZE = -40;        /* Message too long */
	const int  E_EPROTOTYPE = -41;            /* Protocol wrong type for socket */
	const int  E_ENOPROTOOPT = -42;            /* Protocol not available */
	const int  E_EPROTONOSUPPORT = -43;          /* Protocol not supported */
	#if __DARWIN_C_LEVEL >= __DARWIN_C_FULL
	const int  E_ESOCKTNOSUPPORT = -44;           /* Socket type not supported */
	#endif
	const int  E_ENOTSUP = -45;          /* Operation not supported */
	#if !__DARWIN_UNIX03 && !defined(KERNEL)
	/*
	 * This is the same for binary and source copmpatability, unless compiling
	 * the kernel itself, or compiling __DARWIN_UNIX03; if compiling for the
	 * kernel, the correct value will be returned.  If compiling non-POSIX
	 * source, the kernel return value will be converted by a stub in libc, and
	 * if compiling source with __DARWIN_UNIX03, the conversion in libc is not
	 * done, and the caller gets the expected (discrete) value.
	 */
	const int  E_EOPNOTSUPP = E_ENOTSUP;        /* Operation not supported on socket */
	#endif /* !__DARWIN_UNIX03 && !KERNEL */

	#if __DARWIN_C_LEVEL >= __DARWIN_C_FULL
	const int  E_EPFNOSUPPORT = -46;           /* Protocol family not supported */
	#endif
	const int  E_EAFNOSUPPORT = -47;         /* Address family not supported by protocol family */
	const int  E_EADDRINUSE = -48;         /* Address already in use */
	const int  E_EADDRNOTAVAIL = -49;            /* Can't assign requested address */

	 /* ipc/network software -- operational errors */
	const int  E_ENETDOWN = -50;          /* Network is down */
	const int  E_ENETUNREACH = -51;          /* Network is unreachable */
	const int  E_ENETRESET = -52;/* Network dropped connection on reset */
	const int  E_ECONNABORTED = -53;            /* Software caused connection abort */
	const int  E_ECONNRESET = -54;         /* Connection reset by peer */
	const int  E_ENOBUFS = -55;        /* No buffer space available */
	const int  E_EISCONN = -56;         /* Socket is already connected */
	const int  E_ENOTCONN = -57;        /* Socket is not connected */
	#if __DARWIN_C_LEVEL >= __DARWIN_C_FULL
	const int  E_ESHUTDOWN = -58;        /* Can't send after socket shutdown */
	const int  E_ETOOMANYREFS = -59;           /* Too many references: can't splice */
	#endif
	const int  E_ETIMEDOUT = -60;       /* Operation timed out */
	const int  E_ECONNREFUSED = -61;         /* Connection refused */

	const int  E_ELOOP = -62;       /* Too many levels of symbolic links */
	const int  E_ENAMETOOLONG = -63;          /* File name too long */

	/* should be rearranged */
	#if __DARWIN_C_LEVEL >= __DARWIN_C_FULL
	const int  E_EHOSTDOWN = -64;          /* Host is down */
	#endif
	const int  E_EHOSTUNREACH = -65;          /* No route to host */
	const int  E_ENOTEMPTY = -66;     /* Directory not empty */

	/* quotas & mush */
	#if __DARWIN_C_LEVEL >= __DARWIN_C_FULL
	const int  E_EPROCLIM = -67;             /* Too many processes */
	const int  E_EUSERS = -68;           /* Too many users */
	#endif
	const int  E_EDQUOT = -69;             /* Disc quota exceeded */

	/* Network File System */
	const int  E_ESTALE = -70;         /* Stale NFS file handle */
	#if __DARWIN_C_LEVEL >= __DARWIN_C_FULL
	const int  E_EREMOTE = -71;             /* Too many levels of remote in path */
	const int  E_EBADRPC = -72;  /* RPC struct is bad */
	const int  E_ERPCMISMATCH = -73;            /* RPC version wrong */
	const int  E_EPROGUNAVAIL = -74;         /* RPC prog. not avail */
	const int  E_EPROGMISMATCH = -75;              /* Program version wrong */
	const int  E_EPROCUNAVAIL = -76;          /* Bad procedure for program */
	#endif

	const int  E_ENOLCK = -77;             /* No locks available */
	const int  E_ENOSYS = -78;             /* Function not implemented */

	#if __DARWIN_C_LEVEL >= __DARWIN_C_FULL
	const int  E_EFTYPE = -79;          /* Inappropriate file type or format */
	const int  E_EAUTH = -80;          /* Authentication error */
	const int  E_ENEEDAUTH = -81;            /* Need authenticator */

	/* Intelligent device errors */
	const int  E_EPWROFF = -82;    /* Device power is off */
	const int  E_EDEVERR = -83;    /* Device error, e.g. paper out */
	#endif

	const int  E_EOVERFLOW = -84;          /* Value too large to be stored in data type */

	/* Program loading errors */
	#if __DARWIN_C_LEVEL >= __DARWIN_C_FULL
	const int  E_EBADEXEC = -85;     /* Bad executable */
	const int  E_EBADARCH = -86;     /* Bad CPU type in executable */
	const int  E_ESHLIBVERS = -87;    /* Shared library version mismatch */
	const int  E_EBADMACHO = -88;    /* Malformed Macho file */
	#endif

	const int  E_ECANCELED = -89;          /* Operation canceled */

	const int  E_EIDRM = -90;         /* Identifier removed */
	const int  E_ENOMSG = -91;            /* No message of desired type */
	const int  E_EILSEQ = -92;        /* Illegal byte sequence */
	#if __DARWIN_C_LEVEL >= __DARWIN_C_FULL
	const int  E_ENOATTR = -93;         /* Attribute not found */
	#endif

	const int  E_EBADMSG = -94;        /* Bad message */
	const int  E_EMULTIHOP = -95;           /* Reserved */
	const int  E_ENODATA = -96;        /* No message available on STREAM */
	const int  E_ENOLINK = -97;         /* Reserved */
	const int  E_ENOSR = -98;         /* No STREAM resources */
	const int  E_ENOSTR = -99;           /* Not a STREAM */
	const int  E_EPROTO = -100;           /* Protocol error */
	const int  E_ETIME = -101;          /* STREAM ioctl timeout */

	#if __DARWIN_UNIX03 || defined(KERNEL)
	/* This value is only discrete when compiling __DARWIN_UNIX03, or KERNEL */
	const int  E_EOPNOTSUPP = -102;         /* Operation not supported on socket */
	#endif /* __DARWIN_UNIX03 || KERNEL */

	const int  E_ENOPOLICY = -103;       /* No such policy registered */

	#if __DARWIN_C_LEVEL >= 200809L
	const int  E_ENOTRECOVERABLE = -104;         /* State not recoverable */
	const int  E_EOWNERDEAD = -105;            /* Previous owner died */
	#endif

	#if __DARWIN_C_LEVEL >= __DARWIN_C_FULL
	const int  E_EQFULL = -106;             /* Interface output queue is full */
	const int  E_ELAST = -106;             /* Must be equal largest errno */
	#endif
#elif defined(_NETP_GNU_LINUX) || defined(_NETP_ANDROID)
	const int E_EPERM								= -1; //operation not permitted
	const int E_ENOENT							= -2; //no such file or directory
	const int E_ESRCH								= -3; //no such process
	const int E_EINTR								= -4; //interrupted system call
	const int E_EIO										= -5; //I/O error
	const int E_ENXIO								= -6; //no such device or address
	const int E_E2BIG									= -7; //Arg list too long
	const int E_ENOEXEC							= -8; //Exec format error
	const int E_EBADF								= -9; //Bad file number
	const int E_ECHILD								= -10; //No child processes
	const int E_EAGAIN								= -11; //Try again
	const int E_ENOMEM							= -12; //Out of memory
	const int E_EACCESS							= -13; //Permission denied
	const int E_EFAULT								= -14; //Bad address
	const int E_ENOTBLK							= -15; //Block device required
	const int E_EBUSY								= -16; //Device or resource busy
	const int E_EEXIST								= -17; //File exists
	const int E_EEXDEV								= -18; //Cross-device link
	const int E_ENODEV							= -19; //No such device
	const int E_ENOTDIR							= -20; //Not a directory
	const int E_EISDIR								= -21; //Is a directory
	const int E_EINVAL								= -22; //Invalid argument
	const int E_ENFILE								= -23; //File table overflow
	const int E_EMFILE								= -24; //Too many open files
	const int E_ENOTTY								= -25; //Not a tty device
	const int E_ETXTBSY							= -26; //Text file busy
	const int E_EFBIG									= -27; //File too large
	const int E_ENOSPC							= -28; //No space left on device
	const int E_ESPIPE								= -29; //Illegal seek
	const int E_EROFS								= -30; //read-only file system
	const int E_EMLINK								= -31; //Too many links
	const int E_EPIPE									= -32; //broken pipe
	const int E_EDOM								= -33; //Math argument out of domain
	const int E_ERANGE								= -34; //Math result not representable
	const int E_EDEADLK							= -35; //dead lock wolld occur
	const int E_ENAMETOOLONG				= -36; //file too long
	const int E_ENOLCK								= -37; //No record locks available
	const int E_ENOSYS								= -38; //Function not implemented
	const int E_ENOTEMPTY						= -39; //dierctory not empty
	const int E_ELOOP								= -40; //too many symbolic links encountered
	const int E_EWOULDBLOCK				= -41; //same as EAGAIN
	const int E_ENOMSG							= -42; //No message of desired type
	const int E_EIDRM								= -43; //identifier removed
	const int E_ECHRNG							= -44; //channel number out of range
	const int E_EL2NSYNC							= -45; //level 2 not synchronized
	const int E_EL3HLT								= -46; //level 3 halted
	const int E_EL3RST								= -47; //level3 reset
	const int E_ELNRNG							= -48; //Link number out of range
	const int E_EUNATCH							= -49; //protocol driver not attached
	const int E_ENOCSI								= -50; //No CSI structure available
	const int E_EL2HLT								= -51; //Level 2 halted
	const int E_EBADE								= -52; //Invalid exchange
	const int E_EBADR								= -53; //Invalid request descriptor
	const int E_EXFULL								= -54; //Exchange full
	const int E_ENOANO							= -55; //No anode
	const int E_EBADRQC							= -56; //Invalid request code
	const int E_EBADSLT							= -57; //Invalid slot
	const int E_EDEADLOCK						= -58; //Same as EDEADLK
	const int E_EBFONT								= -59; //Bad font file format
	const int E_ENOSTR								= -60; //No data available
	const int E_ENODATA							= -61; //No data available
	const int E_ETIME								= -62; //Timer expired
	const int E_ENOSR								= -63; //Out of streams resources
	const int E_ERROR_NETNAME_DELETED			= -64; //WINDOWS IOCP: The specified network name is no longer available
	const int E_ENONET							= -64; //machine is not on network
	const int E_ENOPKG							= -65; //Package not installed
	const int E_EREMOTE							= -66; //Object is remote
	const int E_ENOLINK							= -67; //Link has been severed
	const int E_EADV									= -68; //Advertise error
	const int E_ESRMNT							= -69; //Srmount error
	const int E_ECOMM								= -70; //Communication error on send
	const int E_EPROTO								= -71; //protocol error
	const int E_EMULTIHOP						= -72; //Multihop attempted
	const int E_EDOTDOT							= -73; //RFS specific error
	const int E_EBADMSG							= -74; //Not a data message
	const int E_EOVERFLOW						= -75; //Value too large for defined data type
	const int E_ENOTUNIQ						= -76; //Name not unique on network
	const int E_EBADFD								= -77; //File descriptor in bad state
	const int E_REMCHG							= -78; //Remote address changed
	const int E_ELIBACC								= -79; //Cannot access a needed shared library
	const int E_ELIBBAD							= -80; //Accessing a corrupted shared library
	const int E_ELIBSCN							= -81; //A .lib section in an .out is corrupted
	const int E_ELIMAX								= -82; //Linking in too many shared libraries
	const int E_ELIEXEC								= -83; //Cannot exec a shared library directly
	const int E_ELIILSEQ							= -84; //Illegal byte sequence
	const int E_ERESTART							= -85; //Interrupted system call should be restarted
	const int E_ESTRPIPE							= -86; //Streams pipe error
	const int E_EUSERS								= -87; //Too many users
	const int E_ENOTSOCK						= -88; //socket operation on non-socket
	const int E_EDESTADDRREQ				= -89; //Destination address required

	const int E_EMSGSIZE							= -90; //Message too long
	const int E_EPROTOTYPE						= -91; //protocol wrong type for socket
	const int E_ENOPROTOOPT				= -92; //protocol not available
	const int E_EPROTONOSUPPORT		= -93; //protocol not supported
	const int E_ESOCKTNOSUPPORT		= -94; //socket type not supported
	const int E_EOPNOTSUPP					= -95; //Operation not supported on transport
	const int E_EPFNOSUPPORT				= -96; //protocol family not supported
	const int E_EAFNOSUPPORT				= -97; //address family not supported by protocol
	const int E_EADDRINUSE					= -98; //address already in use
	const int E_EADDRNOTAVAIL				= -99; //Cannot assign requested address
	const int E_ENETDOWN						= -100; //Network is down
	const int E_ENETUNREACH					= -101; //Network is unreachable
	const int E_ENETRESET						= -102; //Network dropped
	const int E_ECONNABORTED				= -103; //Software caused connection
	const int E_ECONNRESET					= -104; //Connection reset by
	const int E_ENOBUFS							= -105; //No buffer space available
	const int E_EISCONN							= -106; //Transport endpoint
	const int E_ENOTCONN						= -107; //Transport endpoint
	const int E_ESHUTDOWN					= -108; //Cannot send after transport
	const int E_ETOOMANYREFS				= -109; //Too many references
	const int E_ETIMEOUT							= -110; //Connection timed
	const int E_ECONNREFUSED				= -111; //Connection refused
	const int E_EHOSTDOWN					= -112; //Host is down
	const int E_EHOSTUNREACH				= -113; //No route to host
	const int E_EALREADY							= -114; //Operation already
	const int E_EINPROGRESS					= -115; //Operation now in
	const int E_ESTALE								= -116; //Stale NFS file handle
	const int E_EUCLEAN							= -117; //Structure needs cleaning
	const int E_ENOTNAM						= -118; //Not a XENIX-named
	const int E_ENAVAIL							= -119; //No XENIX semaphores
	const int E_EISNAM								= -120; //Is a named type file
	const int E_EREMOTEIO						= -121; //Remote I/O error
	const int E_EDQUOT							= -122; //Quota exceeded
	const int E_ENOMEDIUM					= -123; //No medium found
	const int E_EMEDIUMTYPE					= -124; //Wrong medium type
	const int E_ECANCELED						= -125;
	const int E_ENOKEY								= -126;
	const int E_EKEYEXPIRED					= -127;
	const int E_EKEYREVOKED					= -128;
	const int E_EKEYREJECTED					= -129;
	const int E_EOWNERDEAD					= -130;
	const int E_ENOTRECOVERABLE			= -131;
	const int E_ERFKILL								= -132;
	const int E_EHWPOISON						= -133;


#elif defined(_NETP_WIN)
	const int E_ERROR_ACCESS_DENIED = -5;
	const int E_WSA_INVALID_HANDLE = -6;
	const int E_ERROR_HANDLE_EOF = -38;
	const int E_WSA_INVALID_PARAMETER = -87;
	const int E_ERROR_MORE_DATA            = -234;
	const int E_WAIT_TIMEOUT = -258;
	const int E_ERROR_ABANDONED_WAIT_0 = -735;

	const int E_WSA_OPERATION_ABORTED = -995;
	const int E_WSA_IO_INCOMPLETE = -996;
	const int E_WSA_IO_PENDING = -997;
	const int E_ERROR_PRIVILEGE_NOT_HELD = -1314;

	const int E_ERROR_CONNECTION_ABORTED = -1236;

// 1---19999; reserved for system call error code (windows)
	const int E_WSAEINTR							= -10004; // A blocking operation was interrupted by a call to WSACancelBlockingCall.
	const int E_WSAEBADF						= -10009; // The file handle supplied is not valid.
	const int E_WSAEACCES						= -10013; //An attempt was made to access a socket in a way forbidden by its access permissions.
	const int E_WSAEFAULT						= -10014; // The system detected an invalid pointer address in attempting to use a pointer argument in a call.
	const int E_WSAEINVAL						= -10022; // An invalid argument was supplied
	const int E_WSAEMFILE						= -10024; // Too many open sockets.
	const int E_WSAEWOULDBLOCK		= -10035; // A non-blocking socket operation could not be completed immediately.
	const int E_WSAEINPROGRESS			= -10036; // A blocking operation is currently executing.
	const int E_WSAEALREADY						= -10037; // An operation was attempted on a non-blocking socket that already had an operation in progress.
	const int E_WSAENOTSOCK					= -10038; // An operation was attempted on something that is not a socket.
	const int E_WSAEMSGSIZE						= -10040; // A message sent on a datagram socket was larger than the internal message buffer or some other network limit;  or the buffer used to receive a datagram into was smaller than the datagram itself.
	const int E_WSAEPROTOTYPE					= -10041; //A protocol was specified in the socket function call that does not support the semantics of the socket type requested.
	const int E_WSAENOPROTOOPT					= -10042; //An unknown; invalid; or unsupported option or level was specified in a getsockopt or setsockopt call
	const int E_WSAEPROTONOSUPPORT		= -10043; //The requested protocol has not been configured into the system;  or no implementation for it exists.
	const int E_WSAESOCKTNOSUPPORT		= -10044; //The support for the specified socket type does not exist in this address family
	const int E_WSAEOPNOTSUPP					= -10045;  //The attempted operation is not supported for the type of object referenced.
	const int E_WSAEPFNOSUPPORT				= -10046; //The protocol family has not been configured into the system or no implementation for it exists
	const int E_WSAEAFNOSUPPORT				= -10047; //An address incompatible with the requested protocol was used
	const int E_WSAEADDRINUSE						= -10048;  // Only one usage of each socket address (protocol/network address/port) is normally permitted.
	const int E_WSAEADDRNOTAVAIL				= -10049; // The requested address is not valid in its context.
	const int E_WSAENETDOWN						= -10050; //A socket operation encountered a dead network.
	const int E_WSAENETUNREACH					= -10051;  // A socket operation was attempted to an unreachable network.
	const int E_WSAENETRESET						= -10052; // The connection has been broken due to keep-alive activity detecting a failure while the operation was in progress.
	const int E_WSAECONNABORTED				= -10053; // An established connection was aborted by the software in your host machine.
	const int E_WSAECONNRESET					= -10054; // An existing connection was forcibly closed by the remote host.
	const int E_WSAENOBUFS						= -10055; // An operation on a socket could not be performed because the system lacked sufficient buffer space or because a queue was full.
	const int E_WSAEISCONN						= -10056;  // A request to send or receive data was disallowed because the socket is not connected and (when sending on a datagram socket using a sendto call) no address was supplied.
	const int E_WSAENOTCONN					= -10057; // A request to send or receive data was disallowed because the socket is not connected and (when sending on a datagram socket using a sendto call) no address was supplied.
	const int E_WSAESHUTDOWN				= -10058;  //A request to send or receive data was disallowed because the socket had already been shut down in that direction with a previous shutdown call
	const int E_WSAETOOMANYREFS			= -10059;  //Too many references to some kernel object.
	const int E_WSAETIMEDOUT					= -10060;  //A connection attempt failed because the connected party did not properly respond after a period of time;  or established connection failed because connected host has failed to respond.
	const int E_WSAECONNREFUSED			= -10061;  //No connection could be made because the target machine actively refused it.
	const int E_WSAELOOP							= -10062;  //Cannot translate name
	const int E_WSAEHOSTDOWN				= -10064; // A socket operation failed because the destination host was down
	const int E_WSAEHOSTUNREACH			= -10065; // A socket operation was attempted to an unreachable host.
	const int E_WSAEPROCLIM						= -10067; // A Windows Sockets implementation may have a limit on the number of applications that may use it simultaneously.
	const int E_WSAEUSERS							= -10068;
	const int E_WSAEDQUOT						= -10069;

	const int E_WSASYSNOTREADY				= -10091;
	const int E_WSANOTINITIALISED			= -10093;

	const int E_WSAEPROVIDERFAILEDINIT		= -10106;
	const int E_WSASYSCALLFAILURE				= -10107;
	const int E_WSASERVICE_NOT_FOUND		= -10108;
	const int E_WSATYPE_NOT_FOUND			= -10109;
	const int E_WSA_E_NO_MORE					= -10110;
	const int E_WSA_E_CANCELLED					= -10111;
	const int E_WSAEREFUSED							= -10112;
	const int E_WSAHOST_NOT_FOUND			= -11001;
	const int E_WSATRY_AGAIN						= -11002;
	const int E_WSANO_RECOVERY					= -11003;
	const int E_WSANO_DATA							= -11004;
	const int E_WSA_QOS_RECEIVERS				= -11005;
	const int E_WSA_QOS_SENDERS					= -11006;

	const int E_ECONNABORTED = E_WSAECONNABORTED;
	const int E_EINTR = E_WSAEINTR;
	const int E_EWOULDBLOCK = E_WSAEWOULDBLOCK;
	const int E_EAGAIN = E_EWOULDBLOCK;
	const int E_EINPROGRESS = E_WSAEWOULDBLOCK; //pls refer tohttps://docs.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-connect

	const int E_EMFILE = E_WSAEMFILE;


	const int E_EBADF = E_WSAEBADF;
	const int E_EALREADY = E_WSAEALREADY;
	const int E_ECONNRESET = E_WSAECONNRESET;
	const int E_ETIMEOUT = E_WSAETIMEDOUT;
	const int E_EADDRINUSE = E_WSAEADDRINUSE;
	const int E_EINVAL = E_WSAEINVAL;
	const int E_ENOTCONN = E_WSAENOTCONN;
#endif
	//internal error
	const int E_NETP_APP_EXIT								= -20000;
	const int E_NO_NET_DEV								= -20001;
	const int E_INVALID_OPERATION					= -20002;
	const int E_INVALID_STATE								= -20003;
	const int E_MEMORY_MALLOC_FAILED			= -20004;
	const int E_MEMORY_ACCESS_ERROR			= -20005;
	const int E_MEMORY_ALLOC_FAILED				= -20006;
	const int E_MEMORY_NEW_FAILED				= -20007;
	const int E_TRY_AGAIN									= -20008;
	const int E_INVAL											= -20009;
	const int E_ASSERT_FAILED								= -20010;
	const int E_NETP_THROW								= -20011;
	const int E_UNKNOWN									= -20012;
	const int E_THREAD_INTERRUPT						= -20013;
	const int E_TODO											= -20014;
	const int E_BAD_CAST										= -20015;
	const int E_OP_INPROCESS								= -20016;
	const int E_OP_ABORT									= -20017;
	const int E_OP_TIMEOUT								= -20018;
	const int E_OP_ALREADY = -20019;

	const int E_EVENT_BROKER_BINDED_ALREADY = -21001;
	const int E_EVENT_BROKER_NO_LISTENER			= -21002;

	const int E_XXTEA_INVALID_DATA			= -22001;
	const int E_XXTEA_DECRYPT_FAILED		= -22002;
	const int E_XXTEA_ENCRYPT_FAILED		= -22003;

	const int E_DH_HANDSHAKE_MESSAGE_CHECK_FAILED = -22011;
	const int E_DH_HANDSHAKE_FAILED		= -22012;

	const int E_IO_EVENT_LOOP_NOTIFY_TERMINATING = -25001;
	const int E_IO_EVENT_LOOP_TERMINATED = -25002;
	const int E_IO_EVENT_LOOP_BYE_DO_NOTHING = -25003;
	const int E_IO_BEGIN_FAILED = -25004;

	//30000 - 30999 //system level socket error
	const int E_SOCKET_EPOLLHUP			= -30001;
//	const int E_SOCKET_WRITE_BLOCK		= -30002;
//	const int E_SOCKET_READ_BLOCK		= -30003;
	const int E_SOCKET_GRACE_CLOSE	= -30004;

	//31000 - 31999 //user custom socket error
	const int E_SOCKET_INVALID_FAMILY		= -31001;
	const int E_SOCKET_INVALID_TYPE				= -31002;
	const int E_SOCKET_INVALID_ADDRESS		= -31003;
	const int E_SOCKET_INVALID_PROTOCOL	= -31004;
	const int E_SOCKET_INVALID_STATE			= -31005;
	const int E_SOCKET_SELF_CONNCTED		= -31006;
	const int E_SOCKET_READ_CLOSED_ALREADY = -31007;
	const int E_SOCKET_WRITE_CLOSED_ALREADY = -31008;
	const int E_SOCKET_NO_AVAILABLE_ADDR = -31009;
	const int E_SOCKET_OP_ALREADY		 = -31010;

	const int E_CHANNEL_BDLIMIT							= -34001;
	const int E_CHANNEL_READ_BLOCK					= -34002;
	const int E_CHANNEL_WRITE_BLOCK					= -34003;
	const int E_CHANNEL_READ_CLOSED					= -34004;
	const int E_CHANNEL_WRITE_CLOSED				= -34005;
	const int E_CHANNEL_INVALID_STATE				= -34006;
	const int E_CHANNEL_NOT_EXISTS					= -34007;
	const int E_CHANNEL_EXISTS								= -34008;
	const int E_CHANNEL_CLOSED							= -34009;
	const int E_CHANNEL_CLOSING							= -34010;
	const int E_CHANNEL_WRITE_SHUTDOWNING	= -34011;
	const int E_CHANNEL_WRITING							= -34012;
	const int E_CHANNEL_OUTGO_LIST_EMPTY		= -34013; //FOR IOCP

	const int E_CHANNEL_READ_WRITE_ERROR		= -34014;
	const int E_CHANNEL_ABORT								= -34015;
	const int E_CHANNEL_CONTEXT_REMOVED		= -34016;

	const int E_CHANNEL_OVERLAPPED_OP_TRY = -34017;
	const int E_CHANNEL_MISSING_MAKER = -34018;//custom socket channel must have its own maker
	const int E_FORWARDER_DOMAIN_LEN_EXCEED	= -35001;
	const int E_FORWARDER_INVALID_IPV4					= -35002;
	const int E_FORWARDER_DIAL_DST_FAILED			= -35003;

	const int E_DNS_LOOKUP_RETURN_NO_IP			= -36001;
	const int E_DNS_TEMPORARY_ERROR				= -36002;
	const int E_DNS_PROTOCOL_ERROR					= -36003;
	const int E_DNS_DOMAIN_NAME_NOT_EXISTS	= -36004;
	const int E_DNS_DOMAIN_NO_DATA					= -36005;
	const int E_DNS_NOMEM									= -36006;
	const int E_DNS_BADQUERY								= -36007;
	const int E_DNS_SERVER_SHUTDOWN				= -36008;

	const int E_MUX_STREAM_TRANSPORT_CLOSED = -37001;
	const int E_MUX_STREAM_RST = -37002;

	const int E_RPC_NO_WRITE_CHANNEL		= -40001;
	const int E_RPC_CALL_UNKNOWN_API		= -40002;
	const int E_RPC_CALL_INVALID_PARAM		= -40003;
	const int E_RPC_CALL_ERROR_UNKNOWN		= -40004;
	const int E_RPC_CALL_CANCEL						= -40005;
	const int E_RPC_CALL_TIMEOUT						= -40006;
	const int E_RPC_WRITE_TIMEOUT					= -40007;
	const int E_RPC_CONNECT_ABORT					= -40008;
	const int E_RPC_MESSAGE_DECODE_FAILED = -40009;

	const int E_HTTP_CLIENT_REQ_IN_OP					= -41001;
	const int E_HTTP_MESSAGE_INVALID_TYPE		= -41002;
	const int E_HTTP_REQUEST_NOT_DONE				= -41003;
	const int E_HTTP_INVALID_HOST						= -41004;
	const int E_HTTP_INVALID_OP							= -41005;
	const int E_HTTP_REQ_TIMEOUT						= -41006;
	const int E_HTTP_CLIENT_DIAL_CANCEL				= -41007;
	const int E_HTTP_CLIENT_CLOSING					= -41008;
	const int E_HTTP_EMPTY_FILED_NAME				= -41009;

} //endif netp ns

#endif //END OF NETP_ERROR_HEADER
