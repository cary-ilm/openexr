//
// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) Contributors to the OpenEXR Project.
//

//------------------------------------------------------------------------
//
//	Functions to control floating point exceptions.
//
//------------------------------------------------------------------------

#include "IexMathFpu.h"

#include <IexConfig.h>
#include <stdint.h>
#include <stdio.h>

#if 0
#    include <iostream>
#    define debug(x) (std::cout << x << std::flush)
#else
#    define debug(x)
#endif

#include <IexConfigInternal.h>
#if defined(HAVE_UCONTEXT_H) &&                                                \
    (defined(IEX_HAVE_SIGCONTEXT_CONTROL_REGISTER_SUPPORT) ||                  \
     defined(IEX_HAVE_CONTROL_REGISTER_SUPPORT))

#    include <cstring>
#    include <iostream>
#    include <signal.h>
#    include <stdint.h>
#    include <ucontext.h>

IEX_INTERNAL_NAMESPACE_SOURCE_ENTER

namespace FpuControl
{

//-------------------------------------------------------------------
//
//    Modern x86 processors and all AMD64 processors have two
//    sets of floating-point control/status registers: cw and sw
//    for legacy x87 stack-based arithmetic, and mxcsr for
//    SIMD arithmetic.  When setting exception masks or checking
//    for exceptions, we must set/check all relevant registers,
//    since applications may contain code that uses either FP
//    model.
//
//    These functions handle both FP models for x86 and AMD64.
//
//-------------------------------------------------------------------

//-------------------------------------------------------------------
//
//    Restore the control register state from a signal handler
//    user context, optionally clearing the exception bits
//    in the restored control register, if applicable.
//
//-------------------------------------------------------------------

void restoreControlRegs (const ucontext_t& ucon, bool clearExceptions = false);

//------------------------------------------------------------
//
//    Set exception mask bits in the control register state.
//    A value of 1 means the exception is masked, a value of
//    0 means the exception is enabled.
//
//    setExceptionMask returns the previous mask value.  If
//    the 'exceptions' pointer is non-null, it returns in
//    this argument the FPU exception bits.
//
//------------------------------------------------------------

const int INVALID_EXC   = (1 << 0);
const int DENORMAL_EXC  = (1 << 1);
const int DIVZERO_EXC   = (1 << 2);
const int OVERFLOW_EXC  = (1 << 3);
const int UNDERFLOW_EXC = (1 << 4);
const int INEXACT_EXC   = (1 << 5);
const int ALL_EXC = INVALID_EXC | DENORMAL_EXC | DIVZERO_EXC | OVERFLOW_EXC |
                    UNDERFLOW_EXC | INEXACT_EXC;

int setExceptionMask (int mask, int* exceptions = 0);
int getExceptionMask ();

//---------------------------------------------
//
//    Get/clear the exception bits in the FPU.
//
//---------------------------------------------

int  getExceptions ();
void clearExceptions ();

//------------------------------------------------------------------
//
//    Everything below here is implementation.  Do not use these
//    constants or functions in your applications or libraries.
//    This is not the code you're looking for.  Move along.
//
//    Optimization notes -- on a Pentium 4, at least, it appears
//    to be faster to get the mxcsr first and then the cw; and to
//    set the cw first and then the mxcsr.  Also, it seems to
//    be faster to clear the sw exception bits after setting
//    cw and mxcsr.
//
//------------------------------------------------------------------

static inline uint16_t
getSw ()
{
    uint16_t sw;
    asm volatile ("fnstsw %0" : "=m"(sw) :);
    return sw;
}

static inline void
setCw (uint16_t cw)
{
    asm volatile ("fldcw %0" : : "m"(cw));
}

static inline uint16_t
getCw ()
{
    uint16_t cw;
    asm volatile ("fnstcw %0" : "=m"(cw) :);
    return cw;
}

static inline void
setMxcsr (uint32_t mxcsr, bool clearExceptions)
{
    mxcsr &= clearExceptions ? 0xffffffc0 : 0xffffffff;
    asm volatile ("ldmxcsr %0" : : "m"(mxcsr));
}

static inline uint32_t
getMxcsr ()
{
    uint32_t mxcsr;
    asm volatile ("stmxcsr %0" : "=m"(mxcsr) :);
    return mxcsr;
}

static inline int
calcMask (uint16_t cw, uint32_t mxcsr)
{
    //
    // Hopefully, if the user has been using FpuControl functions,
    // the masks are the same, but just in case they're not, we
    // AND them together to report the proper subset of the masks.
    //

    return (cw & ALL_EXC) & ((mxcsr >> 7) & ALL_EXC);
}

inline int
setExceptionMask (int mask, int* exceptions)
{
    uint16_t cw    = getCw ();
    uint32_t mxcsr = getMxcsr ();

    if (exceptions) *exceptions = (mxcsr & ALL_EXC) | (getSw () & ALL_EXC);

    int oldmask = calcMask (cw, mxcsr);

    //
    // The exception constants are chosen very carefully so that
    // we can do a simple mask and shift operation to insert
    // them into the control words.  The mask operation is for
    // safety, in case the user accidentally set some other
    // bits in the exception mask.
    //

    mask &= ALL_EXC;
    cw    = (cw & ~ALL_EXC) | mask;
    mxcsr = (mxcsr & ~(ALL_EXC << 7)) | (mask << 7);

    setCw (cw);
    setMxcsr (mxcsr, false);

    return oldmask;
}

inline int
getExceptionMask ()
{
    uint32_t mxcsr = getMxcsr ();
    uint16_t cw    = getCw ();
    return calcMask (cw, mxcsr);
}

inline int
getExceptions ()
{
    return (getMxcsr () | getSw ()) & ALL_EXC;
}

void
clearExceptions ()
{
    uint32_t mxcsr = getMxcsr () & 0xffffffc0;
    asm volatile ("ldmxcsr %0\n"
                  "fnclex"
                  :
                  : "m"(mxcsr));
}

// If the fpe was taken while doing a float-to-int cast using the x87,
// the rounding mode and possibly the precision will be wrong.  So instead
// of restoring to the state as of the fault, we force the rounding mode
// to be 'nearest' and the precision to be double extended.
//
// rounding mode is in bits 10-11, value 00 == round to nearest
// precision is in bits 8-9, value 11 == double extended (80-bit)
//
const uint16_t cwRestoreMask = ~((3 << 10) | (3 << 8));
const uint16_t cwRestoreVal  = (0 << 10) | (3 << 8);

#    ifdef IEX_HAVE_CONTROL_REGISTER_SUPPORT

inline void
restoreControlRegs (const ucontext_t& ucon, bool clearExceptions)
{
    setCw ((ucon.uc_mcontext.fpregs->cwd & cwRestoreMask) | cwRestoreVal);
    setMxcsr (ucon.uc_mcontext.fpregs->mxcsr, clearExceptions);
}

#    else

//
// Ugly, the mxcsr isn't defined in GNU libc ucontext_t, but
// it's passed to the signal handler by the kernel.  Use
// the kernel's version of the ucontext to get it, see
// <asm/sigcontext.h>
//
// Do not include <asm/sigcontext.h>: with modern glibc, <signal.h> already
// brings in bits/sigcontext.h, and including asm/sigcontext.h redefines the
// same structs.

inline void
restoreControlRegs (const ucontext_t& ucon, bool clearExceptions)
{
#        if (defined(__linux__) && defined(__i386__)) ||                       \
            defined(__ANDROID_API__)
    setCw ((ucon.uc_mcontext.fpregs->cw & cwRestoreMask) | cwRestoreVal);
#        else
    setCw ((ucon.uc_mcontext.fpregs->cwd & cwRestoreMask) | cwRestoreVal);
#        endif

#        if defined(__linux__) && defined(__i386__)
    //
    // uc_mcontext.fpregs points at glibc's struct _libc_fpstate, which is
    // not layout-compatible with the kernel's struct _fpstate where mxcsr
    // and the FXSR magic field live. Reinterpreting fpregs as struct _fpstate
    // reads unrelated memory as MXCSR and can pass garbage to LDMXCSR.
    // Use the processor's MXCSR instead, then clear sticky exception bits.
    //
    uint32_t mxcsr = getMxcsr ();
    if (mxcsr == 0) mxcsr = 0x1f80;
    setMxcsr (mxcsr, clearExceptions);
#        else
    _fpstate* kfp = reinterpret_cast<_fpstate*> (ucon.uc_mcontext.fpregs);
    setMxcsr (kfp->mxcsr, clearExceptions);
#        endif
}

#    endif

} // namespace FpuControl

