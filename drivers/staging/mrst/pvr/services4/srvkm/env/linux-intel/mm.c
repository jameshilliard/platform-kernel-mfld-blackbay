/**********************************************************************
 *
 * Copyright(c) 2008 Imagination Technologies Ltd. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful but, except
 * as otherwise stated in writing, without any warranty; without even the
 * implied warranty of merchantability or fitness for a particular purpose.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * The full GNU General Public License is included in this distribution in
 * the file called "COPYING".
 *
 * Contact Information:
 * Imagination Technologies Ltd. <gpl-support@imgtec.com>
 * Home Park Estate, Kings Langley, Herts, WD4 8LZ, UK
 *
 ******************************************************************************/

#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <asm/io.h>
#include <linux/slab.h>
#include <linux/highmem.h>
#include <linux/sched.h>
#include <linux/mutex.h>

#include "img_defs.h"
#include "services.h"
#include "servicesint.h"
#include "syscommon.h"
#include "mutils.h"
#include "mm.h"
#include "pvrmmap.h"
#include "mmap.h"
#include "osfunc.h"
#include "pvr_debug.h"
#include "proc.h"
#include "lock.h"


static struct kmem_cache *linux_mem_area_cache;

int linux_mm_init(void)
{
	linux_mem_area_cache = kmem_cache_create("img-mm", sizeof(LinuxMemArea), 0, 0, NULL);

	if (!linux_mem_area_cache)
	{
		pr_err("%s: failed to allocate kmem_cache", __FUNCTION__);
		return -ENOMEM;
	}
	return 0;
}

void linux_mm_cleanup(void)
{
	if (linux_mem_area_cache)
	{
		kmem_cache_destroy(linux_mem_area_cache);
		linux_mem_area_cache = NULL;
	}
}

void *vmalloc_wrapper(u32 bytes, u32 alloc_flags)
{
	/*
	 * FIXME: This function creates a memory alias
	 * of a page, with a mismatching PAT type.
	 * This wants to be fixed most likely.
	 */
	pgprot_t pgprot_flags;

	switch(alloc_flags & PVRSRV_HAP_CACHETYPE_MASK)
	{
	case PVRSRV_HAP_CACHED:
		pgprot_flags = PAGE_KERNEL;
		break;
	case PVRSRV_HAP_WRITECOMBINE:
		pgprot_flags = PGPROT_WC(PAGE_KERNEL);
		break;
	case PVRSRV_HAP_UNCACHED:
		pgprot_flags = PGPROT_UC(PAGE_KERNEL);
		break;
	default:
		WARN(1, "unknown mapping flags=0x%08x", alloc_flags);
		return NULL;
	}


	return __vmalloc(bytes, GFP_KERNEL | __GFP_HIGHMEM, pgprot_flags);
}

LinuxMemArea *vmalloc_linux_mem_area(u32 bytes, u32 area_flags)
{
	LinuxMemArea *mem_area;
	void *vptr;

	mem_area = kmem_cache_alloc(linux_mem_area_cache, GFP_KERNEL);

	if (!mem_area)
		goto failed;

	vptr = vmalloc_wrapper(bytes, area_flags);
	if (!vptr)
		goto failed;

	mem_area->eAreaType = LINUX_MEM_AREA_VMALLOC;
	mem_area->uData.sVmalloc.pvVmallocAddress = vptr;
	mem_area->ui32ByteSize = bytes;
	mem_area->ui32AreaFlags = area_flags;
	mem_area->bMMapRegistered = IMG_FALSE;
	INIT_LIST_HEAD(&mem_area->sMMapOffsetStructList);

	return mem_area;

failed:
	pr_err("%s: failed!", __FUNCTION__);
	if (mem_area)
		kmem_cache_free(linux_mem_area_cache, mem_area);
	return NULL;
}


