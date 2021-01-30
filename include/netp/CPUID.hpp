#ifndef _NETP_CPUID_HPP
#define _NETP_CPUID_HPP

#include <vector>
#include <bitset>
#include <array>
#include <string>

#include <netp/core.hpp>

namespace netp {

	class CPUID_x86 {
        friend class CPUID;
        private:
            int nIds_;
            int nExIds_;
            std::string vendor_;
            std::string brand_;
            bool isIntel_;
            bool isAMD_;
            std::bitset<32> f_1_ECX_;
            std::bitset<32> f_1_EDX_;
            std::bitset<32> f_7_EBX_;
            std::bitset<32> f_7_ECX_;
            std::bitset<32> f_81_ECX_;
            std::bitset<32> f_81_EDX_;
            std::vector<std::array<int, 4>> data_;
            std::vector<std::array<int, 4>> extdata_;
    public:
            CPUID_x86();
	};

    enum arm_flag {
        f_neon = 1
    };
    class CPUID_arm {
        friend class CPUID;
       
        int flag;
    public:
        CPUID_arm();
    };

	class CPUID {
    private:
        static const CPUID_x86 x86_Rep;
        static const CPUID_arm arm_Rep;

    public:
        // getters
        static std::string Vendor(void) { return x86_Rep.vendor_; }
        static std::string Brand(void) { return x86_Rep.brand_; }

        static bool SSE3(void) { return x86_Rep.f_1_ECX_[0]; }
        static bool PCLMULQDQ(void) { return x86_Rep.f_1_ECX_[1]; }
        static bool MONITOR(void) { return x86_Rep.f_1_ECX_[3]; }
        static bool SSSE3(void) { return x86_Rep.f_1_ECX_[9]; }
        static bool FMA(void) { return x86_Rep.f_1_ECX_[12]; }
        static bool CMPXCHG16B(void) { return x86_Rep.f_1_ECX_[13]; }
        static bool SSE41(void) { return x86_Rep.f_1_ECX_[19]; }
        static bool SSE42(void) { return x86_Rep.f_1_ECX_[20]; }
        static bool MOVBE(void) { return x86_Rep.f_1_ECX_[22]; }
        static bool POPCNT(void) { return x86_Rep.f_1_ECX_[23]; }
        static bool AES(void) { return x86_Rep.f_1_ECX_[25]; }
        static bool XSAVE(void) { return x86_Rep.f_1_ECX_[26]; }
        static bool OSXSAVE(void) { return x86_Rep.f_1_ECX_[27]; }
        static bool AVX(void) { return x86_Rep.f_1_ECX_[28]; }
        static bool F16C(void) { return x86_Rep.f_1_ECX_[29]; }
        static bool RDRAND(void) { return x86_Rep.f_1_ECX_[30]; }

        static bool MSR(void) { return x86_Rep.f_1_EDX_[5]; }
        static bool CX8(void) { return x86_Rep.f_1_EDX_[8]; }
        static bool SEP(void) { return x86_Rep.f_1_EDX_[11]; }
        static bool CMOV(void) { return x86_Rep.f_1_EDX_[15]; }
        static bool CLFSH(void) { return x86_Rep.f_1_EDX_[19]; }
        static bool MMX(void) { return x86_Rep.f_1_EDX_[23]; }
        static bool FXSR(void) { return x86_Rep.f_1_EDX_[24]; }
        static bool SSE(void) { return x86_Rep.f_1_EDX_[25]; }
        static bool SSE2(void) { return x86_Rep.f_1_EDX_[26]; }

        static bool FSGSBASE(void) { return x86_Rep.f_7_EBX_[0]; }
        static bool BMI1(void) { return x86_Rep.f_7_EBX_[3]; }
        static bool HLE(void) { return x86_Rep.isIntel_ && x86_Rep.f_7_EBX_[4]; }
        static bool AVX2(void) { return x86_Rep.f_7_EBX_[5]; }
        static bool BMI2(void) { return x86_Rep.f_7_EBX_[8]; }
        static bool ERMS(void) { return x86_Rep.f_7_EBX_[9]; }
        static bool INVPCID(void) { return x86_Rep.f_7_EBX_[10]; }
        static bool RTM(void) { return x86_Rep.isIntel_ && x86_Rep.f_7_EBX_[11]; }
        static bool AVX512F(void) { return x86_Rep.f_7_EBX_[16]; }
        static bool RDSEED(void) { return x86_Rep.f_7_EBX_[18]; }
        static bool ADX(void) { return x86_Rep.f_7_EBX_[19]; }
        static bool AVX512PF(void) { return x86_Rep.f_7_EBX_[26]; }
        static bool AVX512ER(void) { return x86_Rep.f_7_EBX_[27]; }
        static bool AVX512CD(void) { return x86_Rep.f_7_EBX_[28]; }
        static bool SHA(void) { return x86_Rep.f_7_EBX_[29]; }

        static bool PREFETCHWT1(void) { return x86_Rep.f_7_ECX_[0]; }

        static bool LAHF(void) { return x86_Rep.f_81_ECX_[0]; }
        static bool LZCNT(void) { return x86_Rep.isIntel_ && x86_Rep.f_81_ECX_[5]; }
        static bool ABM(void) { return x86_Rep.isAMD_ && x86_Rep.f_81_ECX_[5]; }
        static bool SSE4a(void) { return x86_Rep.isAMD_ && x86_Rep.f_81_ECX_[6]; }
        static bool XOP(void) { return x86_Rep.isAMD_ && x86_Rep.f_81_ECX_[11]; }
        static bool TBM(void) { return x86_Rep.isAMD_ && x86_Rep.f_81_ECX_[21]; }

        static bool SYSCALL(void) { return x86_Rep.isIntel_ && x86_Rep.f_81_EDX_[11]; }
        static bool MMXEXT(void) { return x86_Rep.isAMD_ && x86_Rep.f_81_EDX_[22]; }
        static bool RDTSCP(void) { return x86_Rep.isIntel_ && x86_Rep.f_81_EDX_[27]; }
        static bool _3DNOWEXT(void) { return x86_Rep.isAMD_ && x86_Rep.f_81_EDX_[30]; }
        static bool _3DNOW(void) { return x86_Rep.isAMD_ && x86_Rep.f_81_EDX_[31]; }

        static bool NEON() { return (arm_Rep.flag&f_neon) != 0; }
	};
}

#endif