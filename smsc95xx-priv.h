/* smsc95xx-priv.h
 *
 * Copyright (c) 2020 Leesoo Ahn <lsahn@ooseel.net>
 *
 * This software is distributed under the terms of the BSD or GPL license.
 */

#ifndef _SMSC95XX_PRIV_H
#define _SMSC95XX_PRIV_H

#include <linux/netdevice.h>
#include <linux/mii.h>
#include <linux/usb.h>
#include <linux/usb/usbnet.h>

struct smsc95xx_priv {
	u32 chip_id;
	u32 mac_cr;
	u32 hash_hi;
	u32 hash_lo;
	u32 wolopts;
	spinlock_t mac_cr_lock;
	u8 features;
	u8 suspend_flags;
	u8 mdix_ctrl;
	bool link_ok;
	struct delayed_work carrier_check;
	struct usbnet *dev;
#if defined(NETRW_DRV)
  struct netrw_priv *netrw_priv;
#endif
};

#define GET_SMSC_PRIV(ptr) ((struct smsc95xx_priv *)ptr)

#endif