void __iomem *ioremap_wrapper(resource_size_t address,
               u32 bytes, u32 mapping_flags)
{
	void __iomem *cookie;

	switch(mapping_flags & PVRSRV_HAP_CACHETYPE_MASK)
	{
        case PVRSRV_HAP_CACHED:
		cookie = ioremap_cache(address, bytes);
		break;
	case PVRSRV_HAP_WRITECOMBINE:
		cookie = ioremap_wc(address, bytes);
		break;
	case PVRSRV_HAP_UNCACHED:
		cookie = ioremap_nocache(address, bytes);
		break;
        default:
		pr_err("ioremap_wrapper: unknown mapping flags");
		return NULL;
	}

	return cookie;
}

LinuxMemArea *
ioremap_linux_mem_area(resource_size_t address, u32 bytes, u32 area_flags)
{
	LinuxMemArea *mem_area;
	void __iomem *cookie;

	mem_area = kmem_cache_alloc(linux_mem_area_cache, GFP_KERNEL);
	if (!mem_area)
		return NULL;

	cookie = ioremap_wrapper(address, bytes, area_flags);
	if (!cookie)
	{
		kmem_cache_free(linux_mem_area_cache, mem_area);
		return NULL;
	}

	mem_area->eAreaType = LINUX_MEM_AREA_IOREMAP;
	mem_area->uData.sIORemap.pvIORemapCookie = cookie;
	mem_area->uData.sIORemap.CPUPhysAddr = address;
	mem_area->ui32ByteSize = bytes;
	mem_area->ui32AreaFlags = area_flags;
	mem_area->bMMapRegistered = IMG_FALSE;
	INIT_LIST_HEAD(&mem_area->sMMapOffsetStructList);

	return mem_area;
}


static IMG_BOOL
TreatExternalPagesAsContiguous(IMG_SYS_PHYADDR *psSysPhysAddr, u32 ui32Bytes, IMG_BOOL bPhysContig)
{
	u32 ui32;
	u32 ui32AddrChk;
	u32 ui32NumPages = RANGE_TO_PAGES(ui32Bytes);

	for (ui32 = 0, ui32AddrChk = psSysPhysAddr[0].uiAddr;
		ui32 < ui32NumPages;
		ui32++, ui32AddrChk = (bPhysContig) ? (ui32AddrChk + PAGE_SIZE) : psSysPhysAddr[ui32].uiAddr)
	{
		if (!pfn_valid(PHYS_TO_PFN(ui32AddrChk)))
		{
			break;
		}
	}
	if (ui32 == ui32NumPages)
	{
		return IMG_FALSE;
	}

	if (!bPhysContig)
	{
		for (ui32 = 0, ui32AddrChk = psSysPhysAddr[0].uiAddr;
			ui32 < ui32NumPages;
			ui32++, ui32AddrChk += PAGE_SIZE)
		{
			if (psSysPhysAddr[ui32].uiAddr != ui32AddrChk)
			{
				return IMG_FALSE;
			}
		}
	}

	return IMG_TRUE;
}


LinuxMemArea *NewExternalKVLinuxMemArea(IMG_SYS_PHYADDR *pBasePAddr, void *pvCPUVAddr, u32 bytes, IMG_BOOL bPhysContig, u32 area_flags)
{
	LinuxMemArea *mem_area;

	mem_area = kmem_cache_alloc(linux_mem_area_cache, GFP_KERNEL);

	if (!mem_area)
		return NULL;

	mem_area->eAreaType = LINUX_MEM_AREA_EXTERNAL_KV;
	mem_area->uData.sExternalKV.pvExternalKV = pvCPUVAddr;
	mem_area->uData.sExternalKV.bPhysContig = (IMG_BOOL)(bPhysContig || TreatExternalPagesAsContiguous(pBasePAddr, bytes, bPhysContig));

	if (mem_area->uData.sExternalKV.bPhysContig)
		mem_area->uData.sExternalKV.uPhysAddr.SysPhysAddr = *pBasePAddr;
	else
		mem_area->uData.sExternalKV.uPhysAddr.pSysPhysAddr = pBasePAddr;

	mem_area->ui32ByteSize = bytes;
	mem_area->ui32AreaFlags = area_flags;
	mem_area->bMMapRegistered = IMG_FALSE;
	INIT_LIST_HEAD(&mem_area->sMMapOffsetStructList);

	return mem_area;
}

