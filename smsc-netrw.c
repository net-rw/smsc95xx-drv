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


struct netrw_priv {
  int enable;
  spinlock_t enable_lock;

  /* stat */
  struct drv_stat rx_sum;
  spinlock_t rx_sum_lock;
  struct drv_stat tx_sum;
  spinlock_t tx_sum_lock;

  struct delayed_work collect_cpu_stat;
};
static struct netrw_priv *netrw_data;

/* For only race conditional & time critical data */
struct netrw_pcpu {
  struct drv_stat nrx;
  struct drv_stat ntx;
};
DEFINE_PER_CPU(struct netrw_pcpu, nrw_pcpu);


static struct proc_dir_entry *root_dentry;

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
netrw_skb_rx_hook(struct sk_buff *skb)
{
  struct netrw_priv *pn;

  rcu_read_lock();
  pn = rcu_dereference(netrw_data);
  retval_if_fail(pn && pn->enable,
                 (rcu_read_unlock(), 0));
  rcu_read_unlock();

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
netrw_skb_tx_hook(struct sk_buff *skb)
{
  struct netrw_priv *pn;

  rcu_read_lock();
  pn = rcu_dereference(netrw_data);
  retval_if_fail(pn && pn->enable,
                 (rcu_read_unlock(), 0));
  rcu_read_unlock();

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

static int collect_pcpu_stat_cb(void *vptr)
{
  struct drv_stat rx, tx;
  struct netrw_priv *pn;

  vptr = this_cpu_ptr(&nrw_pcpu.nrx);
  memcpy(&rx, vptr, sizeof(rx));
  memset(vptr, 0, sizeof(rx));
  vptr = this_cpu_ptr(&nrw_pcpu.ntx);
  memcpy(&tx, vptr, sizeof(tx));
  memset(vptr, 0, sizeof(tx));

  rcu_read_lock();
  pn = rcu_dereference(netrw_data);
  retval_if_fail(pn, (rcu_read_unlock(), 0));
  /* XXX is this safe? */
  spin_lock(&pn->rx_sum_lock);
  sum_stats(&pn->rx_sum, &rx);
  spin_unlock(&pn->rx_sum_lock);
  spin_lock(&pn->tx_sum_lock);
  sum_stats(&pn->tx_sum, &tx);
  spin_unlock(&pn->tx_sum_lock);
  rcu_read_unlock();

  return 0;
}

static void collect_stat_start(struct work_struct *unused)
{
  int i;
  struct netrw_priv *pn;

  for_each_possible_cpu(i) {
    /* this actual enqueues a work on a specific cpu */
    smp_call_on_cpu(i, collect_pcpu_stat_cb, NULL, true);
  }

  rcu_read_lock();
  pn = rcu_dereference(netrw_data);
  retcall_if_fail(pn && pn->enable,
                  rcu_read_unlock());
  schedule_delayed_work(&pn->collect_cpu_stat, WQ_INTV_2SEC);
  rcu_read_unlock();
}

static int
stat_show(struct seq_file *s, void *unused)
{
  struct drv_stat rx, tx;
  struct netrw_priv *pn;

  rcu_read_lock();
  pn = rcu_dereference(netrw_data);
  retval_if_fail(pn, (rcu_read_unlock(), 0));
  memcpy(&rx, &pn->rx_sum, sizeof(rx));
  memcpy(&tx, &pn->tx_sum, sizeof(tx));
  rcu_read_unlock();

  /* tx */
  seq_printf(s, ".-* Transmit\n"
                "|  ARP packets  : (%u)\n"
                "|\n"
                "|  L3 Unicasts  : (%llu)\n"
                "|  L3 Broadcasts: (%llu)\n"
                "|  L3 Multicasts: (%llu)\n"
                ":\n"
                "'\n\n",
                tx.l2_arp,
                tx.l3_unicst,
                tx.l3_brdcst,
                tx.l3_mltcst);

  /* rx */
  seq_printf(s, ".-* Receive\n"
                "|  ARP packets  : (%u)\n"
                "|\n"
                "|  L3 Unicasts  : (%llu)\n"
                "|  L3 Broadcasts: (%llu)\n"
                "|  L3 Multicasts: (%llu)\n"
                ":\n"
                "'\n",
                rx.l2_arp,
                rx.l3_unicst,
                rx.l3_brdcst,
                rx.l3_mltcst);

  return 0;
}

static int
stat_open(struct inode *inode, struct file *file)
{
  return single_open(file, stat_show, NULL);
}

static struct file_operations stat_seq_fops = {
  .owner = THIS_MODULE,
  .open = stat_open,
  .read = seq_read,
  .llseek = seq_lseek,
  .release = seq_release,
};

/* called before bottom-half init */
int
smsc_netrw_init(void)
{
  int ret = 0;

  root_dentry = proc_mkdir("smsc95xx", NULL);
  if (!root_dentry) {
    ret = -EPERM;
    goto out;
  }

  if (!proc_create("stats", 0, root_dentry,
                   &stat_seq_fops)) {
    ret = -EPERM;
    goto rm_root_proc;
  }

  netrw_data = kzalloc(sizeof(struct netrw_priv),
                       GFP_KERNEL);
  if (!netrw_data) {
    ret = -ENOMEM;
    goto out;
  }

  /* locks */
  spin_lock_init(&netrw_data->enable_lock);
  spin_lock_init(&netrw_data->rx_sum_lock);
  spin_lock_init(&netrw_data->tx_sum_lock);

  /* stat */
  INIT_DELAYED_WORK(&netrw_data->collect_cpu_stat,
                    collect_stat_start);
	schedule_delayed_work(&netrw_data->collect_cpu_stat,
                        WQ_INTV_2SEC);

  /* ready */
  spin_lock(&netrw_data->enable_lock);
  rcu_dereference(netrw_data)->enable = 1;
  spin_unlock(&netrw_data->enable_lock);

out:
  return ret;

rm_root_proc:
  if (root_dentry)
    proc_remove(root_dentry);
  goto out;
}

int
smsc_netrw_exit(void)
{
  int ret = 0;

  spin_lock(&netrw_data->enable_lock);
  rcu_dereference(netrw_data)->enable = 0;
  spin_unlock(&netrw_data->enable_lock);
  /* Now netrw is turnt off */

  if (root_dentry) {
    proc_remove(root_dentry);
  }

  /* TODO what if yet nrx/ntx have to-do data? */
  if (netrw_data) {
    /* stat */
    cancel_delayed_work_sync(&netrw_data->collect_cpu_stat);

    kfree(netrw_data);
  }

  netrw_data = NULL;

  return ret;
}
