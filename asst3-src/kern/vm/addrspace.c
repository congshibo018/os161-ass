/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *        The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <spl.h>
#include <spinlock.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>
#include <proc.h>

/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 *
 * UNSW: If you use ASST3 config as required, then this file forms
 * part of the VM subsystem.
 *
 */

struct addrspace *
as_create(void)
{
        struct addrspace *as;

        as = kmalloc(sizeof(struct addrspace));
        if (as == NULL) {
                return NULL;
        }

        /*
         * Initialize as needed.
         */
        as->region_list = (struct regionlist*)kmalloc(sizeof(struct regionlist*));
        if(as->region_list == NULL){
                return NULL;
        }

        as->region_list->head = NULL;
        as->region_list->tail = NULL;
        as->stack_base = USERSTACK - (VMSTACKSIZE * PAGE_SIZE); // fixed stack size;
        as->stack_top =  USERSTACK;
        as->heap_base = as->heap_top =  0;
        return as;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
        struct addrspace *newas;

        newas = as_create();
        if (newas==NULL) {
                return ENOMEM;
        }

        /*
         * Write this.
         */

        (void)old;

        struct as_region *i, *new;

        i = old->region_list->head;
        new = newas->region_list->head;

        while(i != NULL){
                if(new == NULL){
                        new = (struct as_region*) kmalloc(sizeof(struct as_region));
                }  
                new->vbase = i->vbase;
                new->pbase = i->pbase;
                new->npages = i->npages;
                new->permissions = i->permissions;
		new->prepermissions = i->prepermissions;
                if(i->next != NULL){
                        new->next = (struct as_region*) kmalloc(sizeof(struct as_region));
                        new = new->next;
                        i = i->next;
                }
                else{
                        break;
                }
        }

        //TODO: allocate and copy frames.

        *ret = newas;
        return 0;
}

void
as_destroy(struct addrspace *as)
{
        /*
         * Clean up as needed.
         */
        struct as_region *i,*cur;
	i = as->region_list->head;
	/* free region */
	while(i != NULL){
	        cur = i->next;
	        kfree(i);
		i = cur;
	}
        kfree(as);
}

void
as_activate(void)
{
        struct addrspace *as;

        as = proc_getas();
        if (as == NULL) {
                /*
                 * Kernel thread without an address space; leave the
                 * prior address space in place.
                 */
                return;
        }

        /*
         * Write this.
         */

        int spl = splhigh();

        for(int i=0; i<NUM_TLB; i++){
                tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
        }

        splx(spl);
}

void
as_deactivate(void)
{
        /*
         * Write this. For many designs it won't need to actually do
         * anything. See proc.c for an explanation of why it (might)
         * be needed.
         */

        int spl = splhigh();

        for(int i=0; i<NUM_TLB; i++){
                tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
        }

        splx(spl);
}

/*
 * Set up a segment at virtual address VADDR of size MEMSIZE. The
 * segment in memory extends from VADDR up to (but not including)
 * VADDR+MEMSIZE.
 *
 * The READABLE, WRITEABLE, and EXECUTABLE flags are set if read,
 * write, or execute permission should be set on the segment. At the
 * moment, these are ignored. When you write the VM system, you may
 * want to implement them.
 */
int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t memsize,
                 int readable, int writeable, int executable)
{
        /*
         * Write this.
         */

        struct as_region* new = NULL;
        if(as->region_list->head != NULL && as->region_list->tail != NULL){
                new = as->region_list->tail;
                new -> next = (struct as_region*) kmalloc(sizeof(struct as_region));
                new = new->next;
                new->next = NULL;
                new->prev = as->region_list->tail;
                as->region_list->tail = new;
        }
        else {
              new = (struct as_region*) kmalloc(sizeof(struct as_region));
              new->next = new->prev = NULL;
              as->region_list->head = new;
              as->region_list->tail = new;
        }

        vaddr &= PAGE_FRAME;

        // round up the memsize to the next multiple of page_size
        memsize = (memsize + PAGE_SIZE - 1) & PAGE_FRAME;
        size_t sz = memsize / PAGE_SIZE;

        new -> vbase = vaddr;
        new -> pbase = 0;                            
        new -> npages = sz;
        new -> permissions = (readable|writeable|executable);   // need to verify the value
	new -> prepermissions = (readable|writeable|executable);
        return 0; 
}

int
as_prepare_load(struct addrspace *as)
{
        /*
         * Write this.
         */

        as ->region_list-> ignore_permissions = true;
        struct as_region *temp = as->region_list->head;
	while(temp->next != NULL){
	        temp -> permissions = temp -> permissions | 0x2;
		temp = temp->next;
	}

        return 0;
}

int
as_complete_load(struct addrspace *as)
{
        /*
         * Write this.
         */

        as ->region_list -> ignore_permissions = false;
        struct as_region *temp = as->region_list->head;
	while(temp->next !=NULL){
	        temp -> permissions = temp -> prepermissions;
		temp = temp -> next;
	}

        return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
        /*
         * Write this.
         */

        (void)as;

        /* Initial user-level stack pointer */
        *stackptr = as->stack_top;

        return 0;
}