LinuxMemArea *
NewIOLinuxMemArea(resource_size_t address, u32 bytes, u32 area_flags)
{
	LinuxMemArea *mem_area;

	mem_area  = kmem_cache_alloc(linux_mem_area_cache, GFP_KERNEL);

	if (!mem_area)
		return NULL;

	mem_area->eAreaType = LINUX_MEM_AREA_IO;
	mem_area->uData.sIO.CPUPhysAddr = address;
	mem_area->ui32ByteSize = bytes;
	mem_area->ui32AreaFlags = area_flags;
	mem_area->bMMapRegistered = IMG_FALSE;
	INIT_LIST_HEAD(&mem_area->sMMapOffsetStructList);

	return mem_area;
}


LinuxMemArea * alloc_pages_linux_mem_area(u32 bytes, u32 area_flags)
{
	LinuxMemArea *mem_area;
	int page_count;
	struct page **page_list;
	IMG_HANDLE hBlockPageList;
	int i;
	PVRSRV_ERROR eError;

	mem_area = kmem_cache_alloc(linux_mem_area_cache, GFP_KERNEL);
	if (!mem_area)
		goto failed_area_alloc;


	page_count =  RANGE_TO_PAGES(bytes);

	eError = OSAllocMem(0, sizeof(*page_list) * page_count, (void **)&page_list, &hBlockPageList,
							"Array of pages");

	if(eError != PVRSRV_OK)
		goto failed_page_list_alloc;

	for(i=0; i<page_count; i++)
	{
		page_list[i] = alloc_pages(GFP_KERNEL | __GFP_HIGHMEM, 0);
		if(!page_list[i])
			goto failed_alloc_pages;
	}

	mem_area->eAreaType = LINUX_MEM_AREA_ALLOC_PAGES;
	mem_area->uData.sPageList.pvPageList = page_list;
	mem_area->uData.sPageList.hBlockPageList = hBlockPageList;
	mem_area->ui32ByteSize = bytes;
	mem_area->ui32AreaFlags = area_flags;
	mem_area->bMMapRegistered = IMG_FALSE;
	INIT_LIST_HEAD(&mem_area->sMMapOffsetStructList);

	return mem_area;

failed_alloc_pages:
	for(i--; i >= 0; i--)
		__free_pages(page_list[i], 0);

	OSFreeMem(0, sizeof(*page_list) * page_count, page_list, hBlockPageList);
	mem_area->uData.sPageList.pvPageList = NULL;
failed_page_list_alloc:
	kmem_cache_free(linux_mem_area_cache, mem_area);
failed_area_alloc:
	pr_debug("%s: failed", __FUNCTION__);

	return NULL;
}


void free_pages_linux_mem_area(LinuxMemArea *mem_area)
{
	u32 page_count;
	struct page **page_list;
	IMG_HANDLE hBlockPageList;
	int i;

	BUG_ON(!mem_area);


	page_count = RANGE_TO_PAGES(mem_area->ui32ByteSize);
	page_list = mem_area->uData.sPageList.pvPageList;
	hBlockPageList = mem_area->uData.sPageList.hBlockPageList;

	for(i = 0; i < page_count; i++)
		__free_pages(page_list[i], 0);


	OSFreeMem(0, sizeof(*page_list) * page_count, page_list, hBlockPageList);
	mem_area->uData.sPageList.pvPageList = NULL;
}


struct page* LinuxMemAreaOffsetToPage(LinuxMemArea *mem_area, u32 offset)
{
	u32 page_index;
	u8 *addr;

