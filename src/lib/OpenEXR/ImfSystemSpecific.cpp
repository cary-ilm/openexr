//
// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) DreamWorks Animation LLC and Contributors of the OpenEXR Project
//

#include "ImfSystemSpecific.h"
#include "ImfNamespace.h"
#include "ImfSimd.h"
#include "OpenEXRConfig.h"
#include "OpenEXRConfigInternal.h"
#if defined(_MSC_VER)
#    include <intrin.h>
#elif defined(IMF_HAVE_SSE2) && defined(__GNUC__) && !defined(__e2k__)
#    include <cpuid.h>
#endif

OPENEXR_IMF_INTERNAL_NAMESPACE_SOURCE_ENTER

namespace
{
#if defined(IMF_HAVE_SSE2) && defined(__GNUC__) && !defined(__e2k__)

// Use __get_cpuid from <cpuid.h> instead of inline asm so PIC builds work
// (inline asm clobbers %ebx which may be the PIC base register). See issue #128.
void
cpuid (unsigned int n, unsigned int& eax, unsigned int& ebx, unsigned int& ecx,
       unsigned int& edx)
{
    __get_cpuid (n, &eax, &ebx, &ecx, &edx);
}

#elif defined(_MSC_VER) &&                                                     \
    (defined(_M_IX86) || (defined(_M_AMD64) && !defined(_M_ARM64EC)))

// Helper functions for MSVC
void
cpuid (unsigned int n, unsigned int& eax, unsigned int& ebx, unsigned int& ecx,
       unsigned int& edx)
{
    int cpuInfo[4] = {-1};
    __cpuid (cpuInfo, static_cast<int> (n));
    eax = static_cast<unsigned int> (cpuInfo[0]);
    ebx = static_cast<unsigned int> (cpuInfo[1]);
    ecx = static_cast<unsigned int> (cpuInfo[2]);
    edx = static_cast<unsigned int> (cpuInfo[3]);
}

#else // IMF_HAVE_SSE2 && __GNUC__ && !__e2k__

// Helper functions for generic compiler - all disabled
void
cpuid (unsigned int n, unsigned int& eax, unsigned int& ebx, unsigned int& ecx,
       unsigned int& edx)
{
    eax = ebx = ecx = edx = 0;
}

#endif // IMF_HAVE_SSE2 && __GNUC__ && !__e2k__

#if defined(_MSC_VER) &&                                                     \
    (defined(_M_IX86) || (defined(_M_AMD64) && !defined(_M_ARM64EC)))

void
xgetbv (unsigned int n, unsigned int& eax, unsigned int& edx)
{
    unsigned long long v = _xgetbv (static_cast<int> (n));
    eax = static_cast<unsigned int> (v & 0xffffffffu);
    edx = static_cast<unsigned int> (v >> 32);
}

#elif defined(IMF_HAVE_GCC_INLINEASM_X86)

// No GCC intrinsic or cpuid.h wrapper for xgetbv; inline asm is required.
// xgetbv does not clobber %ebx so PIC is not affected (unlike cpuid).
void
xgetbv (unsigned int n, unsigned int& eax, unsigned int& edx)
{
    __asm__ __volatile__ ("xgetbv"
                          : /* Output  */ "=a"(eax), "=d"(edx)
                          : /* Input   */ "c"(n)
                          : /* Clobber */);
}

#else

void
xgetbv (unsigned int n, unsigned int& eax, unsigned int& edx)
{
    eax = edx = 0;
}

#endif

} // namespace

CpuId::CpuId ()
    : sse2 (false)
    , sse3 (false)
    , ssse3 (false)
    , sse4_1 (false)
    , sse4_2 (false)
    , avx (false)
    , f16c (false)
{
#if defined(__e2k__) // e2k - MCST Elbrus 2000 architecture
    // Use IMF_HAVE definitions to determine e2k CPU features
#    if defined(IMF_HAVE_SSE2)
    sse2 = true;
#    endif
#    if defined(IMF_HAVE_SSE3)
    sse3 = true;
#    endif
#    if defined(IMF_HAVE_SSSE3)
    ssse3 = true;
#    endif
#    if defined(IMF_HAVE_SSE4_1)
    sse4_1 = true;
#    endif
#    if defined(IMF_HAVE_SSE4_2)
    sse4_2 = true;
#    endif
#    if defined(IMF_HAVE_AVX)
    avx = true;
#    endif
#    if defined(IMF_HAVE_F16C)
    f16c = true;
#    endif
#else // x86/x86_64
    bool         osxsave = false;
    unsigned int max = 0, eax, ebx, ecx, edx;

    cpuid (0, max, ebx, ecx, edx);
    if (max > 0)
    {
        cpuid (1, eax, ebx, ecx, edx);
        sse2    = (edx & (1 << 26));
        sse3    = (ecx & (1 << 0));
        ssse3   = (ecx & (1 << 9));
        sse4_1  = (ecx & (1 << 19));
        sse4_2  = (ecx & (1 << 20));
        osxsave = (ecx & (1 << 27));
        avx     = (ecx & (1 << 28));
        f16c    = (ecx & (1 << 29));

        if (!osxsave) { avx = f16c = false; }
        else
        {
            xgetbv (0, eax, edx);
            // eax bit 1 - SSE managed, bit 2 - AVX managed
            if ((eax & 6) != 6) { avx = f16c = false; }
        }
    }
#endif
}

OPENEXR_IMF_INTERNAL_NAMESPACE_SOURCE_EXIT
