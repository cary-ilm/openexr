//
// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) Contributors to the OpenEXR Project.
//

#include <testMathExc.h>
#include <mathFuncs.h>
#include <IexMathFloatExc.h>
#include <IexMathExc.h>
#include <cstdio>
#include <assert.h>
#include <string.h>

namespace
{

void
print (float f)
{
    std::printf ("%g\n", static_cast<double> (f));
}

void
test1 ()
{
    //
    // Turn math exception handling off, and verify that no C++ exceptions
    // are thrown even though "exceptional" floating-point operations are
    // performed.
    //

    std::puts ("invalid operations / exception handling off");

    IEX_INTERNAL_NAMESPACE::mathExcOn (0);

    for (int i = 0; i < 3; ++i)
    {
        try
        {
            print (divide (1, 0));    // division by zero
            print (root (-1));        // invalid operation
            print (grow (1000, 100)); // overflow
        }
        catch (...)
        {
            assert (false);
        }
    }
}

void
test2a ()
{
    try
    {
        print (divide (1, 0)); // division by zero
    }
    catch (const IEX_INTERNAL_NAMESPACE::DivzeroExc& e)
    {
        std::printf ("caught exception: %s\n", e.what ());
    }
}

void
test2b ()
{
    try
    {
        print (root (-1)); // invalid operation
    }
    catch (const IEX_INTERNAL_NAMESPACE::InvalidFpOpExc& e)
    {
        std::printf ("caught exception: %s\n", e.what ());
    }
}

void
test2c ()
{
    try
    {
        print (grow (1000, 100)); // overflow
    }
    catch (const IEX_INTERNAL_NAMESPACE::OverflowExc& e)
    {
        std::printf ("caught exception: %s\n", e.what ());
    }
}

void
test2 ()
{
    //
    // Turn math exception handling on, and verify that C++ exceptions
    // are thrown when "exceptional" floating-point operations are
    // performed.
    //

    std::puts ("invalid operations / exception handling on");

    IEX_INTERNAL_NAMESPACE::mathExcOn (
        IEX_INTERNAL_NAMESPACE::IEEE_OVERFLOW |
        IEX_INTERNAL_NAMESPACE::IEEE_DIVZERO |
        IEX_INTERNAL_NAMESPACE::IEEE_INVALID);

    for (int i = 0; i < 3; ++i)
    {
        test2a ();
        test2b ();
        test2c ();
    }
}

void
test3 ()
{
    //
    // Verify that getMathExcOn() returns the value that
    // was most recently set with setMathExcOn().
    //

#if defined(HAVE_UCONTEXT_H) &&                                                \
    (defined(IEX_HAVE_SIGCONTEXT_CONTROL_REGISTER_SUPPORT) ||                  \
     defined(IEX_HAVE_CONTROL_REGISTER_SUPPORT))

    std::puts ("getMathExc()");

    int when = 0;

    IEX_INTERNAL_NAMESPACE::mathExcOn (when);
    assert (IEX_INTERNAL_NAMESPACE::getMathExcOn () == when);

    when = IEX_INTERNAL_NAMESPACE::IEEE_OVERFLOW;

    IEX_INTERNAL_NAMESPACE::mathExcOn (when);
    assert (IEX_INTERNAL_NAMESPACE::getMathExcOn () == when);

    when = IEX_INTERNAL_NAMESPACE::IEEE_DIVZERO;

    IEX_INTERNAL_NAMESPACE::mathExcOn (when);
    assert (IEX_INTERNAL_NAMESPACE::getMathExcOn () == when);

    when = IEX_INTERNAL_NAMESPACE::IEEE_INVALID;

    IEX_INTERNAL_NAMESPACE::mathExcOn (when);
    assert (IEX_INTERNAL_NAMESPACE::getMathExcOn () == when);

    when = IEX_INTERNAL_NAMESPACE::IEEE_OVERFLOW |
           IEX_INTERNAL_NAMESPACE::IEEE_DIVZERO |
           IEX_INTERNAL_NAMESPACE::IEEE_INVALID;

    IEX_INTERNAL_NAMESPACE::mathExcOn (when);
    assert (IEX_INTERNAL_NAMESPACE::getMathExcOn () == when);
#endif
}

} // namespace

void
testMathExc ()
{
    std::puts ("See if floating-point exceptions work:");

    test1 ();
    test2 ();
    test3 ();

    // test2/test3 leave IEEE traps unmasked; restore default masking before
    // returning so process teardown (e.g. libstdc++ after testBaseExc) does
    // not raise SIGFPE (CTest reports that as "Exception: Numerical").
    IEX_INTERNAL_NAMESPACE::mathExcOn (0);

    std::puts (" ok");
}