	switch(mem_area->eAreaType)
	{
        case LINUX_MEM_AREA_ALLOC_PAGES:
		page_index = PHYS_TO_PFN(offset);
		return mem_area->uData.sPageList.pvPageList[page_index];

        case LINUX_MEM_AREA_VMALLOC:
		addr = mem_area->uData.sVmalloc.pvVmallocAddress;
		addr += offset;
		return vmalloc_to_page(addr);

        case LINUX_MEM_AREA_SUB_ALLOC:
		return LinuxMemAreaOffsetToPage(mem_area->uData.sSubAlloc.psParentLinuxMemArea,
                                            mem_area->uData.sSubAlloc.ui32ByteOffset
                                             + offset);
        default:
		pr_err("%s: Unsupported request for struct page from LinuxMemArea with type=%s",
                    __FUNCTION__, LinuxMemAreaTypeToString(mem_area->eAreaType));
		return NULL;
	}
}


LinuxMemArea *NewSubLinuxMemArea(LinuxMemArea *parent, u32 offset, u32 bytes)
{
	LinuxMemArea *mem_area;

	BUG_ON(offset+bytes > parent->ui32ByteSize);

	mem_area = kmem_cache_alloc(linux_mem_area_cache, GFP_KERNEL);
	if (!mem_area)
		return NULL;

	mem_area->eAreaType = LINUX_MEM_AREA_SUB_ALLOC;
	mem_area->uData.sSubAlloc.psParentLinuxMemArea = parent;
	mem_area->uData.sSubAlloc.ui32ByteOffset = offset;
	mem_area->ui32ByteSize = bytes;
	mem_area->ui32AreaFlags = parent->ui32AreaFlags;
	mem_area->bMMapRegistered = IMG_FALSE;
	INIT_LIST_HEAD(&mem_area->sMMapOffsetStructList);

	return mem_area;
}


void LinuxMemAreaDeepFree(LinuxMemArea *mem_area)
{
	/* FIXME: call vfree and co direct, and free the mem area centrally at the end */
	switch(mem_area->eAreaType)
	{
        case LINUX_MEM_AREA_VMALLOC:
		vfree(mem_area->uData.sVmalloc.pvVmallocAddress);
		break;
	case LINUX_MEM_AREA_ALLOC_PAGES:
		free_pages_linux_mem_area(mem_area);
		break;
	case LINUX_MEM_AREA_IOREMAP:
		iounmap(mem_area->uData.sIORemap.pvIORemapCookie);
		break;
	case LINUX_MEM_AREA_EXTERNAL_KV:
	case LINUX_MEM_AREA_IO:
	case LINUX_MEM_AREA_SUB_ALLOC:
		break;
        default:
		pr_debug("%s: Unknown are type (%d)\n",
                     __FUNCTION__, mem_area->eAreaType);
		break;
	}
	kmem_cache_free(linux_mem_area_cache, mem_area);
}


void * LinuxMemAreaToCpuVAddr(LinuxMemArea *mem_area)
{
	switch(mem_area->eAreaType)
	{
        case LINUX_MEM_AREA_VMALLOC:
		return mem_area->uData.sVmalloc.pvVmallocAddress;
        case LINUX_MEM_AREA_IOREMAP:
		return mem_area->uData.sIORemap.pvIORemapCookie;
	case LINUX_MEM_AREA_EXTERNAL_KV:
		return mem_area->uData.sExternalKV.pvExternalKV;
        case LINUX_MEM_AREA_SUB_ALLOC:
        {
		char *addr = LinuxMemAreaToCpuVAddr(mem_area->uData.sSubAlloc.psParentLinuxMemArea);
		if (!addr)
			return NULL;
		return addr + mem_area->uData.sSubAlloc.ui32ByteOffset;
        }
        default:
		return NULL;
	}
}


