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
	int pending_read_ops;
	int pending_write_ops;
	int flags;
	callback_t callback;
	void *user_data;

	struct list_head list;
};

/* Returns 0 if the callback was successfully registered.
 * Returns a negative value on error.
 */
int
PVRSRVCallbackOnSync(PVRSRV_KERNEL_SYNC_INFO *sync_info, int flags,
                     callback_t callback, void *user_data)
{
	struct pending_sync *pending_sync;
	unsigned long lock_flags;

	/* If the object is already in sync, don't add it to the list */
	if ((!(flags & PVRSRV_SYNC_READ)
	     || (sync_info->psSyncData->ui32ReadOpsPending
		 <= sync_info->psSyncData->ui32ReadOpsComplete))
	    && (!(flags & PVRSRV_SYNC_WRITE)
		|| (sync_info->psSyncData->ui32WriteOpsPending
		    <= sync_info->psSyncData->ui32WriteOpsComplete))) {
		callback(user_data);
		return 0;
	}

	pending_sync = kmalloc(sizeof *pending_sync, GFP_KERNEL);
	if (!pending_sync)
		return -ENOMEM;

	pending_sync->sync_info = sync_info;
	pending_sync->pending_read_ops = sync_info->psSyncData->ui32ReadOpsPending;
	pending_sync->pending_write_ops = sync_info->psSyncData->ui32WriteOpsPending;
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
	int read_ops_done, write_ops_done;
	int sync_read, sync_write;
	unsigned long flags;

	spin_lock_irqsave(&sync_lock, flags);

	list_for_each_entry_safe(ps, tmp, &sync_list, list) {
		sync_read = (ps->flags & PVRSRV_SYNC_READ) != 0;
		read_ops_done = ps->sync_info->psSyncData->ui32ReadOpsComplete;

		sync_write = (ps->flags & PVRSRV_SYNC_WRITE) != 0;
		write_ops_done = ps->sync_info->psSyncData->ui32WriteOpsComplete;

		if ((!sync_read || ps->pending_read_ops <= read_ops_done)
		    && (!sync_write || ps->pending_write_ops <= write_ops_done)) {
			ps->callback(ps->user_data);
			list_del(&ps->list);
			kfree(ps);
		}
	}

	spin_unlock_irqrestore(&sync_lock, flags);
}

