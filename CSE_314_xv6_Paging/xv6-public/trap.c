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

void printQueue(){


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

    cprintf("\n\nT_PGFLT\n");

    char* va = (char*) rcr2();

    cprintf("rcr2 = %d\n", va);

    va = (char*) PGROUNDDOWN((int)va);

    cprintf("va = %d\n", va);

    pte_t *pteTemp = walkpgdir(myproc()->pgdir, va, 0);

    cprintf("pteTemp=%d\n", *pteTemp);

    if(  (*pteTemp&PTE_P)==0 && (*pteTemp&(~PTE_PG))!=0 ){

        ///swapped out, va needs to be loaded back into the memory

        cprintf("T_PGFLT: va=%d, pid = %d, name=%s\n",va, myproc()->pid, myproc()->name);


        ///getting from what offset to read
        int swapIdx = INVALID_QUEUE_IDX;

        for(int i=0; i<MAX_PSYC_PAGES; i++){
          if(myproc()->swapPageInfo[i].va == va && myproc()->swapPageInfo[i].dataPresent == 1){
            //cprintf("found swapIdx = %d\n", i );
            swapIdx = i;
            break;
          }
        }

        if(swapIdx==INVALID_QUEUE_IDX){
          panic("T_PGFLT: SWAP PAGE NOT FOUND");
        }


        char* swapVa = myproc()->swapPageInfo[swapIdx].va;


        ///storing in dummy offset in swap file
        int dummySwapIdx = MAX_PSYC_PAGES;

        for(int i=0; i<MAX_PSYC_PAGES; i++){
          if(myproc()->swapPageInfo[i].va == INVALID_ADDRESS && myproc()->swapPageInfo[i].dataPresent == 0){
            dummySwapIdx = i;
            break;
          }
        }

        char buf[PGSIZE/4];

        readFromSwapFile(myproc(), buf, swapIdx*PGSIZE, PGSIZE/4);
        writeToSwapFile(myproc(), buf, dummySwapIdx*PGSIZE, PGSIZE/4);

        readFromSwapFile(myproc(), buf, swapIdx*PGSIZE+PGSIZE/4, PGSIZE/4);
        writeToSwapFile(myproc(), buf, dummySwapIdx*PGSIZE+PGSIZE/4, PGSIZE/4);

        readFromSwapFile(myproc(), buf, swapIdx*PGSIZE+2*PGSIZE/4, PGSIZE/4);
        writeToSwapFile(myproc(), buf, dummySwapIdx*PGSIZE+2*PGSIZE/4, PGSIZE/4);

        readFromSwapFile(myproc(), buf, swapIdx*PGSIZE+3*PGSIZE/4, PGSIZE/4);
        writeToSwapFile(myproc(), buf, dummySwapIdx*PGSIZE+3*PGSIZE/4, PGSIZE/4);


        ///updating queue for newly swapped out page


        int headIdx = myproc()->headOfQueueIdx;
        if(headIdx == INVALID_QUEUE_IDX){
          panic("QUEUE ERROR");
        }

        char *headVa = myproc()->physPageInfo[headIdx].va;   //va of the page that need to be paged out

        myproc()->physPageInfo[headIdx].va = INVALID_ADDRESS;
        myproc()->physPageInfo[headIdx].dataPresent = 0;

        myproc()->headOfQueueIdx = myproc()->physPageInfo[headIdx].nextIdx;
        myproc()->physPageInfo[headIdx].nextIdx = INVALID_QUEUE_IDX;

        //cprintf("headIdx = %d, new headIdx = %d\n", headIdx, myproc()->headOfQueueIdx);

        ///writing content of head to swap file
        writeToSwapFile(myproc(), headVa, swapIdx*PGSIZE, PGSIZE);

        myproc()->swapPageInfo[swapIdx].dataPresent = 1;
        myproc()->swapPageInfo[swapIdx].va = headVa;



        ///updating pgdir
        kfree((char*)PTE_ADDR(P2V(*walkpgdir(myproc()->pgdir, headVa, 0))));   //freeing headVa

        //cprintf("headVa = %d\n", headVa);
        pte_t *pteHead = walkpgdir(myproc()->pgdir, headVa, 0);      //now written to swap file
       // cprintf("prev pteHead = %d\n", *walkpgdir(myproc()->pgdir, headVa, 0));
        *pteHead = (*pteHead|PTE_PG)&(~PTE_P);
        //cprintf("new pteHead = %d\n", *walkpgdir(myproc()->pgdir, headVa, 0));



        char *mem = kalloc();   //new memory acquired
        if(mem == 0){
          cprintf("T_PGFLT out of memory: pid=%d, name=%s\n", myproc()->pid, myproc()->name);
          return;
        }


        //cprintf("swapVa = %d\n", swapVa);
        pte_t *pteSwap = walkpgdir(myproc()->pgdir, swapVa, 0);    //will be brought back to memory
        //cprintf("prev pteSwap = %d\n", *walkpgdir(myproc()->pgdir, swapVa, 0));
        *pteSwap = (((V2P(mem))>>12)<<12 |PTE_P|PTE_W|PTE_U ) ;
        //cprintf("new pteSwap = %d\n", *walkpgdir(myproc()->pgdir, swapVa, 0));


        ///writing previously swapped page to memory

        int idx = INVALID_QUEUE_IDX;  //where to write in array structure

        for(int i=0; i<MAX_PSYC_PAGES; i++){
          if(myproc()->physPageInfo[i].dataPresent == 0){
            idx = i;
            break;
          }

        }

        if(idx == INVALID_QUEUE_IDX){
          panic("PANIC UNEXPECTED NO FREE PAGES\n");
        }


        int tail = myproc()->headOfQueueIdx;   //finding tail of the queue

        if(tail == INVALID_QUEUE_IDX){    //queue empty
            myproc()->headOfQueueIdx = idx;
        }
        else{
          while(myproc()->physPageInfo[tail].nextIdx != INVALID_QUEUE_IDX){
              tail = myproc()->physPageInfo[tail].nextIdx;
          }
          myproc()->physPageInfo[tail].nextIdx = idx;

        }

          myproc()->physPageInfo[idx].nextIdx = INVALID_QUEUE_IDX;
          myproc()->physPageInfo[idx].dataPresent = 1;
          myproc()->physPageInfo[idx].va = swapVa;




      readFromSwapFile(myproc(), buf, dummySwapIdx*PGSIZE, PGSIZE/4);
      memmove(swapVa, buf, PGSIZE/4);

      readFromSwapFile(myproc(), buf, dummySwapIdx*PGSIZE+PGSIZE/4, PGSIZE/4);
      memmove(swapVa+PGSIZE/4, buf, PGSIZE/4);

      readFromSwapFile(myproc(), buf, dummySwapIdx*PGSIZE+2*PGSIZE/4, PGSIZE/4);
      memmove(swapVa+2*PGSIZE/4, buf, PGSIZE/4);

      readFromSwapFile(myproc(), buf, dummySwapIdx*PGSIZE+3*PGSIZE/4, PGSIZE/4);
      memmove(swapVa+3*PGSIZE/4, buf, PGSIZE/4);






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
