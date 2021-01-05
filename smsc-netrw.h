/* smsc-netrw.h
 *
 * Copyright (c) 2021 Leesoo Ahn <lsahn@ooseel.net>
 *
 * This software is distributed under the terms of the BSD or GPL license.
 */

#ifndef _SMSC_NETRW_H
#define _SMSC_NETRW_H

#include "smsc95xx-priv.h"

int smsc_netrw_init(struct smsc95xx_priv *priv);
int smsc_netrw_exit(struct smsc95xx_priv *priv);
int netrw_skb_rx_hook(struct usbnet *dev, struct sk_buff *skb);
int netrw_skb_tx_hook(struct usbnet *dev, struct sk_buff *skb);

#endif
