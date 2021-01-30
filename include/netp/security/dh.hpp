#ifndef _NETP_SECURITY_DH_HPP_
#define _NETP_SECURITY_DH_HPP_

#include <netp/core.hpp>
#include <netp/security/formula.hpp>

namespace netp { namespace security {

	/*
	 * refer to
	 * https://en.wikipedia.org/wiki/Diffie%E2%80%93Hellman_key_exchange
	 *
	 */
	const static u64_t DH_q = 0xffffffffffffffc5ULL;
	const static u64_t DH_g = 5;

	struct DH_factor {
		u64_t priv_key;
		u64_t pub_key;
	};

	inline static DH_factor dh_generate_factor() {
		DH_factor f = { 0,0 };
		while (f.priv_key == 0 || f.pub_key == 0) {
			f.priv_key = netp::random_u64();
			f.pub_key = formula::Q_powmod(DH_g, f.priv_key, DH_q);
		}
		return f;
	}
	inline static u64_t dh_generate_key(u64_t const& factor2_pub, u64_t const& factor1_priv) {
		return formula::Q_powmod(factor2_pub, factor1_priv, DH_q);
	}
}}
#endif