#ifndef NETP_MATH_FORMULA_HPP
#define NETP_MATH_FORMULA_HPP

#include <netp/core.hpp>
namespace netp { namespace security {

	namespace formula {

		inline static u64_t Q_mulmod(u64_t a, u64_t b, u64_t q) {
			u64_t rt = 0;
			while (b) {
				u64_t q_a = (q - a);
				if (b&0x1) {
					if (rt >= q_a) rt -= q_a;
					else rt += a;
				}
				if (a >= q_a) a -= q_a;
				else a += a;

				b >>= 1;
			}
			return rt;
		}

		inline static u64_t Q_powmod(u64_t a, u64_t b, u64_t q) {
			u64_t rt = 1;
			if (a > q) a -= q;
			if (b > q) b -= q;

			while (b) {
				if (b&0x1) rt = Q_mulmod(rt, a, q);
				b >>= 1;
				a = Q_mulmod(a, a, q);
			}
			return rt;
		}
	}//end of formula
}}
#endif