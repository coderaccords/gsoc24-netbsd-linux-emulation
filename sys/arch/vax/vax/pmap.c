/*      $NetBSD: pmap.c,v 1.5 1994/11/25 19:09:59 ragge Exp $     */

#define DEBUG
/*
 * Copyright (c) 1994 Ludd, University of Lule}, Sweden.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *     This product includes software developed at Ludd, University of Lule}.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

 /* All bugs are subject to removal without further notice */
		
#include "sys/types.h"
#include "sys/param.h"
#include "sys/queue.h"
#include "sys/malloc.h"
#include "sys/proc.h"
#include "sys/user.h"
#include "sys/msgbuf.h"
#include "vm/vm.h"
#include "vm/vm_page.h"
#include "vm/vm_kern.h"
#include "vax/include/pte.h"
#include "vax/include/pcb.h"
#include "vax/include/mtpr.h"
#include "vax/include/loconf.h"
#include "vax/include/macros.h"

#include "uba.h"

struct pte *pmap_virt2pte(pmap_t, u_int);

/**** Machine specific definitions ****/
#define VAX_PAGES_PER_PAGE PAGE_SIZE/NBPG


#define	VAXLOOP	{int vaxloop;for(vaxloop=0;vaxloop<	\
		VAX_PAGES_PER_PAGE;vaxloop++){
#define	VAXEND	}}


#define	PHYS_TO_PV(phys_page) (&pv_table[(phys_page>>PAGE_SHIFT)])

#define	PTE_TO_PV(pte)	(PHYS_TO_PV((pte&PG_FRAME)<<PG_SHIFT))



struct pmap kernel_pmap_store;
unsigned int gurkskit[50];
pmap_t kernel_pmap = &kernel_pmap_store;

static pv_entry_t alloc_pv_entry();
static void        free_pv_entry();

static int prot_array[]={ PG_NONE, PG_RO,   PG_RW,   PG_RW,
			  PG_RO,   PG_RO,   PG_RW,   PG_RW };
    

static pv_entry_t   pv_head =NULL;
static unsigned int pv_count=0;

extern uint etext;
extern uint v_cmap;
extern u_int *pte_cmap;
extern int  maxproc;
extern struct vmspace vmspace0;
uint*  UMEMmap;
void*  Numem;
void  *scratch;
uint   sigsida;
#ifdef DEBUG
int startpmapdebug=0;
extern int startsysc, faultdebug;
#endif
unsigned int *valueptr=gurkskit;
struct pte *Sysmap;
vm_map_t	pte_map;

vm_offset_t     avail_start, avail_end;
vm_offset_t   virtual_avail, virtual_end; /* Available virtual memory   */

/******************************************************************************
 *
 * pmap_bootstrap()
 *
 ******************************************************************************
 *
 * Called as part of vm bootstrap, allocates internal pmap structures.
 * Assumes that nothing is mapped, and that kernel stack is located
 * immediately after end.
 */

void 
pmap_bootstrap(pstart, pend)
	vm_offset_t pstart, pend;
{
	uint	istack,i;
	extern	u_int sigcode, esigcode, proc0paddr;
	struct pmap *p0pmap=&vmspace0.vm_pmap;
#define	ROUND_PAGE(x)	(((uint)(x)+PAGE_SIZE-1)& ~(PAGE_SIZE-1))

 /* These are in phys memory */
	istack=ROUND_PAGE((uint)Sysmap+SYSPTSIZE*4);
	(u_int)scratch=istack+ISTACK_SIZE;
	mtpr(scratch,PR_ISP); /* set interrupt stack pointer */
	mtpr(scratch,PR_SSP); /* put first setregs in scratch page */
	pv_table=(struct pv_entry *)(scratch+NBPG*4);

/* These are virt only */
	v_cmap=ROUND_PAGE(pv_table+(pend/PAGE_SIZE));
	(u_int)Numem=v_cmap+NBPG*2;

	(struct pte *)UMEMmap=kvtopte(Numem);
	(struct pte *)pte_cmap=kvtopte(v_cmap);

	avail_start=ROUND_PAGE(v_cmap)&0x7fffffff;
	avail_end=pend-ROUND_PAGE(sizeof(struct msgbuf));
	virtual_avail=ROUND_PAGE((uint)Numem+NUBA*NBPG*NBPG);
	virtual_end=SYSPTSIZE*NBPG+KERNBASE;
#ifdef DEBUG
	printf("Sysmap %x, istack %x, scratch %x\n",Sysmap,istack,scratch);
	printf("SYSPTSIZE %x, USRPTSIZE %x\n",SYSPTSIZE,USRPTSIZE);
	printf("pv_table %x, v_cmap %x, Numem %x, pte_cmap %x\n",
		pv_table,v_cmap,Numem,pte_cmap);
	printf("avail_start %x, avail_end %x\n",avail_start,avail_end);
	printf("virtual_avail %x,virtual_end %x\n",virtual_avail,virtual_end);
	printf("clearomr: %x \n",(uint)v_cmap-(uint)Sysmap);
	printf("faultdebug %x, startsysc %x\n",&faultdebug, &startsysc);
	printf("startpmapdebug %x\n",&startpmapdebug);
#endif

	bzero(Sysmap,(uint)v_cmap-(uint)Sysmap);
	pmap_map(0x80000000,0,2*NBPG,VM_PROT_READ|VM_PROT_WRITE);
	pmap_map(0x80000400,2*NBPG,(vm_offset_t)(&etext),VM_PROT_READ);
	pmap_map((vm_offset_t)(&etext),(vm_offset_t)&etext,
		(vm_offset_t)Sysmap,VM_PROT_READ|VM_PROT_WRITE);
	pmap_map((vm_offset_t)Sysmap,(vm_offset_t)Sysmap,istack,
		VM_PROT_READ|VM_PROT_WRITE);
	pmap_map(istack,istack,(vm_offset_t)scratch,VM_PROT_READ|VM_PROT_WRITE);
	pmap_map((vm_offset_t)scratch,(vm_offset_t)scratch,
		(vm_offset_t)pv_table,VM_PROT_READ|VM_PROT_WRITE);
	pmap_map((vm_offset_t)pv_table,(vm_offset_t)pv_table,v_cmap,
		VM_PROT_READ/*|VM_PROT_WRITE*/);

	/* Init kernel pmap */
	kernel_pmap->ref_count = 1;
	simple_lock_init(&kernel_pmap->pm_lock);
	p0pmap->pm_pcb=(struct pcb *)proc0paddr;


	sigsida=(u_int)scratch+NBPG; /* used for signal trampoline code */
	bcopy(&sigcode, sigsida, (u_int)&esigcode-(u_int)&sigcode);
	mtpr(proc0paddr+NBPG*2-0x800000,PR_P1BR);
	mtpr(0x200000-14,PR_P1LR);
	for(i=2;i<16;i++){
		*(struct pte *)(proc0paddr+NBPG*2-16*4+i*4)=
			Sysmap[((proc0paddr&0x7fffffff)>>PG_SHIFT)+i];
	}
	p0pmap->pm_pcb->P1BR=(void *)mfpr(PR_P1BR);
	p0pmap->pm_pcb->P1LR=mfpr(PR_P1LR);
/*
 * Now everything should be complete, start virtual memory.
 */
	mtpr((uint)Sysmap&0x7fffffff,PR_SBR); /* Where is SPT? */
	mtpr(SYSPTSIZE,PR_SLR);
	mtpr(1,PR_MAPEN);
	bzero(valueptr, 200);
}

/****************************************************************************** *
 * pmap_init()
 *
 ******************************************************************************
 *
 * Called as part of vm init.
 *
 */

void 
pmap_init(s, e) 
	vm_offset_t s,e;
{
	vm_offset_t start,end;

	/* reserve place on SPT for UPT * maxproc */
	pte_map=kmem_suballoc(kernel_map, &start, &end, 
		USRPTSIZE*4*maxproc, FALSE); /* Don't allow paging yet */
#ifdef DEBUG
if(startpmapdebug) printf("pmap_init: pte_map start %x, end %x, size %x\n",
		start, end, USRPTSIZE*4*maxproc);
#endif
}

/******************************************************************************
 *
 * pmap_create()
 *
 ******************************************************************************
 *
 * pmap_t pmap_create(phys_size)
 *
 * Create a pmap for a new task.
 * 
 * Allocate a pmap form kernel memory with malloc.
 * Clear the pmap.
 * Allocate a ptab for the pmap.
 * 
 */
pmap_t 
pmap_create(phys_size)
	vm_size_t phys_size;
{
	pmap_t   pmap;

#ifdef DEBUG
if(startpmapdebug)printf("pmap_create: phys_size %x\n",phys_size);
#endif
	if(phys_size) return NULL;

/* Malloc place for pmap struct */

	pmap = (pmap_t) malloc(sizeof(struct pmap), M_VMPMAP, M_WAITOK);
	pmap_pinit(pmap); 

	return (pmap);
}


/*
 * Release any resources held by the given physical map.
 * Called when a pmap initialized by pmap_pinit is being released.
 * Should only be called if the map contains no valid mappings.
 */
void
pmap_release(pmap)
	struct pmap *pmap;
{
#ifdef DEBUG
if(startpmapdebug)printf("pmap_release: pmap %x\n",pmap);
#endif

	if(pmap->pm_pcb->P0BR) kmem_free_wakeup(pte_map,
		(vm_offset_t)pmap->pm_pcb->P0BR, pmap->pm_pcb->P0LR*4);

	if(pmap->pm_pcb->P1BR) kmem_free_wakeup(pte_map,
		(vm_offset_t)pmap->pm_stack, (0x200000-pmap->pm_pcb->P1LR)*4);

	bzero(pmap, sizeof(struct pmap));
}


/******************************************************************************
 *
 * pmap_destroy()
 *
 ******************************************************************************
 *
 * pmap_destroy(pmap)
 *
 * Remove a reference from the pmap. 
 *
 * If the pmap is NULL then just return else decrese pm_count.
 *
 *  XXX pmap == NULL => "software only" pmap???
 *
 * If this was the last reference we call's pmap_relaese to release this pmap.
 *
 * OBS! remember to set pm_lock
 */

void
pmap_destroy(pmap)
	pmap_t pmap;
{
	int count;
  
#ifdef DEBUG
if(startpmapdebug)printf("pmap_destroy: pmap %x\n",pmap);
#endif
	if(pmap == NULL) return;

	simple_lock(&pmap->pm_lock);
	count = --pmap->ref_count;
	simple_unlock(&pmap->pm_lock);
  
	if (!count) {
		pmap_release(pmap);
		free((caddr_t)pmap, M_VMPMAP);
	}
}

void 
pmap_enter(pmap, v, p, prot, wired)
	register pmap_t pmap;
	vm_offset_t     v;
	vm_offset_t     p;
	vm_prot_t       prot;
	boolean_t       wired;
{
	int j,i,pte,s, *patch;
	pv_entry_t pv, tmp;

	pte=prot_array[prot]|PG_PFNUM(p)|PG_V;
	s=splimp();
	pv=PHYS_TO_PV(p);

#ifdef DEBUG
if(startpmapdebug)
printf("pmap_enter: pmap: %x,virt %x, phys %x,pv %x prot %x\n",
	pmap,v,p,pv,prot);
#endif

	if(!pmap) return;
	if(wired) pte |= PG_W;





	if(v<0x40000000){
		patch=(int *)pmap->pm_pcb->P0BR;
		i=(v>>PG_SHIFT);
		if(i>=pmap->pm_pcb->P0LR) pmap_expandp0(pmap);
		patch=(int *)pmap->pm_pcb->P0BR;
	} else if(v<(u_int)0x80000000){
		patch=(int *)pmap->pm_pcb->P1BR;
		i=(v-0x40000000)>>PG_SHIFT;
		if(i<mfpr(PR_P1LR)) asm("halt");
	} else {
		patch=(int *)Sysmap;
		i=(v-(u_int)0x80000000)>>PG_SHIFT;
	}

	if((patch[i]&PG_FRAME)==(pte&PG_FRAME)){ /* no map change */
		if((patch[i]&PG_W)!=(pte&PG_W)){ /* wiring change */
			pmap_change_wiring(pmap, v, wired);
		} else if((patch[i]&PG_PROT)!=(pte&PG_PROT)){
			VAXLOOP
				patch[i+vaxloop]&= ~PG_PROT; /* Protection */
				patch[i+vaxloop]|= prot_array[prot];
				mtpr(v+vaxloop*NBPG,PR_TBIS);
			VAXEND
		} else if((patch[i]&PG_V)==0) {
			VAXLOOP
				patch[i+vaxloop]|=PG_V;
				mtpr(v+vaxloop*NBPG,PR_TBIS);
			VAXEND
		} /* else nothing to do */
		return;
	}

	if(!pv->pv_pmap) {
unlock_pv_table(p);
		pv->pv_pmap=pmap;
		pv->pv_next=NULL;
		pv->pv_va=v;
lock_pv_table(p);
	} else {
		tmp=alloc_pv_entry();
		tmp->pv_pmap=pmap;
		tmp->pv_next=pv->pv_next;
		tmp->pv_va=v;
unlock_pv_table(p);
		pv->pv_next=tmp;
lock_pv_table(p);
	}
	VAXLOOP
		patch[i++]=pte++;
		mtpr(v,PR_TBIS);
		p+=NBPG;
	VAXEND
	mtpr(0,PR_TBIA);
	splx(s);
}

unlock_pv_table(phys)
	u_int phys;
{
	u_int i,*j;

	i=(u_int)(&pv_table[phys>>10]);
	j=(u_int*)(&Sysmap[(i&0x7fffffff)>>9]);
/* printf("unlock-pv: i %x, j %x\n",i,j); */
	*j&= ~0x78000000;
	*j|= prot_array[7];
}

lock_pv_table(phys)
	u_int phys;
{
	u_int i,*j;

	i=(u_int)(&pv_table[phys>>10]);
	j=(u_int*)(&Sysmap[(i&0x7fffffff)>>9]);
/* printf("lock-pv: i %x, j %x\n",i,j); */
	*j&= ~0x78000000;
	*j|= prot_array[5];
}

void *
pmap_bootstrap_alloc(size)
	int size;
{
	void *mem;

/* printf("pmap_bootstrap_alloc: size %x\n",size); */
        size = round_page(size);
        mem = (void *)virtual_avail;
        virtual_avail = pmap_map(virtual_avail, avail_start,
            avail_start + size, VM_PROT_READ|VM_PROT_WRITE);
        avail_start += size;
        bzero(mem, size);
        return (mem);
}

vm_offset_t
pmap_map(virtuell, pstart, pend, prot)
	vm_offset_t	virtuell, pstart, pend;
{
	vm_offset_t count;
	int *pentry;

#ifdef DEBUG
if(startpmapdebug)printf("pmap_map: virt %x, pstart %x, pend %x\n",virtuell, pstart, pend);
#endif

        pstart=(uint)pstart &0x7fffffff;
        pend=(uint)pend &0x7fffffff;
        virtuell=(uint)virtuell &0x7fffffff;
        (uint)pentry= (((uint)(virtuell)>>PGSHIFT)*4)+(uint)Sysmap;
        for(count=pstart;count<pend;count+=NBPG){
                *pentry++ = (count>>PGSHIFT)|prot_array[prot]|PG_V;
        }
	mtpr(0,PR_TBIA);
	return(virtuell+(count-pstart)+0x80000000);
}

vm_offset_t 
pmap_extract(pmap_t pmap, vm_offset_t va) {

	int	*pte, nypte;
#ifdef DEBUG
if(startpmapdebug)printf("pmap_extract: pmap %x, va %x\n",pmap, va);
#endif

	pte=(int *)pmap_virt2pte(pmap,va);
	if(pte) return(((*pte&PG_FRAME)<<PG_SHIFT)+((u_int)va&PG_FRAME));
	else return 0;
}

/*
 * pmap_protect( pmap, vstart, vend, protect)
 */
void
pmap_protect(pmap, start, end, prot)
	pmap_t pmap;
	vm_offset_t start;
	vm_offset_t     end;
	vm_prot_t       prot;
{
  int pte, *patch,s;

#ifdef DEBUG
if(startpmapdebug) printf("pmap_protect: pmap %x, start %x, end %x, prot %x\n",
	pmap, start, end,prot);
#endif
	if(pmap==NULL) return;
	s=splimp();
	pte=prot_array[prot];

	while (start < end) {
		patch = (int *)pmap_virt2pte(pmap,start);
		if(patch){
			*patch&=(~PG_PROT);
			*patch|=pte;
			mtpr(start,PR_TBIS);
		}
		start += NBPG;
	}
	mtpr(0,PR_TBIA);
	splx(s);
}

/*
 * pmap_remove(pmap, start, slut) removes all valid mappings between
 * the two virtual adresses start and slut from pmap pmap.
 * NOTE: all adresses between start and slut may not be mapped.
 */

void
pmap_remove(pmap, start, slut)
	pmap_t	pmap;
	vm_offset_t	start, slut;
{
	u_int *ptestart, *pteslut,i,s,*temp;
	pv_entry_t	pv;
	vm_offset_t	countup;

#ifdef DEBUG
if(startpmapdebug) printf("pmap_remove: pmap=0x %x, start=0x %x, slut=0x %x\n",
	   pmap, start, slut);
#endif

	if(!pmap) return;
	if(!pmap->pm_pcb) return; /* No page registers */

/* First, get pte first address */
	if(start<0x40000000){ /* P0 */
		if(!(temp=pmap->pm_pcb->P0BR)) return; /* No page table */
		ptestart=&temp[start>>PG_SHIFT];
		pteslut=&temp[slut>>PG_SHIFT];
		if(pteslut>&temp[pmap->pm_pcb->P0LR])
			pteslut=&temp[pmap->pm_pcb->P0LR];
if(startpmapdebug) printf("pmap->pm_pcb->P0BR %x,\n",pmap->pm_pcb->P0BR);
	} else if(start>0x7fffffff){ /* System region */
                ptestart=(u_int *)&Sysmap[(start&0x3fffffff)>>PG_SHIFT];
                pteslut=(u_int *)&Sysmap[(slut&0x3fffffff)>>PG_SHIFT];
	} else { /* P1 (stack) region */
		if(!(temp=pmap->pm_pcb->P1BR)) return; /* No page table */
		pteslut=&temp[(slut&0x3fffffff)>>PG_SHIFT];
		ptestart=&temp[(start&0x3fffffff)>>PG_SHIFT];
		if(ptestart<&temp[pmap->pm_pcb->P1LR])
			ptestart=&temp[pmap->pm_pcb->P1LR];
	}

#ifdef DEBUG
if(startpmapdebug)
printf("pmap_remove: ptestart %x, pteslut %x, pv %x\n",ptestart, pteslut,pv);
#endif

	s=splimp();
	for(countup=start;ptestart<pteslut;ptestart+=VAX_PAGES_PER_PAGE,
			countup+=PAGE_SIZE){

		if(!(*ptestart&PG_FRAME))
			continue; /* not valid phys addr,no mapping */

		pv=PTE_TO_PV(*ptestart);
if((*ptestart&PG_FRAME)>0x4000){
printf("utanf|r:\n");
printf("pmap_remove: ptestart %x, pteslut %x, pv %x\n",ptestart, pteslut,pv);
printf("pmap_remove: pmap=0x %x, start=0x %x, slut=0x %x\n",pmap, start, slut);
showstate(curproc);
asm("halt"); 
}

if(startpmapdebug) printf("ptestart %x, *ptestart %x, pv %x\n",
			ptestart,*ptestart,pv);
		if(!remove_pmap_from_mapping(pv,pmap)){
				printf("pmap_remove: pmap not in pv_table\n");
			printf("pv %x, ptestart %x, pmap %x, phys %x\n",
				pv, ptestart, pmap, (*ptestart&0xffff)<<9);
printf("pmap_remove: ptestart %x, pteslut %x, pv %x\n",ptestart, pteslut,pv);
printf("pmap_remove: pmap=0x %x, start=0x %x, slut=0x %x\n",pmap, start, slut);
			showstate(curproc);
			asm("halt");
			panic("pmap_remove: pmap not in pv_table");
		}

                *ptestart=0; /* XXX */
                *(ptestart+1)=0;
/*               mtpr(countup,PR_TBIS); */
        }
	mtpr(0,PR_TBIA);
        splx(s);
}


remove_pmap_from_mapping(pv, pmap)
	pv_entry_t pv;
	pmap_t	pmap;
{
	pv_entry_t temp_pv,temp2;

	if(!pv->pv_pmap&&pv->pv_next)
		halvpanik("J{ttefel i remove_pmap_from_mapping\n");

	if(pv->pv_pmap==pmap){
unlock_pv_table(((((u_int)pv-(u_int)pv_table)/16)<<10));
		if(pv->pv_next){
			temp_pv=pv->pv_next;
			pv->pv_pmap=temp_pv->pv_pmap;
			pv->pv_va=temp_pv->pv_va;
			pv->pv_next=temp_pv->pv_next;
			free_pv_entry(temp_pv);
		} else {
			bzero(pv,sizeof(struct pv_entry));
		}
lock_pv_table(((((u_int)pv-(u_int)pv_table)/16)<<10));
	} else {
		temp_pv=pv;
		while(temp_pv->pv_next){
			if(temp_pv->pv_next->pv_pmap==pmap){
				temp2=temp_pv->pv_next;
unlock_pv_table(((((u_int)pv-(u_int)pv_table)/16)<<10));
				temp_pv->pv_next=temp2->pv_next;
lock_pv_table(((((u_int)pv-(u_int)pv_table)/16)<<10));
				free_pv_entry(temp2);
				return 1;
			}
			temp_pv=temp_pv->pv_next;
		}
		return 0;
	}
	return 1;
}

void 
pmap_copy_page(src, dst)
	vm_offset_t   src;
	vm_offset_t   dst;
{
	int s;
	extern uint v_cmap;

#ifdef DEBUG
if(startpmapdebug)printf("pmap_copy_page: src %x, dst %x\n",src, dst);
#endif
	s=splimp();
	
	VAXLOOP
		pte_cmap[0]=((src>>PGSHIFT)+vaxloop)|PG_V|PG_RO;
		pte_cmap[1]=((dst>>PGSHIFT)+vaxloop)|PG_V|PG_KW;
		mtpr(v_cmap,PR_TBIS);
		mtpr(v_cmap+NBPG,PR_TBIS);
/*		asm("movc3   $512,_v_cmap,_v_cmap+512"); */
		bcopy(v_cmap, v_cmap+NBPG, NBPG); /* XXX slow... */
	VAXEND
		
	splx(s);
}

halvpanik(char *str){
	printf(str);
	asm("halt");
}

pv_entry_t 
alloc_pv_entry()
{
	pv_entry_t temporary;

	if(!pv_head) {
		temporary=(pv_entry_t)malloc(sizeof(struct pv_entry), M_VMPVENT, M_NOWAIT);
#ifdef DEBUG
if(startpmapdebug) printf("alloc_pv_entry: %x\n",temporary);
#endif
	} else {
		temporary=pv_head;
		pv_head=temporary->pv_next;
		pv_count--;
	}
	bzero(temporary, sizeof(struct pv_entry));
	return temporary;
}

void
free_pv_entry(entry)
	pv_entry_t entry;
{
	if(pv_count>=50) {         /* XXX Should be a define? */
		free(entry, M_VMPVENT);
	} else {
		entry->pv_next=pv_head;
		pv_head=entry;
		pv_count++;
	}
}

boolean_t
pmap_is_referenced(pa)
	vm_offset_t     pa;
{
	struct pv_entry *pv;
	int *pte;

	pv=PHYS_TO_PV(pa);
	if(!pv->pv_pmap) return 0;

	pte=(int *)pmap_virt2pte(pv->pv_pmap, pv->pv_va);
	return(*pte&PG_REF);
}

boolean_t
pmap_is_modified(pa)
     vm_offset_t     pa;
{
        struct pv_entry *pv;
        int *pte;

        pv=PHYS_TO_PV(pa);
        if(!pv->pv_pmap) return 0;

        pte=(int *)pmap_virt2pte(pv->pv_pmap, pv->pv_va);
        return(*pte&PG_M);
}


void 
pmap_clear_reference(pa)
	vm_offset_t     pa;
{
	struct pv_entry *pv;
	int *pte,s;
/*
 * Simulate page reference bit
 */
	pv=PHYS_TO_PV(pa);
#ifdef DEBUG
if(startpmapdebug) printf("pmap_clear_reference: pa %x, pv %x\n",pa,pv);
#endif

	if(!pv->pv_pmap) return;

	do {
		pte=(int *)pmap_virt2pte(pv->pv_pmap, pv->pv_va);
		s=splimp();
		*pte&= ~(PG_REF|PG_V);
		*pte|=PG_SREF;
		splx(s);
	} while(pv=pv->pv_next);
	mtpr(0,PR_TBIA);
}

void 
pmap_clear_modify(pa)
	vm_offset_t     pa;
{
	struct pv_entry *pv;
	int *pte;

	pv=PHYS_TO_PV(pa);
	if(!pv->pv_pmap) return;
	do {
		pte=(int *)pmap_virt2pte(pv->pv_pmap, pv->pv_va);
		*pte&= ~(PG_M);
	} while(pv=pv->pv_next);
}

void 
pmap_change_wiring(pmap, va, wired)
        register pmap_t pmap;
        vm_offset_t     va;
        boolean_t       wired;
{
	int *pte;
#ifdef DEBUG
if(startpmapdebug) printf("pmap_change_wiring: pmap %x, va %x, wired %x\n",
	pmap, va, wired);
#endif

	pte=(int *)pmap_virt2pte(pmap,va);
	if(!pte) return; /* no pte allocated */
	if(wired) *pte|=PG_W;
	else *pte&=~PG_W;
}

/*
 *      pmap_page_protect:
 *
 *      Lower the permission for all mappings to a given page.
 */
void
pmap_page_protect(pa, prot)
	vm_offset_t     pa;
	vm_prot_t       prot;
{
	pv_entry_t pv,opv;
	int s,*pte,nyprot;
  
#ifdef DEBUG
if(startpmapdebug) printf("pmap_page_protect: pa %x, prot %x\n",pa, prot);
#endif
	pv = PHYS_TO_PV(pa);
	if(!pv->pv_pmap) return;
	nyprot=prot_array[prot];

	switch (prot) {

	case VM_PROT_ALL:
		break;
	case VM_PROT_READ:
	case VM_PROT_READ|VM_PROT_EXECUTE:
		do {
			pte=(int *)pmap_virt2pte(pv->pv_pmap, pv->pv_va);
			s=splimp();
			VAXLOOP
				*(pte+vaxloop)&=~PG_PROT;
				*(pte+vaxloop)|=nyprot;
			VAXEND
			splx(s);
		} while(pv=pv->pv_next);
		break;

	default:

		pte=(int *)pmap_virt2pte(pv->pv_pmap, pv->pv_va);
		s = splimp();
		VAXLOOP
			*(pte+vaxloop)=0;
		VAXEND
		opv=pv;
		pv=pv->pv_next;
unlock_pv_table(pa);
		bzero(opv,sizeof(struct pv_entry));
lock_pv_table(pa);
		while(pv){
			pte=(int *)pmap_virt2pte(pv->pv_pmap, pv->pv_va);
			VAXLOOP
				*(pte+vaxloop)=0;
			VAXEND
			opv=pv;
			pv=pv->pv_next;
			free_pv_entry(opv);
		}

		mtpr(0,PR_TBIA);
		splx(s);
		break;
	}
}

/*
 *      pmap_zero_page zeros the specified (machine independent)
 *      page by mapping the page into virtual memory and using
 *      bzero to clear its contents, one machine dependent page
 *      at a time.
 */
void
pmap_zero_page(phys)
	vm_offset_t    phys;
{
	int s;

#ifdef DEBUG
if(startpmapdebug)printf("pmap_zero_page(phys %x, v_cmap %x, pte_cmap %x\n",
	phys,v_cmap,pte_cmap);
#endif
	s=splimp();
	VAXLOOP
		*pte_cmap=((phys+(NBPG*vaxloop))>>PG_SHIFT)|PG_V|PG_KW;
		mtpr(v_cmap,PR_TBIA);
		clearpage(v_cmap);
	VAXEND
	*pte_cmap=0;
	mtpr(v_cmap,PR_TBIA);
	splx(s);
}

struct pte *
pmap_virt2pte(pmap,vaddr)
	pmap_t	pmap;
	u_int	vaddr;
{
	u_int *pte,scr;

if(0) printf("pmap_virt2pte: pmap %x, vaddr %x\n",pmap,vaddr);

	if(vaddr<0x40000000){
		pte=pmap->pm_pcb->P0BR;
		if((vaddr>>PG_SHIFT)>pmap->pm_pcb->P0LR) return 0;
	} else if(vaddr<(u_int)0x80000000){
		pte=pmap->pm_pcb->P1BR;
		if(((vaddr&0x3fffffff)>>PG_SHIFT)<pmap->pm_pcb->P1LR) return 0;
	} else {
		pte=(u_int *)Sysmap;
	}

	vaddr&=(u_int)0x3fffffff;

if(0) printf("pmap_virt2pte2: vaddr %x, pte %x, sbr %x\n",
		vaddr, &pte[vaddr>>PG_SHIFT], mfpr(PR_SBR));
	return((struct pte *)&pte[vaddr>>PG_SHIFT]);
}

pmap_expandp0(pmap)
	struct pmap *pmap;
{
        u_int tmp,s,size,osize,oaddr;

if(startpmapdebug)printf("pmap_expandp0: pmap %x, size %x, pmap->pcb %x\n",
		pmap,mfpr(PR_P0LR), pmap->pm_pcb);
if(!pmap)asm("halt");
	osize=pmap->pm_pcb->P0LR*4;
	size=osize+PAGE_SIZE;
if(startpmapdebug)printf("F|re kmem_alloc_wait %x\n",size);
	tmp=kmem_alloc_wait(pte_map, size);
if(startpmapdebug)printf("Efter kmem_alloc_wait %x\n",tmp);
	bzero(tmp,size); /* Sanity */
	if(osize) bcopy(pmap->pm_pcb->P0BR, tmp,osize);
	oaddr=(u_int)pmap->pm_pcb->P0BR;
	s=splhigh();
	mtpr(tmp,PR_P0BR);
	mtpr(size>>2,PR_P0LR);
	pmap->pm_pcb->P0BR=(void*)tmp;
	pmap->pm_pcb->P0LR=mfpr(PR_P0LR);
	splx(s);
	if(osize) kmem_free_wakeup(pte_map, (vm_offset_t)oaddr, osize);
}

pmap_expandp1(pmap)
        struct pmap *pmap;
{
        u_int tmp,s,size,osize,oaddr;

if(startpmapdebug)printf("pmap_expandp1: pmap %x, size %x\n",
		pmap,mfpr(PR_P1LR));
if(!pmap)asm("halt");
        osize=0x800000-(pmap->pm_pcb->P1LR*4);
        size=osize+PAGE_SIZE;
        tmp=kmem_alloc_wait(pte_map, size);
        bzero(tmp,size); /* Sanity */
        if(osize) bcopy(pmap->pm_stack, tmp,osize);
        oaddr=pmap->pm_stack;
        s=splhigh();
	pmap->pm_pcb->P1BR=(void*)(tmp+size-0x800000);
	pmap->pm_stack=tmp;
        mtpr(pmap->pm_pcb->P1BR,PR_P1BR);
        mtpr((0x800000-size)>>2,PR_P1LR);
        pmap->pm_pcb->P1LR=mfpr(PR_P1LR);
        splx(s);
        if(osize) kmem_free_wakeup(pte_map, (vm_offset_t)oaddr, osize);
}
