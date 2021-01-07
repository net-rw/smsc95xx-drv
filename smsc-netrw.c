/* smsc-netrw.c
 *
 * Copyright (c) 2021 Leesoo Ahn <lsahn@ooseel.net>
 *
 * This software is distributed under the terms of the BSD or GPL license.
 */

#include <linux/types.h>
#include <linux/percpu.h>
#include <linux/string.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/spinlock.h>
#include <linux/netdevice.h>
#include <linux/mii.h>
#include <linux/usb.h>
#include <linux/usb/usbnet.h>
#include <linux/irqflags.h>
#include <linux/byteorder/generic.h>
#include <linux/skbuff.h>
#include <linux/if_ether.h>
#include <linux/ip.h>

#include "smsc-netrw.h"
#include "smsc-common.h"
#include "smsc-util.h"

#define WQ_INTV_1SEC (1 * HZ)
#define WQ_INTV_2SEC (2 * HZ)
#define WQ_INTV_4SEC (4 * HZ)

struct netrw_pcpu {
  struct drv_stat nrx;
  struct drv_stat ntx;
};
DEFINE_PER_CPU(struct netrw_pcpu, nrw_pcpu);

/* called under per-cpu */
static void inline
adjust_stat_pcpu(struct sk_buff* skb,
                 struct drv_stat *ds_cpu)
{
  struct ethhdr *ethdr = skb_eth_hdr(skb);
  struct iphdr *iph = ip_hdr(skb);
  __be32 daddr = 0;

  switch (ntohs(ethdr->h_proto)) {
  case ETH_P_ARP:
    ds_cpu->l2_arp++;
    break;
  case ETH_P_IP:
    daddr = ntohl(iph->daddr);
    if (daddr == 0xffffffff) {
      ds_cpu->l3_brdcst++;
    } else if ((daddr & 0xe0000000) == 0xe0000000) {
      ds_cpu->l3_mltcst++;
    } else {
      ds_cpu->l3_unicst++;
    }
    break;
  default: break;
  }
}

/**
 * always returns 0
 * we never touch skb's life cycle
 *
 * called under softirq
 */
int
netrw_skb_rx_hook(struct usbnet *dev,
                  struct sk_buff *skb)
{
  struct netrw_priv *pn;

  rcu_read_lock_bh();
  pn = rcu_dereference_bh(GET_SMSC_PRIV(dev->data[0])->netrw_priv);
  retval_if_fail(pn->enable,
                 (rcu_read_unlock_bh(), 0));
  rcu_read_unlock_bh();

  /* per-cpu tasks */
  adjust_stat_pcpu(skb, this_cpu_ptr(&nrw_pcpu.nrx));

  return 0;
}

/**
 * always returns 0
 * we never touch skb's life cycle
 *
 * called under softirq
 */
int
netrw_skb_tx_hook(struct usbnet *dev,
                  struct sk_buff *skb)
{
  struct netrw_priv *pn;

  rcu_read_lock_bh();
  pn = rcu_dereference_bh(GET_SMSC_PRIV(dev->data[0])->netrw_priv);
  retval_if_fail(pn->enable,
                 (rcu_read_unlock_bh(), 0));
  rcu_read_unlock_bh();

  /* per-cpu tasks */
  adjust_stat_pcpu(skb, this_cpu_ptr(&nrw_pcpu.ntx));

  return 0;
}

/* called under lock */
static void sum_stats(struct drv_stat *dst,
                      struct drv_stat *src)
{
  if (!dst || !src)
    return;

  dst->l2_arp += src->l2_arp;
  dst->l3_unicst += src->l3_unicst;
  dst->l3_brdcst += src->l3_brdcst;
  dst->l3_mltcst += src->l3_mltcst;
}