namespace
{

//
// Sticky IEEE flags at SIGFPE delivery: the kernel often saves them in the
// ucontext frame while the live MXCSR / x87 status visible to stmxcsr/fnstsw
// in the handler is already cleared. Merge saved-frame bits with a live read
// so sqrt(-1) (SSE invalid) still classifies on Linux i386.
//
static int
fpExcBitsFromUcontext (const ucontext_t* uc)
{
#    if defined(__linux__) && defined(__i386__)
    //
    // Read sticky bits from the signal frame without including asm/sigcontext.h
    // (see comment above restoreControlRegs). Layout matches Linux
    // struct _fpstate_32: sw @4, magic @106, mxcsr @132 when magic==0 (FXSAVE).
    //
    if (!uc || !uc->uc_mcontext.fpregs) return 0;
    const uint8_t* b = reinterpret_cast<const uint8_t*> (uc->uc_mcontext.fpregs);
    uint32_t swdw = 0;
    std::memcpy (&swdw, b + 4, sizeof (swdw));
    int exc = (int)(swdw & FpuControl::ALL_EXC);
    uint16_t magic = 0;
    std::memcpy (&magic, b + 106, sizeof (magic));
    if (magic == 0)
    {
        uint32_t mxcsr = 0;
        std::memcpy (&mxcsr, b + 132, sizeof (mxcsr));
        exc |= (int)(mxcsr & FpuControl::ALL_EXC);
    }
    return exc;
#    elif defined(__x86_64__)
    if (!uc || !uc->uc_mcontext.fpregs) return 0;
    const auto* gfp = uc->uc_mcontext.fpregs;
    return (int)((gfp->swd & FpuControl::ALL_EXC) |
                 (gfp->mxcsr & FpuControl::ALL_EXC));
#    else
    (void) uc;
    return 0;
#    endif
}

volatile FpExceptionHandler fpeHandler = 0;

static struct sigaction s_prevSigFpe;
static bool s_sigFpeHooked = false;

extern "C" void
catchSigFpe (int sig, siginfo_t* info, ucontext_t* ucon)
{
    debug ("catchSigFpe (sig = " << sig << ", ...)\n");

    //
    // Capture sticky exception bits before restoreControlRegs() clears them.
    // On Linux i386, siginfo.si_code for some IEEE invalid ops (e.g. sqrt of
    // a negative) is often not FPE_FLTINV, so the si_code switch alone would
    // miss and map the fault to MathExc instead of InvalidFpOpExc.
    //
    const int excBits =
        FpuControl::getExceptions () | fpExcBitsFromUcontext (ucon);

    FpuControl::restoreControlRegs (*ucon, true);

    if (fpeHandler == 0) return;

    if (info->si_code == SI_USER)
    {
        fpeHandler (
            0,
            "Floating-point exception, caused by "
            "a signal sent from another process.");
        return;
    }

    if (sig == SIGFPE)
    {
        switch (info->si_code)
        {
                //
                // IEEE 754 floating point exceptions:
                //

            case FPE_FLTDIV:
                fpeHandler (IEEE_DIVZERO, "Floating-point division by zero.");
                return;

            case FPE_FLTOVF:
                fpeHandler (IEEE_OVERFLOW, "Floating-point overflow.");
                return;

            case FPE_FLTUND:
                fpeHandler (IEEE_UNDERFLOW, "Floating-point underflow.");
                return;

            case FPE_FLTRES:
                fpeHandler (IEEE_INEXACT, "Inexact floating-point result.");
                return;

            case FPE_FLTINV:
                fpeHandler (IEEE_INVALID, "Invalid floating-point operation.");
                return;

                //
                // Other arithmetic exceptions which can also
                // be trapped by the operating system:
                //

            case FPE_INTDIV:
                fpeHandler (0, "Integer division by zero.");
                return;

            case FPE_INTOVF:
                fpeHandler (0, "Integer overflow.");
                return;

            case FPE_FLTSUB:
                fpeHandler (0, "Subscript out of range.");
                return;
        }

        //
        // Fall back to hardware sticky flags when si_code is missing or
        // wrong (see excBits comment above). Order matches
        // handleExceptionsSetInRegisters().
        //
        if (excBits & FpuControl::DIVZERO_EXC)
        {
            fpeHandler (IEEE_DIVZERO, "Floating-point division by zero.");
            return;
        }
        if (excBits & FpuControl::OVERFLOW_EXC)
        {
            fpeHandler (IEEE_OVERFLOW, "Floating-point overflow.");
            return;
        }
        if (excBits & FpuControl::UNDERFLOW_EXC)
        {
            fpeHandler (IEEE_UNDERFLOW, "Floating-point underflow.");
            return;
        }
        if (excBits & FpuControl::INEXACT_EXC)
        {
            fpeHandler (IEEE_INEXACT, "Inexact floating-point result.");
            return;
        }
        if (excBits & FpuControl::INVALID_EXC)
        {
            fpeHandler (IEEE_INVALID, "Invalid floating-point operation.");
            return;
        }
    }

    fpeHandler (0, "Floating-point exception.");
}

} // namespace

