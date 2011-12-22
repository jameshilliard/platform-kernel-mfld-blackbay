/*
 * Copyright (c) 2011, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,·
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Authors:
 * Ander Conselvan de Oliveira <ander.conselvan.de.oliveira@intel.com>
 * Pauli Nieminen <pauli.nieminen@intel.com>
 *
 */

#include <linux/slab.h>
#include <linux/list.h>
#include <linux/spinlock.h>

#include "ossync.h"
#include "servicesint.h"

DEFINE_SPINLOCK(sync_lock);
LIST_HEAD(sync_list);

typedef void (*callback_t)(void *);

struct pending_sync {
	PVRSRV_KERNEL_SYNC_INFO *sync_info;
	u32 pending_read_ops;
	u32 pending_write_ops;
	unsigned int flags;
	callback_t callback;
	void *user_data;

	struct list_head list;
};

#define ops_after(a, b) ((s32)(b) - (s32)(a) < 0)

static bool pending_ops_completed(PVRSRV_KERNEL_SYNC_INFO *sync_info,
				  unsigned int flags,
				  u32 pending_read_ops,
				  u32 pending_write_ops)
{
	if (flags & PVRSRV_SYNC_READ &&
	    ops_after(pending_read_ops, sync_info->psSyncData->ui32ReadOpsComplete))
		return false;

	if (flags & PVRSRV_SYNC_WRITE &&
	    ops_after(pending_write_ops, sync_info->psSyncData->ui32WriteOpsComplete))
		return false;

	return true;
}

/* Returns 0 if the callback was successfully registered.
 * Returns a negative value on error.
 */
int
PVRSRVCallbackOnSync(PVRSRV_KERNEL_SYNC_INFO *sync_info,
		     unsigned int flags,
                     callback_t callback, void *user_data)
{
	struct pending_sync *pending_sync;
	unsigned long lock_flags;
	u32 pending_read_ops = sync_info->psSyncData->ui32ReadOpsPending;
	u32 pending_write_ops = sync_info->psSyncData->ui32WriteOpsPending;

	/* If the object is already in sync, don't add it to the list */
	if (pending_ops_completed(sync_info, flags,
				  pending_read_ops,
				  pending_write_ops)) {
		callback(user_data);
		return 0;
	}

	pending_sync = kmalloc(sizeof *pending_sync, GFP_KERNEL);
	if (!pending_sync)
		return -ENOMEM;

	pending_sync->sync_info = sync_info;
	pending_sync->pending_read_ops = pending_read_ops;
	pending_sync->pending_write_ops = pending_write_ops;
	pending_sync->flags = flags;
	pending_sync->callback = callback;
	pending_sync->user_data = user_data;

	spin_lock_irqsave(&sync_lock, lock_flags);
	list_add_tail(&pending_sync->list, &sync_list);
	spin_unlock_irqrestore(&sync_lock, lock_flags);

	return 0;
}

void
PVRSRVCheckPendingSyncs()
{
	struct pending_sync *ps, *tmp;
	unsigned long flags;

	spin_lock_irqsave(&sync_lock, flags);

	list_for_each_entry_safe(ps, tmp, &sync_list, list) {
		if (pending_ops_completed(ps->sync_info, ps->flags,
					  ps->pending_read_ops,
					  ps->pending_write_ops)) {
			ps->callback(ps->user_data);
			list_del(&ps->list);
			kfree(ps);
		}
	}

	spin_unlock_irqrestore(&sync_lock, flags);
}

