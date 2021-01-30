#include <netp/CPUID.hpp>

//this is not a generic detector, just used for fec/rq
#ifdef _NETP_MSVC
    #include <intrin.h>
#elif defined(_NETP_GCC)
    #if (defined(_NETP_ARCH_X86) || defined(_NETP_ARCH_X64))
        #include <cpuid.h>
        #ifdef __cpuid
            #undef __cpuid
        #endif
        #ifdef __cpuidex
               #undef __cpuidex 
        #endif

        #define __cpuid(out,type) do { __get_cpuid(type, (unsigned int*)(out), ((unsigned int*)(out))+1, ((unsigned int*)(out))+2, ((unsigned int*)(out))+3); } while(0)
        #define __cpuidex(out,type,level) \
             do { __cpuid_count(type, level, out[0], out[1], out[2], out[3]); } while(0)
    #elif defined(_NETP_ARCH_ARM)

    #endif
#else
    #error please add a way to detect CPU SIMD support at runtime 
#endif

namespace netp {
    CPUID_x86::CPUID_x86()
        : nIds_{ 0 },
        nExIds_{ 0 },
        isIntel_{ false },
        isAMD_{ false },
        f_1_ECX_{ 0 },
        f_1_EDX_{ 0 },
        f_7_EBX_{ 0 },
        f_7_ECX_{ 0 },
        f_81_ECX_{ 0 },
        f_81_EDX_{ 0 },
        data_{},
        extdata_{}
    {

#if defined(_NETP_ARCH_X86) || defined(_NETP_ARCH_X64)
        //int cpuInfo[4] = {-1};
        std::array<int, 4> cpui;

        // Calling __cpuid with 0x0 as the function_id argument
        // gets the number of the highest valid function ID.
        __cpuid(cpui.data(), 0);
        nIds_ = cpui[0];

        for (int i = 0; i <= nIds_; ++i)
        {
            __cpuidex(cpui.data(), i, 0);
            data_.push_back(cpui);
        }

        // Capture vendor string
        char vendor[0x20];
        memset(vendor, 0, sizeof(vendor));
        *reinterpret_cast<int*>(vendor) = data_[0][1];
        *reinterpret_cast<int*>(vendor + 4) = data_[0][3];
        *reinterpret_cast<int*>(vendor + 8) = data_[0][2];
        vendor_ = vendor;
        if (vendor_ == "GenuineIntel")
        {
            isIntel_ = true;
        }
        else if (vendor_ == "AuthenticAMD")
        {
            isAMD_ = true;
        }

        // load bitset with flags for function 0x00000001
        if (nIds_ >= 1)
        {
            f_1_ECX_ = data_[1][2];
            f_1_EDX_ = data_[1][3];
        }

        // load bitset with flags for function 0x00000007
        if (nIds_ >= 7)
        {
            f_7_EBX_ = data_[7][1];
            f_7_ECX_ = data_[7][2];
        }

        // Calling __cpuid with 0x80000000 as the function_id argument
        // gets the number of the highest valid extended ID.
        __cpuid(cpui.data(), 0x80000000);
        nExIds_ = cpui[0];

        char brand[0x40];
        memset(brand, 0, sizeof(brand));

        for (int i = 0x80000000; i <= nExIds_; ++i)
        {
            __cpuidex(cpui.data(), i, 0);
            extdata_.push_back(cpui);
        }

        // load bitset with flags for function 0x80000001
        if (nExIds_ >= 0x80000001)
        {
            f_81_ECX_ = extdata_[1][2];
            f_81_EDX_ = extdata_[1][3];
        }

        // Interpret CPU brand string if reported
        if (nExIds_ >= 0x80000004)
        {
            memcpy(brand, extdata_[2].data(), sizeof(cpui));
            memcpy(brand + 16, extdata_[3].data(), sizeof(cpui));
            memcpy(brand + 32, extdata_[4].data(), sizeof(cpui));
            brand_ = brand;
        }
#endif
    }

    CPUID_arm::CPUID_arm():
        flag(0)
    {
#ifdef NETP_ENABLE_NEON
        flag |= f_neon;
#endif
    }

    const  CPUID_x86 CPUID::x86_Rep;
    const  CPUID_arm CPUID::arm_Rep;
}