void
setFpExceptions (int when)
{
    int mask = FpuControl::ALL_EXC;

    if (when & IEEE_OVERFLOW) mask &= ~FpuControl::OVERFLOW_EXC;
    if (when & IEEE_UNDERFLOW) mask &= ~FpuControl::UNDERFLOW_EXC;
    if (when & IEEE_DIVZERO) mask &= ~FpuControl::DIVZERO_EXC;
    if (when & IEEE_INEXACT) mask &= ~FpuControl::INEXACT_EXC;
    if (when & IEEE_INVALID) mask &= ~FpuControl::INVALID_EXC;

    //
    // The Linux kernel apparently sometimes passes
    // incorrect si_info to signal handlers unless
    // the exception flags are cleared.
    //
    // XXX is this still true on 2.4+ kernels?
    //

    FpuControl::setExceptionMask (mask);
    FpuControl::clearExceptions ();
}

int
fpExceptions ()
{
    int mask = FpuControl::getExceptionMask ();

    int when = 0;

    if (!(mask & FpuControl::OVERFLOW_EXC)) when |= IEEE_OVERFLOW;
    if (!(mask & FpuControl::UNDERFLOW_EXC)) when |= IEEE_UNDERFLOW;
    if (!(mask & FpuControl::DIVZERO_EXC)) when |= IEEE_DIVZERO;
    if (!(mask & FpuControl::INEXACT_EXC)) when |= IEEE_INEXACT;
    if (!(mask & FpuControl::INVALID_EXC)) when |= IEEE_INVALID;

    return when;
}

