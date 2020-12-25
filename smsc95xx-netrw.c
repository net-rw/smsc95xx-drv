/* smsc95xx-netrw.c
 *
 * Copyright (c) 2020 Leesoo Ahn <lsahn@ooseel.net>
 *
 * This software is distributed under the terms of the BSD or GPL license.
 */

#include <linux/proc_fs.h>

static struct proc_dir_entry *root_dentry;

int
smsc_netrw_init(void)
{
  int ret = -EINVAL;

  root_dentry = proc_mkdir("smsc95xx", NULL);
  if (!root_dentry) {
    goto out;
  }

  ret = 0;

out:
  return ret;
}

int
smsc_netrw_exit(void)
{
  int ret = 0;

  if (root_dentry)
    proc_remove(root_dentry);

  return ret;
}
