// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the OpenEXR Project.

#ifndef OPENEXR_CORE_TEST_MALLOC_FAIL_H
#define OPENEXR_CORE_TEST_MALLOC_FAIL_H

#include <stdlib.h>

// Shared malloc-failure injection helpers used by multiple test files.
// Defined inline in a header so they resolve to a single definition even
// when the test sources are combined into one translation unit by a unity
// build.

inline int&
malloc_fail_counter ()
{
    static int s_malloc_fail_on = 0;
    return s_malloc_fail_on;
}

inline void*
failable_malloc (size_t bytes)
{
    int& fail = malloc_fail_counter ();
    if (fail == 1) return NULL;
    if (fail > 0) --fail;
    return malloc (bytes);
}

inline void
failable_free (void* p)
{
    if (!p) abort ();
    free (p);
}

inline void
set_malloc_fail_on (int count)
{
    malloc_fail_counter () = count;
}

inline void
set_malloc_fail_off ()
{
    malloc_fail_counter () = 0;
}

#endif // OPENEXR_CORE_TEST_MALLOC_FAIL_H
