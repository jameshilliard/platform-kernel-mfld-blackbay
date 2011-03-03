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

#ifndef __IMG_LINUX_MM_H__
#define __IMG_LINUX_MM_H__

#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/list.h>
#include <linux/types.h>

#include <asm/io.h>

#define	PHYS_TO_PFN(phys) ((phys) >> PAGE_SHIFT)
#define PFN_TO_PHYS(pfn) ((pfn) << PAGE_SHIFT)

#define RANGE_TO_PAGES(range) (((range) + (PAGE_SIZE - 1)) >> PAGE_SHIFT)

#define	ADDR_TO_PAGE_OFFSET(addr) (((unsigned long)(addr)) & (PAGE_SIZE - 1))

static inline u32 VMallocToPhys(void *pCpuVAddr)
{
	return (page_to_phys(vmalloc_to_page(pCpuVAddr)) + ADDR_TO_PAGE_OFFSET(pCpuVAddr));
		
}

typedef enum {
    LINUX_MEM_AREA_IOREMAP,
	LINUX_MEM_AREA_EXTERNAL_KV,
    LINUX_MEM_AREA_IO,
    LINUX_MEM_AREA_VMALLOC,
    LINUX_MEM_AREA_ALLOC_PAGES,
    LINUX_MEM_AREA_SUB_ALLOC,
    LINUX_MEM_AREA_TYPE_COUNT
}LINUX_MEM_AREA_TYPE;

typedef struct _LinuxMemArea LinuxMemArea;


struct _LinuxMemArea {
    LINUX_MEM_AREA_TYPE eAreaType;
    union _uData
    {
        struct _sIORemap
        {
            
            resource_size_t CPUPhysAddr;
            void *pvIORemapCookie;
        }sIORemap;
        struct _sExternalKV
        {
            
	    IMG_BOOL bPhysContig;
	    union {
		    
		    IMG_SYS_PHYADDR SysPhysAddr;
		    IMG_SYS_PHYADDR *pSysPhysAddr;
	    } uPhysAddr;
            void *pvExternalKV;
        }sExternalKV;
        struct _sIO
        {
            resource_size_t CPUPhysAddr;
        }sIO;
        struct _sVmalloc
        {
            
            void *pvVmallocAddress;
        }sVmalloc;
        struct _sPageList
        {
            
            struct page **pvPageList;
	    IMG_HANDLE hBlockPageList;
        }sPageList;
        struct _sSubAlloc
        {
            
            LinuxMemArea *psParentLinuxMemArea;
            u32 ui32ByteOffset;
        }sSubAlloc;
    }uData;

    u32 ui32ByteSize;		

    u32 ui32AreaFlags;		

    IMG_BOOL bMMapRegistered;		

    
    struct list_head	sMMapItem;

    
    struct list_head	sMMapOffsetStructList;
};

int linux_mm_init(void);


void linux_mm_cleanup(void);

void *vmalloc_wrapper(u32 bytes, u32 alloc_flags);

LinuxMemArea *vmalloc_linux_mem_area(u32 bytes, u32 area_flags);


void *ioremap_wrapper(resource_size_t address, u32 bytes, u32 mapping_flags);


LinuxMemArea *ioremap_linux_mem_area(resource_size_t address, u32 bytes, u32 area_flags);


LinuxMemArea *NewExternalKVLinuxMemArea(IMG_SYS_PHYADDR *pBasePAddr, void *pvCPUVAddr, u32 ui32Bytes, IMG_BOOL bPhysContig, u32 ui32AreaFlags);


struct page *LinuxMemAreaOffsetToPage(LinuxMemArea *psLinuxMemArea, u32 ui32ByteOffset);


LinuxMemArea *NewIOLinuxMemArea(resource_size_t address, u32 ui32Bytes, u32 ui32AreaFlags);


LinuxMemArea *alloc_pages_linux_mem_area(u32 ui32Bytes, u32 ui32AreaFlags);


void free_pages_linux_mem_area(LinuxMemArea *psLinuxMemArea);


LinuxMemArea *NewSubLinuxMemArea(LinuxMemArea *psParentLinuxMemArea,
                                 u32 ui32ByteOffset,
                                 u32 ui32Bytes);


void LinuxMemAreaDeepFree(LinuxMemArea *psLinuxMemArea);


void *LinuxMemAreaToCpuVAddr(LinuxMemArea *psLinuxMemArea);


resource_size_t LinuxMemAreaToCpuPAddr(LinuxMemArea *mem_area, u32 offset);


#define	 LinuxMemAreaToCpuPFN(psLinuxMemArea, ui32ByteOffset) PHYS_TO_PFN(LinuxMemAreaToCpuPAddr(psLinuxMemArea, ui32ByteOffset))

IMG_BOOL LinuxMemAreaPhysIsContig(LinuxMemArea *psLinuxMemArea);

static inline LinuxMemArea *
LinuxMemAreaRoot(LinuxMemArea *psLinuxMemArea)
{
    if(psLinuxMemArea->eAreaType == LINUX_MEM_AREA_SUB_ALLOC)
    {
        return psLinuxMemArea->uData.sSubAlloc.psParentLinuxMemArea;
    }
    else
    {
        return psLinuxMemArea;
    }
}


static inline LINUX_MEM_AREA_TYPE
LinuxMemAreaRootType(LinuxMemArea *psLinuxMemArea)
{
    return LinuxMemAreaRoot(psLinuxMemArea)->eAreaType;
}


const IMG_CHAR *LinuxMemAreaTypeToString(LINUX_MEM_AREA_TYPE eMemAreaType);


#if defined(DEBUG)
const IMG_CHAR *HAPFlagsToString(u32 ui32Flags);
#endif

#endif 

