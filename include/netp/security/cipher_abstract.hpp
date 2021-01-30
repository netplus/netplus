#ifndef _NETP_SECURITY_CIPHER_ABSTRACT_HPP
#define _NETP_SECURITY_CIPHER_ABSTRACT_HPP

#include <netp/core.hpp>
#include <netp/packet.hpp>

namespace netp { namespace security {

	class cipher_abstract
	{
	public:
		cipher_abstract() {}
		virtual ~cipher_abstract() {}

		virtual void set_key(byte_t const* const k, u32_t const& klen) = 0;
		virtual int encrypt(NRP<packet> const& in, NRP<packet>& out) const = 0;
		virtual int decrypt(NRP<packet> const& in, NRP<packet>& out) const = 0;
	};
}}
#endif