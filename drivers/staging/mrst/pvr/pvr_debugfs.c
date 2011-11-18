/*
 * Copyright (C) 2011 Nokia Corporation
 * Copyright (C) 2011 Intel Corporation
 * Author: Luc Verhaegen <libv@codethink.co.uk>
 * Author: Imre Deak <imre.deak@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/vmalloc.h>
#include <linux/mutex.h>

#include "img_types.h"
#include "servicesext.h"
#include "services.h"
#include "syscommon.h"
#include "pvr_bridge_km.h"
#include "sgx_bridge_km.h"
#include "sgxutils.h"
#include "pvr_debugfs.h"
#include "mmu.h"
#include "bridged_support.h"
#include "mm.h"
#include "pvr_trace_cmd.h"
#include "pvr_debug_core.h"

static struct dentry *pvr_debugfs_dir;
static u32 reset_sgx;

static int pvr_debugfs_reset_sgx(void)
{
	PVRSRV_DEVICE_NODE *dev_node;

	dev_node = pvr_get_sgx_dev_node();
	if (!dev_node)
		return -ENODEV;

	return sgx_trigger_reset(dev_node);
}

static int pvr_debugfs_reset_sgx_wrapper(void *data, u64 val)
{
	u32 *var = data;

	if (var == &reset_sgx) {
		int r = -EINVAL;

		if (val == 1)
			r = pvr_debugfs_reset_sgx();
		return r;
	}

	BUG();
}

DEFINE_SIMPLE_ATTRIBUTE(pvr_debugfs_reset_sgx_fops, NULL,
			pvr_debugfs_reset_sgx_wrapper, "%llu\n");


#ifdef CONFIG_PVR_TRACE_CMD

static void *trcmd_str_buf;
static u8 *trcmd_snapshot;
static size_t trcmd_snapshot_size;
static unsigned long trcmd_busy;

static int pvr_dbg_trcmd_open(struct inode *inode, struct file *file)
{
	int r;

	if (test_and_set_bit(0, &trcmd_busy))
		return -EBUSY;

	trcmd_str_buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!trcmd_str_buf) {
		clear_bit(0, &trcmd_busy);

		return -ENOMEM;
	}

	r = pvr_trcmd_create_snapshot(&trcmd_snapshot, &trcmd_snapshot_size);
	if (r < 0) {
		kfree(trcmd_str_buf);
		clear_bit(0, &trcmd_busy);

		return r;
	}

	return 0;
}

static int pvr_dbg_trcmd_release(struct inode *inode, struct file *file)
{
	pvr_trcmd_destroy_snapshot(trcmd_snapshot);
	kfree(trcmd_str_buf);
	clear_bit(0, &trcmd_busy);

	return 0;
}

static ssize_t pvr_dbg_trcmd_read(struct file *file, char __user *buffer,
				  size_t count, loff_t *ppos)
{
	ssize_t ret;

	ret = pvr_trcmd_print(trcmd_str_buf, max_t(size_t, PAGE_SIZE, count),
			      trcmd_snapshot, trcmd_snapshot_size, ppos);
	if (copy_to_user(buffer, trcmd_str_buf, ret))
		return -EFAULT;

	return ret;
}

static const struct file_operations pvr_dbg_trcmd_fops = {
	.owner		= THIS_MODULE,
	.open		= pvr_dbg_trcmd_open,
	.release	= pvr_dbg_trcmd_release,
	.read		= pvr_dbg_trcmd_read,
};
#endif

static struct sgx_fw_state *fw_state;
static unsigned long fw_state_busy;

static int pvr_dbg_fw_state_open(struct inode *inode, struct file *file)
{
	PVRSRV_DEVICE_NODE *dev_node;
	int r = 0;

	dev_node = pvr_get_sgx_dev_node();
	if (!dev_node)
		return -ENODEV;

	if (test_and_set_bit(0, &fw_state_busy))
		return -EBUSY;

	fw_state = vmalloc(sizeof(*fw_state));
	if (!fw_state) {
		clear_bit(0, &fw_state_busy);

		return -ENOMEM;
	}

	r = sgx_save_fw_state(dev_node, fw_state);
	if (r < 0) {
		vfree(fw_state);
		clear_bit(0, &fw_state_busy);

		return r;
	}

	return 0;
}

static int pvr_dbg_fw_state_release(struct inode *inode, struct file *file)
{
	vfree(fw_state);
	clear_bit(0, &fw_state_busy);

	return 0;
}

static ssize_t pvr_dbg_fw_state_read(struct file *file, char __user *buffer,
				  size_t count, loff_t *ppos)
{
	char rec[48];
	ssize_t ret;

	if (*ppos >= ARRAY_SIZE(fw_state->trace) + 1)
		return 0;

	if (!*ppos)
		ret = sgx_print_fw_status_code(rec, sizeof(rec),
						fw_state->status_code);
	else
		ret = sgx_print_fw_trace_rec(rec, sizeof(rec), fw_state,
						*ppos - 1);
	(*ppos)++;

	if (copy_to_user(buffer, rec, ret))
		return -EFAULT;

	return ret;
}

static const struct file_operations pvr_dbg_fw_state_fops = {
	.owner		= THIS_MODULE,
	.open		= pvr_dbg_fw_state_open,
	.release	= pvr_dbg_fw_state_release,
	.read		= pvr_dbg_fw_state_read,
};

int pvr_debugfs_init(void)
{
	pvr_debugfs_dir = debugfs_create_dir("pvr", NULL);
	if (!pvr_debugfs_dir)
		goto err;

	if (!debugfs_create_file("reset_sgx", S_IWUSR, pvr_debugfs_dir,
				 &reset_sgx, &pvr_debugfs_reset_sgx_fops))
		goto err;

#ifdef CONFIG_PVR_TRACE_CMD
	if (!debugfs_create_file("command_trace", S_IRUGO, pvr_debugfs_dir,
				NULL, &pvr_dbg_trcmd_fops))
		goto err;
#endif
	if (!debugfs_create_file("firmware_trace", S_IRUGO, pvr_debugfs_dir,
				NULL, &pvr_dbg_fw_state_fops))
		goto err;

	return 0;
err:
	debugfs_remove_recursive(pvr_debugfs_dir);
	pr_err("pvr: debugfs init failed\n");

	return -ENODEV;
}

void pvr_debugfs_cleanup(void)
{
	debugfs_remove_recursive(pvr_debugfs_dir);
}

