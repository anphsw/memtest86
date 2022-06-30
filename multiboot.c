/* multiboot.c
 *
 * Copyright (C) 2008,2009 Robert Millan
 *
 * Loosely based on linuxbios.c, which is:
 *
 * Released under version 2 of the Gnu Public License.
 * By Eric Biederman
 */

#include "multiboot.h"
#include "test.h"

extern struct multiboot_info *mbiptr;

int query_multiboot(void)
{
	struct multiboot_mmap_entry *mem;
	int i;

	if (!mbiptr) {
		return 0;
	}
	if (!mbiptr->mmap_addr) {
		return 1;
	}
	if ((mbiptr->flags & MULTIBOOT_INFO_MEM_MAP) == 0) {
		return 1;
	}

	mem = (struct multiboot_mmap_entry *) mbiptr->mmap_addr;
	mem_info.e820_nr = 0;

	for (i = 0; i < E820MAX; i++) {
		if ((multiboot_uint32_t) mem >= (mbiptr->mmap_addr + mbiptr->mmap_length)) {
			break;
		}

		mem_info.e820[mem_info.e820_nr].addr = mem->addr;
		mem_info.e820[mem_info.e820_nr].size = mem->len;
		/* Multiboot spec defines available / reserved types to match with E820.  */
		mem_info.e820[mem_info.e820_nr].type = mem->type;
		mem_info.e820_nr++;

		mem = (struct multiboot_mmap_entry *) ((multiboot_uint32_t) mem + mem->size + sizeof (mem->size));
	}

	return 1;
}
