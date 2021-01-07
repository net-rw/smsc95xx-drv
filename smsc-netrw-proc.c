/* smsc-netrw-proc.c
 *
 * Copyright (c) 2021 Leesoo Ahn <lsahn@ooseel.net>
 *
 * This software is distributed under the terms of the BSD or GPL license.
 */

/* enable read/write */
#define LEN_USRBUF 32
static ssize_t
enable_proc_write(struct file *file,
                  const char __user *user_buf,
                  size_t sz,
                  loff_t *ppos)
{
  struct netrw_priv *pn;
  u8 usr_buf[LEN_USRBUF];
  int newv, oldv;
  char dummy;

  if (sz > LEN_USRBUF)
    return -EINVAL;

  if (copy_from_user(usr_buf, user_buf, LEN_USRBUF) != 0)
    return -EFAULT;
  usr_buf[LEN_USRBUF-1] = 0;

  if (sscanf(usr_buf, "%d%c", &newv, &dummy) != 2)
    return -EINVAL;
  newv = !!newv;

  pn = PDE_DATA(file_inode(file));
  /* Would it be safe without NULL check? */

  /* critical section */
  spin_lock(&pn->enable_lock);
  retval_if_fail(newv != pn->enable,
                 (spin_unlock(&pn->enable_lock), sz));
  pn->enable = newv;
  spin_unlock(&pn->enable_lock);

  if (newv)
    schedule_delayed_work(&pn->collect_pcpu_stat, WQ_INTV_2SEC);
  else
    cancel_delayed_work_sync(&pn->collect_pcpu_stat);

  return sz;
}

static int
enable_proc_show(struct seq_file *s, void *unused)
{
  struct netrw_priv *pn;
  int enable;

  rcu_read_lock();
  enable = rcu_dereference(s->private)->enable;
  rcu_read_unlock();

  seq_printf(s, "%d\n", enable);

  return 0;
}

static int enable_proc_open(struct inode *inode,
                            struct file *file)
{
  return single_open(file, enable_proc_show, PDE_DATA(inode));
}

static const struct file_operations enable_ops = {
  .owner    = THIS_MODULE,
  .open     = enable_proc_open,
  .write    = enable_proc_write,
  .read     = seq_read,
  .llseek   = seq_lseek,
  .release  = single_release,
};

/* stats show */
static int
stats_show(struct seq_file *s, void *unused)
{
  struct drv_stat rx, tx;
  struct netrw_priv *pn;

  rcu_read_lock();
  pn = rcu_dereference(s->private);
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
netrw_proc_init(struct netrw_priv *priv)
{
  struct proc_dir_entry *dentry;
  int ret = 0;

  retval_if_fail(priv, -EINVAL);
  retval_if_fail(priv->root_dentry, -EPERM);

  dentry = priv->root_dentry;

  if (!proc_create_data("enable", 0644, dentry,
                        &enable_ops, priv)) {
    ret = -EPERM;
    goto out;
  }

  if (!proc_create_single_data("stats", 0, dentry,
                               stats_show, priv)) {
    ret = -EPERM;
    goto out;
  }

out:
  return ret;
}

/* XXX We mightn't need exit routine for them.
 * Cause root would take the responsibility. */
