#include "param.h"
#include "types.h"
#include "defs.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "elf.h"

extern char data[];  // defined by kernel.ld
pde_t *kpgdir;  // for use in scheduler()

// Set up CPU's kernel segment descriptors.
// Run once on entry on each CPU.
void
seginit(void)
{
  struct cpu *c;

  // Map "logical" addresses to virtual addresses using identity map.
  // Cannot share a CODE descriptor for both kernel and user
  // because it would have to have DPL_USR, but the CPU forbids
  // an interrupt from CPL=0 to DPL=3.
  c = &cpus[cpuid()];
  c->gdt[SEG_KCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, 0);
  c->gdt[SEG_KDATA] = SEG(STA_W, 0, 0xffffffff, 0);
  c->gdt[SEG_UCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, DPL_USER);
  c->gdt[SEG_UDATA] = SEG(STA_W, 0, 0xffffffff, DPL_USER);
  lgdt(c->gdt, sizeof(c->gdt));
}

// Return the address of the PTE in page table pgdir
// that corresponds to virtual address va.  If alloc!=0,
// create any required page table pages.
static pte_t *
walkpgdir(pde_t *pgdir, const void *va, int alloc)
{
  pde_t *pde;
  pte_t *pgtab;

  pde = &pgdir[PDX(va)];
  if(*pde & PTE_P){
    pgtab = (pte_t*)P2V(PTE_ADDR(*pde));
  } else {
    if(!alloc || (pgtab = (pte_t*)kalloc()) == 0)
      return 0;
    // Make sure all those PTE_P bits are zero.
    memset(pgtab, 0, PGSIZE);
    // The permissions here are overly generous, but they can
    // be further restricted by the permissions in the page table
    // entries, if necessary.
    *pde = V2P(pgtab) | PTE_P | PTE_W | PTE_U;
  }
  return &pgtab[PTX(va)];
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not
// be page-aligned.
static int
mappages(pde_t *pgdir, void *va, uint size, uint pa, int perm)
{
  char *a, *last;
  pte_t *pte;

  a = (char*)PGROUNDDOWN((uint)va);
  last = (char*)PGROUNDDOWN(((uint)va) + size - 1);
  for(;;){
    if((pte = walkpgdir(pgdir, a, 1)) == 0)
      return -1;
    if(*pte & PTE_P){
      cprintf("REMAP PANIC FROM MAPPAGES\n");
      panic("remap");
    }

    *pte = pa | perm | PTE_P;
    if(a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

// There is one page table per process, plus one that's used when
// a CPU is not running any process (kpgdir). The kernel uses the
// current process's page table during system calls and interrupts;
// page protection bits prevent user code from using the kernel's
// mappings.
//
// setupkvm() and exec() set up every page table like this:
//
//   0..KERNBASE: user memory (text+data+stack+heap), mapped to
//                phys memory allocated by the kernel
//   KERNBASE..KERNBASE+EXTMEM: mapped to 0..EXTMEM (for I/O space)
//   KERNBASE+EXTMEM..data: mapped to EXTMEM..V2P(data)
//                for the kernel's instructions and r/o data
//   data..KERNBASE+PHYSTOP: mapped to V2P(data)..PHYSTOP,
//                                  rw data + free physical memory
//   0xfe000000..0: mapped direct (devices such as ioapic)
//
// The kernel allocates physical memory for its heap and for user memory
// between V2P(end) and the end of physical memory (PHYSTOP)
// (directly addressable from end..P2V(PHYSTOP)).

// This table defines the kernel's mappings, which are present in
// every process's page table.
static struct kmap {
  void *virt;
  uint phys_start;
  uint phys_end;
  int perm;
} kmap[] = {
 { (void*)KERNBASE, 0,             EXTMEM,    PTE_W}, // I/O space
 { (void*)KERNLINK, V2P(KERNLINK), V2P(data), 0},     // kern text+rodata
 { (void*)data,     V2P(data),     PHYSTOP,   PTE_W}, // kern data+memory
 { (void*)DEVSPACE, DEVSPACE,      0,         PTE_W}, // more devices
};

// Set up kernel part of a page table.
pde_t*
setupkvm(void)
{
  pde_t *pgdir;
  struct kmap *k;

  if((pgdir = (pde_t*)kalloc()) == 0)
    return 0;
  memset(pgdir, 0, PGSIZE);
  if (P2V(PHYSTOP) > (void*)DEVSPACE)
    panic("PHYSTOP too high");
  for(k = kmap; k < &kmap[NELEM(kmap)]; k++)
    if(mappages(pgdir, k->virt, k->phys_end - k->phys_start,
                (uint)k->phys_start, k->perm) < 0) {
      freevm(pgdir);
      return 0;
    }
  return pgdir;
}

// Allocate one page table for the machine for the kernel address
// space for scheduler processes.
void
kvmalloc(void)
{
  kpgdir = setupkvm();
  switchkvm();
}

// Switch h/w page table register to the kernel-only page table,
// for when no process is running.
void
switchkvm(void)
{
  lcr3(V2P(kpgdir));   // switch to the kernel page table
}

// Switch TSS and h/w page table to correspond to process p.
void
switchuvm(struct proc *p)
{
  if(p == 0)
    panic("switchuvm: no process");
  if(p->kstack == 0)
    panic("switchuvm: no kstack");
  if(p->pgdir == 0)
    panic("switchuvm: no pgdir");

  pushcli();
  mycpu()->gdt[SEG_TSS] = SEG16(STS_T32A, &mycpu()->ts,
                                sizeof(mycpu()->ts)-1, 0);
  mycpu()->gdt[SEG_TSS].s = 0;
  mycpu()->ts.ss0 = SEG_KDATA << 3;
  mycpu()->ts.esp0 = (uint)p->kstack + KSTACKSIZE;
  // setting IOPL=0 in eflags *and* iomb beyond the tss segment limit
  // forbids I/O instructions (e.g., inb and outb) from user space
  mycpu()->ts.iomb = (ushort) 0xFFFF;
  ltr(SEG_TSS << 3);
  lcr3(V2P(p->pgdir));  // switch to process's address space
  popcli();
}

// Load the initcode into address 0 of pgdir.
// sz must be less than a page.
void
inituvm(pde_t *pgdir, char *init, uint sz)
{
  char *mem;

  if(sz >= PGSIZE)
    panic("inituvm: more than a page");
  mem = kalloc();
  memset(mem, 0, PGSIZE);
  mappages(pgdir, 0, PGSIZE, V2P(mem), PTE_W|PTE_U);
  memmove(mem, init, sz);
}

// Load a program segment into pgdir.  addr must be page-aligned
// and the pages from addr to addr+sz must already be mapped.
int
loaduvm(pde_t *pgdir, char *addr, struct inode *ip, uint offset, uint sz)
{
  uint i, pa, n;
  pte_t *pte;

  if((uint) addr % PGSIZE != 0)
    panic("loaduvm: addr must be page aligned");
  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walkpgdir(pgdir, addr+i, 0)) == 0)
      panic("loaduvm: address should exist");
    pa = PTE_ADDR(*pte);
    if(sz - i < PGSIZE)
      n = sz - i;
    else
      n = PGSIZE;
    if(readi(ip, P2V(pa), offset+i, n) != n)
      return -1;
  }
  return 0;
}


// Allocate page tables and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
int
allocuvm(pde_t *pgdir, uint oldsz, uint newsz)
{

  cprintf("\n\n");
  char *mem;
  uint a;

  struct proc* p = myproc();

  if(newsz >= KERNBASE)
    return 0;
  if(newsz < oldsz)
    return oldsz;

  a = PGROUNDUP(oldsz);
  for(; a < newsz; a += PGSIZE){

    ///here changes are needed for checking if pages need to be swapped out
    int newPageCreated = 1;

    if(p->takenPhysPage>=MAX_PSYC_PAGES && p->takenSwapPage>=MAX_PSYC_PAGES){
        cprintf("OUT OF MEMORY(1): pid=%d, name=%s\n", p->pid, p->name);
        return a;

    }

    if(p->takenPhysPage>=MAX_PSYC_PAGES){
      cprintf("\nIn allocuvm(): Page needs to be swapped out. pid:%d, name:%s\n", p->pid, p->name);


      //swapPage(myproc());
      int headIdx = p->headOfQueueIdx;

      if(headIdx == INVALID_QUEUE_IDX){
        panic("QUEUE ERROR");
      }

      char *headVa = p->physPageInfo[headIdx].va;   //va of the page that need to be paged out

      ///updating queue


      p->headOfQueueIdx = p->physPageInfo[headIdx].nextIdx; //head change



      int tail = p->headOfQueueIdx;

      if(tail == INVALID_QUEUE_IDX){    //queue empty
          p->headOfQueueIdx = headIdx;
      }
      else{
        while(p->physPageInfo[tail].nextIdx != INVALID_QUEUE_IDX){
            tail = p->physPageInfo[tail].nextIdx;
        }
        p->physPageInfo[tail].nextIdx = headIdx;    //adding to tail

      }
        p->physPageInfo[headIdx].va = (char*)a;
        p->physPageInfo[headIdx].dataPresent = 1;
        p->physPageInfo[headIdx].nextIdx = INVALID_QUEUE_IDX;



      ///finding where to write in swap file
      int swapIdx = INVALID_QUEUE_IDX;

      for(int i=0; i<MAX_PSYC_PAGES; i++){
        if(p->swapPageInfo[i].dataPresent == 0){
          swapIdx = i;
          break;
        }
      }

      if(swapIdx==INVALID_QUEUE_IDX){
        cprintf("OUT OF MEMORY(2): pid=%d, name=%s\n", p->pid, p->name);
        return a;
      }

      char buf[PGSIZE/4];

      memmove(buf, headVa, PGSIZE/4);
      writeToSwapFile(p, buf, swapIdx*PGSIZE, PGSIZE/4);

      memmove(buf, headVa+PGSIZE/4, PGSIZE/4);
      writeToSwapFile(p, buf, swapIdx*PGSIZE+PGSIZE/4, PGSIZE/4);

      memmove(buf, headVa+2*PGSIZE/4, PGSIZE/4);
      writeToSwapFile(p, buf, swapIdx*PGSIZE+2*PGSIZE/4, PGSIZE/4);

      memmove(buf, headVa+3*PGSIZE/4, PGSIZE/4);
      writeToSwapFile(p, buf, swapIdx*PGSIZE+3*PGSIZE/4, PGSIZE/4);

      p->swapPageInfo[swapIdx].dataPresent = 1;
      p->swapPageInfo[swapIdx].va = headVa;


      //p->takenPhysPage = p->takenPhysPage-1;
      p->takenSwapPage = p->takenSwapPage+1;


      ///updating pgdir
      kfree((char*)PTE_ADDR(P2V(*walkpgdir(p->pgdir, headVa, 0))));

      //cprintf("headVa = %d\n", headVa);
      pte_t *pteTemp = walkpgdir(p->pgdir, headVa, 0);
      //cprintf("prev pte_t = %d\n", *walkpgdir(p->pgdir, headVa, 0));
      *pteTemp = (*pteTemp|PTE_PG)&(~PTE_P);
      //cprintf("new pte_t = %d\n", *walkpgdir(p->pgdir, headVa, 0));

      cprintf("Swapped %d to swap, allocated %d\n", headVa, a);

      newPageCreated=0;
    }


    mem = kalloc();
    if(mem == 0){
      cprintf("allocuvm out of memory: pid=%d, name=%s\n", p->pid, p->name);
      deallocuvm(pgdir, newsz, oldsz);
      return 0;
    }

    ///mem should be added in the tail of the queue

  if(newPageCreated){

      int idx = INVALID_QUEUE_IDX;

      for(int i=0; i<MAX_PSYC_PAGES; i++){
        if(p->physPageInfo[i].dataPresent == 0){
          idx = i;
          break;
        }

      }

      if(idx == INVALID_QUEUE_IDX){
        panic("PANIC UNEXPECTED NO FREE PAGES\n");
      }

      int tail = p->headOfQueueIdx;

      if(tail == INVALID_QUEUE_IDX){    //queue empty
          p->headOfQueueIdx = idx;
      }
      else{
        while(p->physPageInfo[tail].nextIdx != INVALID_QUEUE_IDX){
            tail = p->physPageInfo[tail].nextIdx;
        }
        p->physPageInfo[tail].nextIdx = idx;

      }

        p->physPageInfo[idx].nextIdx = INVALID_QUEUE_IDX;
        p->physPageInfo[idx].dataPresent = 1;
        p->physPageInfo[idx].va = (char*)a;

        p->takenPhysPage = p->takenPhysPage+1;


      cprintf("\nNew Page %d allocated at idx = %d, takenPhysPage = %d for pid=%d, name=%s\n", a, idx, p->takenPhysPage, p->pid, p->name);

  }
  //no need for else as it is done previously

    memset(mem, 0, PGSIZE);

    if(mappages(pgdir, (char*)a, PGSIZE, V2P(mem), PTE_W|PTE_U) < 0){
      cprintf("allocuvm out of memory (2): pid=%d, name=%s\n", p->pid, p->name);
      deallocuvm(pgdir, newsz, oldsz);
      kfree(mem);
      return 0;
    }


  }

  return newsz;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.

///original

/*

int
deallocuvm(pde_t *pgdir, uint oldsz, uint newsz)
{
  pte_t *pte;
  uint a, pa;

  if(newsz >= oldsz)
    return oldsz;

  a = PGROUNDUP(newsz);
  for(; a  < oldsz; a += PGSIZE){
    pte = walkpgdir(pgdir, (char*)a, 0);
    if(!pte)
      a = PGADDR(PDX(a) + 1, 0, 0) - PGSIZE;
    else if((*pte & PTE_P) != 0){
      pa = PTE_ADDR(*pte);
      if(pa == 0)
        panic("kfree");
      char *v = P2V(pa);
      kfree(v);
      *pte = 0;
    }
  }
  return newsz;
}
*/
///modified


int
deallocuvm(pde_t *pgdir, uint oldsz, uint newsz)
{
  pte_t *pte;
  uint a, pa;

  struct proc *p = myproc();

  if(newsz >= oldsz)
    return oldsz;

  a = PGROUNDUP(newsz);
  for(; a  < oldsz; a += PGSIZE){
    pte = walkpgdir(pgdir, (char*)a, 0);

      if(!pte){
        a = PGADDR(PDX(a) + 1, 0, 0) - PGSIZE;

      }
      else if((*pte & PTE_P) != 0){ ///should remove from main memory

      if(strncmp(p->name, "init", 4)==0 || strncmp(p->name, "sh", 2)==0 ){
        goto queueBypass;
      }

      //cprintf("\nIn deallocuvm(): should remove %d from MAIN MEMORY for pid=%d, name=%s\n", a, p->pid, p->name);

      char *va = (char*) a;


      ///update queue
      int idx = INVALID_QUEUE_IDX;
      int parentIdx = INVALID_QUEUE_IDX;

      for(int i=0; i<MAX_PSYC_PAGES; i++){
        if(p->physPageInfo[i].va == va && p->physPageInfo[i].dataPresent == 1){
          idx = i;
          break;
        }
      }

      if(idx == INVALID_QUEUE_IDX){
          //cprintf("NOT FOUND IN QUEUE(1)\n");
          goto queueBypass;
      }


      if(idx == p->headOfQueueIdx){ //in head
        p->headOfQueueIdx = p->physPageInfo[idx].nextIdx;
      }
      else{
        parentIdx = p->headOfQueueIdx;

        while(p->physPageInfo[parentIdx].nextIdx != idx && p->physPageInfo[parentIdx].nextIdx != INVALID_QUEUE_IDX){
          parentIdx = p->physPageInfo[parentIdx].nextIdx;
        }

        if(p->physPageInfo[parentIdx].nextIdx != idx){
          //cprintf("NOT FOUND IN QUEUE(2)\n");
          goto queueBypass;
        }

        p->physPageInfo[parentIdx].nextIdx = p->physPageInfo[idx].nextIdx; //parent to next, skipping idx


      }

      p->physPageInfo[idx].va = INVALID_ADDRESS;
      p->physPageInfo[idx].dataPresent = 0;
      p->physPageInfo[idx].nextIdx = INVALID_QUEUE_IDX;

      p->takenPhysPage--;

      cprintf("Removed %d from MAIN MEMORY for pid=%d, name=%s\n", a, p->pid, p->name);

      queueBypass:

      pa = PTE_ADDR(*pte);

      if(pa == 0)
        panic("kfree");
      char *v = P2V(pa);
      kfree(v);

      *pte = 0;




    }
    else if( (*pte&PTE_P)==0 && (*pte&(~PTE_PG))!=0 ){  ///in swap file

      if(strncmp(p->name, "init", 4)==0 ){  //|| strncmp(p->name, "sh", 2)==0
        goto swapBypass;
      }

      //cprintf("\nIn deallocuvm(): should remove %d from SWAP for pid=%d, name=%s\n", a, p->pid, p->name);

      char *va = (char*) a;

      int swapIdx = INVALID_QUEUE_IDX;
      for(int i=0; i<MAX_PSYC_PAGES; i++){
        if(p->swapPageInfo[i].va == va && p->swapPageInfo[i].dataPresent == 1){
          swapIdx = i;
          break;
        }
      }

      if(swapIdx==INVALID_QUEUE_IDX){
        //cprintf("NOT FOUND IN SWAP\n");
        goto swapBypass;
      }

      p->swapPageInfo[swapIdx].va = INVALID_ADDRESS;
      p->swapPageInfo[swapIdx].dataPresent = 0;

      p->takenSwapPage--;

      cprintf("Discarded %d from SWAP. pid=%d, name=%s\n", va, p->pid, p->name);


      swapBypass:

      *pte = 0;

    }
  }

  return newsz;

}



// Free a page table and all the physical memory pages
// in the user part.
void
freevm(pde_t *pgdir)
{
  uint i;

  if(pgdir == 0)
    panic("freevm: no pgdir");
  deallocuvm(pgdir, KERNBASE, 0);
  for(i = 0; i < NPDENTRIES; i++){
    if(pgdir[i] & PTE_P){
      char * v = P2V(PTE_ADDR(pgdir[i]));
      kfree(v);
    }
  }
  kfree((char*)pgdir);
}

// Clear PTE_U on a page. Used to create an inaccessible
// page beneath the user stack.
void
clearpteu(pde_t *pgdir, char *uva)
{
  pte_t *pte;

  pte = walkpgdir(pgdir, uva, 0);
  if(pte == 0)
    panic("clearpteu");
  *pte &= ~PTE_U;
}

// Given a parent process's page table, create a copy
// of it for a child.
pde_t*
copyuvm(pde_t *pgdir, uint sz)
{
  pde_t *d;
  pte_t *pte;
  uint pa, i, flags;
  char *mem;

  if((d = setupkvm()) == 0)
    return 0;
  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walkpgdir(pgdir, (void *) i, 0)) == 0)
      panic("copyuvm: pte should exist");
    if(!(*pte & PTE_P))
      panic("copyuvm: page not present");
    pa = PTE_ADDR(*pte);
    flags = PTE_FLAGS(*pte);
    if((mem = kalloc()) == 0)
      goto bad;
    memmove(mem, (char*)P2V(pa), PGSIZE);
    if(mappages(d, (void*)i, PGSIZE, V2P(mem), flags) < 0) {
      kfree(mem);
      goto bad;
    }
  }
  return d;

bad:
  freevm(d);
  return 0;
}

//PAGEBREAK!
// Map user virtual address to kernel address.
char*
uva2ka(pde_t *pgdir, char *uva)
{
  pte_t *pte;

  pte = walkpgdir(pgdir, uva, 0);
  if((*pte & PTE_P) == 0)
    return 0;
  if((*pte & PTE_U) == 0)
    return 0;
  return (char*)P2V(PTE_ADDR(*pte));
}

// Copy len bytes from p to user address va in page table pgdir.
// Most useful when pgdir is not the current page table.
// uva2ka ensures this only works for PTE_U pages.
int
copyout(pde_t *pgdir, uint va, void *p, uint len)
{
  char *buf, *pa0;
  uint n, va0;

  buf = (char*)p;
  while(len > 0){
    va0 = (uint)PGROUNDDOWN(va);
    pa0 = uva2ka(pgdir, (char*)va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (va - va0);
    if(n > len)
      n = len;
    memmove(pa0 + (va - va0), buf, n);
    len -= n;
    buf += n;
    va = va0 + PGSIZE;
  }
  return 0;
}

//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.

