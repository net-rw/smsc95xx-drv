/* smsc-util.h
 *
 * Copyright (c) 2021 Leesoo Ahn <lsahn@ooseel.net>
 *
 * This software is distributed under the terms of the BSD or GPL license.
 */

#ifndef _SMSC_UTIL_H
#define _SMSC_UTIL_H

/* carefully use _if_ called under locks */
#define retval_if_fail(expr, retval) \
  do { \
    if (!(expr)) \
      return (retval); \
  } while (0)

/* carefully use _if_ called under locks */
#define retcall_if_fail(expr, func) \
  do { \
    if (!(expr)) {\
      func; \
      return; \
    } \
  } while (0)

#endif