resource_size_t LinuxMemAreaToCpuPAddr(LinuxMemArea *mem_area, u32 offset)
{
	resource_size_t address;

	address = 0;

	switch(mem_area->eAreaType)
	{
        case LINUX_MEM_AREA_IOREMAP:
        {
		address = mem_area->uData.sIORemap.CPUPhysAddr;
		address += offset;
		break;
        }
	case LINUX_MEM_AREA_EXTERNAL_KV:
	{
		if (mem_area->uData.sExternalKV.bPhysContig)
		{
			IMG_CPU_PHYADDR CpuPAddr = SysSysPAddrToCpuPAddr(mem_area->uData.sExternalKV.uPhysAddr.SysPhysAddr);
			address = CpuPAddr.uiAddr + offset;
		} else {
			u32 page_index = PHYS_TO_PFN(offset);
			IMG_SYS_PHYADDR SysPAddr = mem_area->uData.sExternalKV.uPhysAddr.pSysPhysAddr[page_index];
			IMG_CPU_PHYADDR CpuPAddr = SysSysPAddrToCpuPAddr(SysPAddr);
			address = CpuPAddr.uiAddr + ADDR_TO_PAGE_OFFSET(offset);
		}
		break;
	}
        case LINUX_MEM_AREA_IO:
        {
		address = mem_area->uData.sIO.CPUPhysAddr;
		address += offset;
		break;
        }
        case LINUX_MEM_AREA_VMALLOC:
        {
		char *vaddr;
		vaddr = mem_area->uData.sVmalloc.pvVmallocAddress;
		vaddr += offset;
		address = VMallocToPhys(vaddr);
		break;
        }
        case LINUX_MEM_AREA_ALLOC_PAGES:
        {
		struct page *page;
		u32 page_index = PHYS_TO_PFN(offset);
		page = mem_area->uData.sPageList.pvPageList[page_index];
		address = page_to_phys(page);
		address += ADDR_TO_PAGE_OFFSET(offset);
		break;
        }
        case LINUX_MEM_AREA_SUB_ALLOC:
        {
		IMG_CPU_PHYADDR  CpuPAddr =
		    OSMemHandleToCpuPAddr(mem_area->uData.sSubAlloc.psParentLinuxMemArea,
                                      mem_area->uData.sSubAlloc.ui32ByteOffset
                                        + offset);

		address = CpuPAddr.uiAddr;
		break;
        }
        default:
		pr_debug("%s: Unknown LinuxMemArea type (%d)\n",
                     __FUNCTION__, mem_area->eAreaType);
		break;
	}

	BUG_ON(!address);
	return address;
}


IMG_BOOL LinuxMemAreaPhysIsContig(LinuxMemArea *mem_area)
{
	switch(mem_area->eAreaType)
	{
	case LINUX_MEM_AREA_IOREMAP:
	case LINUX_MEM_AREA_IO:
		return IMG_TRUE;

	case LINUX_MEM_AREA_EXTERNAL_KV:
		return mem_area->uData.sExternalKV.bPhysContig;

        case LINUX_MEM_AREA_VMALLOC:
        case LINUX_MEM_AREA_ALLOC_PAGES:
		return IMG_FALSE;

        case LINUX_MEM_AREA_SUB_ALLOC:

		return LinuxMemAreaPhysIsContig(mem_area->uData.sSubAlloc.psParentLinuxMemArea);

        default:
		pr_debug("%s: Unknown LinuxMemArea type (%d)\n",
                     __FUNCTION__, mem_area->eAreaType);
		break;
	}
	return IMG_FALSE;
}


const char *
LinuxMemAreaTypeToString(LINUX_MEM_AREA_TYPE eMemAreaType)
{

	switch(eMemAreaType)
	{
        case LINUX_MEM_AREA_IOREMAP:
		return "LINUX_MEM_AREA_IOREMAP";
	case LINUX_MEM_AREA_EXTERNAL_KV:
		return "LINUX_MEM_AREA_EXTERNAL_KV";
        case LINUX_MEM_AREA_IO:
		return "LINUX_MEM_AREA_IO";
        case LINUX_MEM_AREA_VMALLOC:
		return "LINUX_MEM_AREA_VMALLOC";
        case LINUX_MEM_AREA_SUB_ALLOC:
		return "LINUX_MEM_AREA_SUB_ALLOC";
        case LINUX_MEM_AREA_ALLOC_PAGES:
		return "LINUX_MEM_AREA_ALLOC_PAGES";
        default:
		BUG();
	}

	return "";
}