static int collect_pcpu_stat_cb(void *data)
{
  struct netrw_priv *pn = (struct netrw_priv *)data;
  struct drv_stat rx, tx, *vptr;
  unsigned long flags;
  int enable;

  rcu_read_lock();
  enable = rcu_dereference(pn)->enable;
  retval_if_fail(enable,
                 (rcu_read_unlock(), 0));
  rcu_read_unlock();

  /* Is this safe? */
  local_irq_save(flags);
  vptr = this_cpu_ptr(&nrw_pcpu.nrx);
  memcpy(&rx, vptr, sizeof(rx));
  memset(vptr, 0, sizeof(rx));
  vptr = this_cpu_ptr(&nrw_pcpu.ntx);
  memcpy(&tx, vptr, sizeof(tx));
  memset(vptr, 0, sizeof(tx));
  local_irq_restore(flags);

  spin_lock(&pn->rx_sum_lock);
  sum_stats(&pn->rx_sum, &rx);
  spin_unlock(&pn->rx_sum_lock);
  spin_lock(&pn->tx_sum_lock);
  sum_stats(&pn->tx_sum, &tx);
  spin_unlock(&pn->tx_sum_lock);

  return 0;
}

static void collect_stat_start(struct work_struct *work)
{
  int i, enable;
  struct netrw_priv *pn = container_of(work,
                                       struct netrw_priv,
                                       collect_pcpu_stat.work);

  rcu_read_lock();
  enable = rcu_dereference(pn)->enable;
  rcu_read_unlock();

  if (enable) {
    /* Is this approach good?
     * 1. smp_call_on_cpu() inits/attaches a work to a specific cpu.
     *  - What if the callback is run on Ncpus simultaneously?
     *  - Would the performance get bad?
     *
     * open to discuss about this */
    for_each_possible_cpu(i) {
      /* this actual enqueues a work on a specific cpu */
      smp_call_on_cpu(i, collect_pcpu_stat_cb, pn, true);
    }
    schedule_delayed_work(&pn->collect_pcpu_stat, WQ_INTV_2SEC);
  }
}

#include "smsc-netrw-proc.c"

/* called before bottom-half init */
int
smsc_netrw_init(struct smsc95xx_priv *priv)
{
  struct netrw_priv *pdata;
  int ret = 0;

  retval_if_fail(priv, -EINVAL);

  pdata = vzalloc(sizeof(struct netrw_priv));
  if (!pdata) {
    ret = -ENOMEM;
    goto out;
  }

  pdata->root_dentry = proc_mkdir("smsc95xx", NULL);
  if (!pdata->root_dentry) {
    ret = -EPERM;
    goto free_pdata;
  }

  ret = netrw_proc_init(pdata);
  if (ret != 0) {
    goto rm_root_proc;
  }

  /* locks */
  spin_lock_init(&pdata->enable_lock);
  spin_lock_init(&pdata->rx_sum_lock);
  spin_lock_init(&pdata->tx_sum_lock);

  pdata->smsc_priv = priv;
  priv->netrw_priv = pdata;

  /* stat */
  INIT_DELAYED_WORK(&pdata->collect_pcpu_stat,
                    collect_stat_start);
  schedule_delayed_work(&pdata->collect_pcpu_stat,
                        WQ_INTV_2SEC);

  /* default is off */
  pdata->enable = 0;

out:
  return ret;

rm_root_proc:
  if (pdata->root_dentry)
    proc_remove(pdata->root_dentry);
free_pdata:
  vfree(pdata);

  goto out;
}

int
smsc_netrw_exit(struct smsc95xx_priv *priv)
{
  struct netrw_priv *pn;
  int ret = 0;

  retval_if_fail(priv, -EINVAL);

  pn = priv->netrw_priv;

  pn->enable = 0;
  /* Now netrw is turnt off */

  if (pn->root_dentry) {
    proc_remove(pn->root_dentry);
  }

  /* TODO what if yet nrx/ntx have to-do data? */
  /* stat */
  cancel_delayed_work_sync(&pn->collect_pcpu_stat);

  /* done */
  vfree(pn);

  return ret;
}
