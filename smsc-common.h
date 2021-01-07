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

/* netrw private */
struct netrw_priv {
  int enable;
  spinlock_t enable_lock;

  struct smsc95xx_priv *smsc_priv;

  struct proc_dir_entry *root_dentry;

  /* stat */
  struct drv_stat rx_sum;
  spinlock_t rx_sum_lock;
  struct drv_stat tx_sum;
  spinlock_t tx_sum_lock;

  struct delayed_work collect_pcpu_stat;
};

#endif
