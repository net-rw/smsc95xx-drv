/* smsc-pktrace.c
 *
 * Copyright (c) 2021 Leesoo Ahn <lsahn@ooseel.net>
 *
 * This software is distributed under the terms of the BSD or GPL license.
 */

#include <linux/string.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/netfilter.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>

#include "smsc-pktrace.h"

#if IS_ENABLED(CONFIG_BRIDGE_NETFILTER)
# include <linux/netfilter_bridge.h>
# define __NFPROTO_NETRW      NFPROTO_BRIDGE
# define __NETRW_PRE_ROUTING  NF_BR_PRE_ROUTING
# define __NETRW_PRE_PRI      NF_BR_PRI_FIRST
# define __NETRW_POST_ROUTING NF_BR_POST_ROUTING
# define __NETRW_POST_PRI     NF_BR_PRI_LAST
#else
# include <linux/netfilter_ipv4.h>
# define __NFPROTO_NETRW      NFPROTO_IPV4
# define __NETRW_PRE_ROUTING  NF_INET_PRE_ROUTING
# define __NETRW_PRE_PRI      NF_IP_PRI_FIRST
# define __NETRW_POST_ROUTING NF_INET_POST_ROUTING
# define __NETRW_POST_PRI     NF_IP_PRI_LAST
#endif

#define NFPROTO_NETRW       __NFPROTO_NETRW
#define NETRW_PRE_ROUTING   __NETRW_PRE_ROUTING
#define NETRW_PRE_PRI       __NETRW_PRE_PRI
#define NETRW_POST_ROUTING  __NETRW_POST_ROUTING
#define NETRW_POST_PRI      __NETRW_POST_PRI

static
unsigned int rx_pre_routing_hook_func(void *priv,
                                      struct sk_buff *skb,
                                      const struct nf_hook_state *state)
{
  struct ethhdr *ethdr = skb_eth_hdr(skb);
  struct iphdr *iph = ip_hdr(skb);
  size_t len = 0;
  char ibuf[256];

  len = snprintf(ibuf+len, sizeof(ibuf)-len, "[RX] ",
                 &iph->saddr, &iph->daddr);
  switch (iph->protocol) {
  case 0x06: /* tcp */
    len += snprintf(ibuf+len, sizeof(ibuf)-len,
                    "(%pI4:%u) -> (%pI4:%u) proto (TCP)",
                    &iph->saddr, ntohs(tcp_hdr(skb)->source),
                    &iph->daddr, ntohs(tcp_hdr(skb)->dest));
    break;
  case 0x11: /* udp */
    len += snprintf(ibuf+len, sizeof(ibuf)-len,
                    "(%pI4:%u) -> (%pI4:%u) proto (UDP)",
                    &iph->saddr, ntohs(udp_hdr(skb)->source),
                    &iph->daddr, ntohs(udp_hdr(skb)->dest));
    break;
  default:
    len += snprintf(ibuf+len, sizeof(ibuf)-len,
                    "(%pI4) -> (%pI4) proto (0x%x)",
                    &iph->saddr, &iph->daddr, iph->protocol);
  }
  printk(KERN_INFO "%s\n", ibuf);

  return NF_ACCEPT;
}

static
unsigned int tx_post_routing_hook_func(void *priv,
                                       struct sk_buff *skb,
                                       const struct nf_hook_state *state)
{
  struct ethhdr *ethdr = skb_eth_hdr(skb);
  struct iphdr *iph = ip_hdr(skb);
  size_t len = 0;
  char ibuf[256];

  len = snprintf(ibuf+len, sizeof(ibuf)-len, "[TX] ",
                 &iph->saddr, &iph->daddr);
  switch (iph->protocol) {
  case 0x06: /* tcp */
    len += snprintf(ibuf+len, sizeof(ibuf)-len,
                    "(%pI4:%u) -> (%pI4:%u) proto (TCP)",
                    &iph->saddr, ntohs(tcp_hdr(skb)->source),
                    &iph->daddr, ntohs(tcp_hdr(skb)->dest));
    break;
  case 0x11: /* udp */
    len += snprintf(ibuf+len, sizeof(ibuf)-len,
                    "(%pI4:%u) -> (%pI4:%u) proto (UDP)",
                    &iph->saddr, ntohs(udp_hdr(skb)->source),
                    &iph->daddr, ntohs(udp_hdr(skb)->dest));
    break;
  default:
    len += snprintf(ibuf+len, sizeof(ibuf)-len,
                    "(%pI4) -> (%pI4) proto (0x%x)",
                    &iph->saddr, &iph->daddr, iph->protocol);
  }
  printk(KERN_INFO "%s\n", ibuf);

  return NF_ACCEPT;
}

static struct nf_hook_ops pktrace_nf_hook_ops[] = {
  {
    /* rx trace */
    .hook     = rx_pre_routing_hook_func,
    .pf       = NFPROTO_NETRW,
    .hooknum  = NETRW_PRE_ROUTING,
    .priority = NETRW_PRE_PRI,
  },
  {
    /* tx trace */
    .hook     = tx_post_routing_hook_func,
    .pf       = NFPROTO_NETRW,
    .hooknum  = NETRW_POST_ROUTING,
    .priority = NETRW_POST_PRI,
  },
};

int
smsc_pktrace_register(struct netrw_priv *pdata)
{
  int ret = -EINVAL;

  if (!pdata) {
    goto out;
  }

  ret = nf_register_net_hooks(dev_net(pdata->smsc_priv->dev->net),
                              pktrace_nf_hook_ops,
                              ARRAY_SIZE(pktrace_nf_hook_ops));
  if (ret < 0)
    goto out;

  /* success */
  ret = 0;

out:
  return ret;
}

void
smsc_pktrace_unregister(struct netrw_priv *pdata)
{
  if (!pdata)
    return;

  nf_unregister_net_hooks(dev_net(pdata->smsc_priv->dev->net),
                          pktrace_nf_hook_ops,
                          ARRAY_SIZE(pktrace_nf_hook_ops));
}
