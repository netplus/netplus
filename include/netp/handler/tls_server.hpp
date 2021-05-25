#ifndef _NETP_HANDLER_TLS_SERVER_HPP
#define _NETP_HANDLER_TLS_SERVER_HPP

namespace netp { namespace handler {

	class tls_server :
		public channel_handler_abstract,
		public Botan::TLS::Callbacks
	{
	};

}}

#endif