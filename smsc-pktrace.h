/* smsc-pktrace.h
 *
 * Copyright (c) 2021 Leesoo Ahn <lsahn@ooseel.net>
 *
 * This software is distributed under the terms of the BSD or GPL license.
 */

#ifndef _SMSC_PKTRACE_H
#define _SMSC_PKTRACE_H

#include "smsc-common.h"

int smsc_pktrace_register(struct netrw_priv *pdata);
void smsc_pktrace_unregister(struct netrw_priv *pdata);

#endif
