#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"

// Interrupt descriptor table (shared by all CPUs).
struct gatedesc idt[256];
extern uint vectors[];  // in vectors.S: array of 256 entry pointers
struct spinlock tickslock;
uint ticks;

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


void
tvinit(void)
{
  int i;

  for(i = 0; i < 256; i++)
    SETGATE(idt[i], 0, SEG_KCODE<<3, vectors[i], 0);
  SETGATE(idt[T_SYSCALL], 1, SEG_KCODE<<3, vectors[T_SYSCALL], DPL_USER);

  initlock(&tickslock, "time");
}

void
idtinit(void)
{
  lidt(idt, sizeof(idt));
}

//PAGEBREAK: 41
void
trap(struct trapframe *tf)
{
  if(tf->trapno == T_SYSCALL){
    if(myproc()->killed)
      exit();
    myproc()->tf = tf;
    syscall();
    if(myproc()->killed)
      exit();
    return;
  }

  switch(tf->trapno){
  case T_IRQ0 + IRQ_TIMER:
    if(cpuid() == 0){
      acquire(&tickslock);
      ticks++;
      wakeup(&ticks);
      release(&tickslock);
    }
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE:
    ideintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE+1:
    // Bochs generates spurious IDE1 interrupts.
    break;
  case T_IRQ0 + IRQ_KBD:
    kbdintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_COM1:
    uartintr();
    lapiceoi();
    break;
  case T_IRQ0 + 7:
  case T_IRQ0 + IRQ_SPURIOUS:
    cprintf("cpu%d: spurious interrupt at %x:%x\n",
            cpuid(), tf->cs, tf->eip);
    lapiceoi();
    break;

  //PAGEBREAK: 13

  case T_PGFLT:
  ///new case added to handle paged out memory fault

  if(myproc()->pid==1){
    break;
  }

  cprintf("T_PGFLT\n");

  struct proc *p;

  char* va = (char*) rcr2();

  cprintf("rcr2 = %d\n", va);

  va = (char*) PGROUNDDOWN((int)va);

  cprintf("va = %d\n", va);

  pte_t *pteTemp = walkpgdir(myproc()->pgdir, va, 0);

  cprintf("pteTemp=%d\n", *pteTemp);

  if(  (*pteTemp&PTE_P)==0 && (*pteTemp&(~PTE_PG))!=0 ){

      ///swapped out, va needs to be loaded back into the memory
      p = myproc();
      cprintf("T_PGFLT: va=%d page has to be loaded into the memory from swap file. pid = %d, name=%s\n", p->pid, p->name);


      ///getting from what offset to read
      int swapIdx = INVALID_QUEUE_IDX;

      for(int i=0; i<MAX_PSYC_PAGES; i++){
        if(p->swapPageInfo[i].va == va && p->swapPageInfo[i].dataPresent == 1){
          cprintf("found swapIdx = %d\n", i );
          swapIdx = i;
          break;
        }
      }

      if(swapIdx==INVALID_QUEUE_IDX){
        panic("T_PGFLT: SWAP PAGE NOT FOUND");
      }

      ///storing in buffer from swap file
      char* swapVa = p->swapPageInfo[swapIdx].va;


      int dummySwapIdx = MAX_PSYC_PAGES;

      for(int i=0; i<MAX_PSYC_PAGES; i++){
        if(p->swapPageInfo[i].va == INVALID_ADDRESS && p->swapPageInfo[i].dataPresent == 0){
          dummySwapIdx = i;
          break;
        }
      }

      char buf[PGSIZE/4];

      readFromSwapFile(p, buf, swapIdx*PGSIZE, PGSIZE/4);
      writeToSwapFile(p, buf, dummySwapIdx*PGSIZE, PGSIZE/4);

      readFromSwapFile(p, buf, swapIdx*PGSIZE+PGSIZE/4, PGSIZE/4);
      writeToSwapFile(p, buf, dummySwapIdx*PGSIZE+PGSIZE/4, PGSIZE/4);

      readFromSwapFile(p, buf, swapIdx*PGSIZE+2*PGSIZE/4, PGSIZE/4);
      writeToSwapFile(p, buf, dummySwapIdx*PGSIZE+2*PGSIZE/4, PGSIZE/4);

      readFromSwapFile(p, buf, swapIdx*PGSIZE+3*PGSIZE/4, PGSIZE/4);
      writeToSwapFile(p, buf, dummySwapIdx*PGSIZE+3*PGSIZE/4, PGSIZE/4);



      int headIdx = p->headOfQueueIdx;

      if(headIdx == INVALID_QUEUE_IDX){
        panic("QUEUE ERROR");
      }

      char *headVa = p->physPageInfo[headIdx].va;   //va of the page that need to be paged out

      ///updating queue for newly swapped out page
      p->physPageInfo[headIdx].va = INVALID_ADDRESS;
      p->physPageInfo[headIdx].dataPresent = 0;

      p->headOfQueueIdx = p->physPageInfo[headIdx].nextIdx;
      p->physPageInfo[headIdx].nextIdx = INVALID_QUEUE_IDX;


      ///writing to swap file
      writeToSwapFile(p, headVa, swapIdx*PGSIZE, PGSIZE);

      p->swapPageInfo[swapIdx].dataPresent = 1;
      p->swapPageInfo[swapIdx].va = headVa;

      ///updating pgdir for newly swapped out page
      //kfree((char*)PTE_ADDR(P2V(*walkpgdir(p->pgdir, headVa, 0))));

      pte_t *pteTemp2 = walkpgdir(p->pgdir, headVa, 0);
      cprintf("prev pteTemp2 = %d\n", *walkpgdir(p->pgdir, headVa, 0));
      *pteTemp2 = (*pteTemp2|PTE_PG)&(~PTE_P);
      cprintf("new pteTemp2 = %d\n", *walkpgdir(p->pgdir, headVa, 0));
      ///writing in swap file done



      ///writing previously swapped page to memory

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
        p->physPageInfo[idx].va = swapVa;


    char *mem = kalloc();
    if(mem == 0){
      cprintf("trap out of memory: pid=%d, name=%s\n", p->pid, p->name);
      return;
    }

    memset(mem, 0, PGSIZE);

    pte_t *pteMem = walkpgdir(p->pgdir, swapVa, 0);
    cprintf("prev pte_t = %d\n", *walkpgdir(p->pgdir, headVa, 0));
    *pteMem = ( (V2P(mem)>>12)<<12 |PTE_P|PTE_W|PTE_U ) ;
    cprintf("new pte_t = %d\n", *walkpgdir(p->pgdir, headVa, 0));


    readFromSwapFile(p, buf, dummySwapIdx*PGSIZE, PGSIZE/4);
    safestrcpy(headVa, buf, PGSIZE/4);

    readFromSwapFile(p, buf, dummySwapIdx*PGSIZE+PGSIZE/4, PGSIZE/4);
    safestrcpy(headVa+PGSIZE/4, buf, PGSIZE/4);

    readFromSwapFile(p, buf, dummySwapIdx*PGSIZE+2*PGSIZE/4, PGSIZE/4);
    safestrcpy(headVa+2*PGSIZE/4, buf, PGSIZE/4);

    readFromSwapFile(p, buf, dummySwapIdx*PGSIZE+3*PGSIZE/4, PGSIZE/4);
    safestrcpy(headVa+3*PGSIZE/4, buf, PGSIZE/4);






      return;

  }






  default:
    if(myproc() == 0 || (tf->cs&3) == 0){
      // In kernel, it must be our mistake.
      cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
              tf->trapno, cpuid(), tf->eip, rcr2());
      panic("trap");
    }
    // In user space, assume process misbehaved.
    cprintf("pid %d %s: trap %d err %d on cpu %d "
            "eip 0x%x addr 0x%x--kill proc\n",
            myproc()->pid, myproc()->name, tf->trapno,
            tf->err, cpuid(), tf->eip, rcr2());
    myproc()->killed = 1;
  }

  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running
  // until it gets to the regular system call return.)
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();

  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
  if(myproc() && myproc()->state == RUNNING &&
     tf->trapno == T_IRQ0+IRQ_TIMER)
    yield();

  // Check if the process has been killed since we yielded
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();
}