void
handleExceptionsSetInRegisters ()
{
    if (fpeHandler == 0) return;

    int mask = FpuControl::getExceptionMask ();

    int exc = FpuControl::getExceptions ();

    if (!(mask & FpuControl::DIVZERO_EXC) && (exc & FpuControl::DIVZERO_EXC))
    {
        fpeHandler (IEEE_DIVZERO, "Floating-point division by zero.");
        return;
    }

    if (!(mask & FpuControl::OVERFLOW_EXC) && (exc & FpuControl::OVERFLOW_EXC))
    {
        fpeHandler (IEEE_OVERFLOW, "Floating-point overflow.");
        return;
    }

    if (!(mask & FpuControl::UNDERFLOW_EXC) &&
        (exc & FpuControl::UNDERFLOW_EXC))
    {
        fpeHandler (IEEE_UNDERFLOW, "Floating-point underflow.");
        return;
    }

    if (!(mask & FpuControl::INEXACT_EXC) && (exc & FpuControl::INEXACT_EXC))
    {
        fpeHandler (IEEE_INEXACT, "Inexact floating-point result.");
        return;
    }

    if (!(mask & FpuControl::INVALID_EXC) && (exc & FpuControl::INVALID_EXC))
    {
        fpeHandler (IEEE_INVALID, "Invalid floating-point operation.");
        return;
    }
}

void
setFpExceptionHandler (FpExceptionHandler handler)
{
    if (!s_sigFpeHooked)
    {
        struct sigaction action;
        sigemptyset (&action.sa_mask);
        // Do not use SA_NODEFER (SA_NOMASK): re-entering this handler on the
        // same thread while translating SIGFPE to a C++ exception is unsafe.
        action.sa_flags     = SA_SIGINFO;
        action.sa_sigaction = (void (*) (int, siginfo_t*, void*)) catchSigFpe;
        action.sa_restorer  = 0;

        sigaction (SIGFPE, &action, &s_prevSigFpe);
        s_sigFpeHooked = true;
    }

    fpeHandler = handler;
}

void
unsetFpExceptionHandler ()
{
    fpeHandler = 0;
    if (s_sigFpeHooked)
    {
        sigaction (SIGFPE, &s_prevSigFpe, nullptr);
        s_sigFpeHooked = false;
    }
}

IEX_INTERNAL_NAMESPACE_SOURCE_EXIT

#else

#    include <assert.h>
#    include <signal.h>

IEX_INTERNAL_NAMESPACE_SOURCE_ENTER

namespace
{
volatile FpExceptionHandler fpeHandler = 0;
void
fpExc_ (int x)
{
    if (fpeHandler != 0) { fpeHandler (x, ""); }
    else { assert (0 != "Floating point exception"); }
}

static void (*s_prevSigFpeHandler) (int) = SIG_DFL;
static bool s_sigFpeHooked = false;
} // namespace

void
setFpExceptions (int)
{}

void
setFpExceptionHandler (FpExceptionHandler handler)
{
    // improve floating point exception handling nanoscopically above "nothing at all"
    if (!s_sigFpeHooked)
    {
        void (*prev)(int) = signal (SIGFPE, fpExc_);
        if (prev != SIG_ERR)
        {
            s_prevSigFpeHandler = prev;
            s_sigFpeHooked     = true;
        }
    }
    fpeHandler = handler;
}

void
unsetFpExceptionHandler ()
{
    fpeHandler = 0;
    if (s_sigFpeHooked)
    {
        if (signal (SIGFPE, s_prevSigFpeHandler) == SIG_ERR)
            signal (SIGFPE, SIG_DFL);
        s_sigFpeHooked = false;
    }
}

int
fpExceptions ()
{
    return 0;
}

void
handleExceptionsSetInRegisters ()
{
    // No implementation on this platform
}

IEX_INTERNAL_NAMESPACE_SOURCE_EXIT

#endif
