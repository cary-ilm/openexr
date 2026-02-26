/*
** SPDX-License-Identifier: BSD-3-Clause
** Copyright Contributors to the OpenEXR Project.
*/

#ifndef OPENEXR_PRIVATE_THREAD_H
#define OPENEXR_PRIVATE_THREAD_H

#include "openexr_config.h"

// Thread-safe single initialization using C11 threads.h where available,
// pthread on macOS, Windows native primitives on Windows, or a simple
// variable if threading is completely disabled.

#if ILMTHREAD_THREADING_ENABLED

#if defined(__APPLE__)
#include <pthread.h>
#        define ONCE_FLAG_INIT PTHREAD_ONCE_INIT
typedef pthread_once_t once_flag;
static inline void
call_once (once_flag* flag, void (*func) (void))
{
    (void) pthread_once (flag, func);
}
#else
#include <threads.h>
#endif

#endif /* ILMTHREAD_THREADING_ENABLED */

#endif /* OPENEXR_PRIVATE_THREAD_H */
