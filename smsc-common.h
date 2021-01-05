/* smsc-common.h
 *
 * Copyright (c) 2021 Leesoo Ahn <lsahn@ooseel.net>
 *
 * This software is distributed under the terms of the BSD or GPL license.
 */

#ifndef _SMSC_COMMON_H
#define _SMSC_COMMON_H

/* -- stat -- */
struct drv_stat {
  u64 l3_unicst;
  u64 l3_brdcst;
  u64 l3_mltcst;

  u32 l2_arp;
};

#endif
