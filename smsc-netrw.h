/* smsc-netrw.h
 *
 * Copyright (c) 2021 Leesoo Ahn <lsahn@ooseel.net>
 *
 * This software is distributed under the terms of the BSD or GPL license.
 */

#ifndef _SMSC_NETRW_H
#define _SMSC_NETRW_H

int smsc_netrw_init(void);
int smsc_netrw_exit(void);
int netrw_skb_rx_hook(struct sk_buff *skb);
int netrw_skb_tx_hook(struct sk_buff *skb);

#endif
