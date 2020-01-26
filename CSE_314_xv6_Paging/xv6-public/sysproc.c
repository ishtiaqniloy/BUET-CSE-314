#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

///dummy system call to check

/*static void
bufferCopy(char *s, const char *t, int n){
  for(int i=0; i<n && t[i]!=0; i++){
    s[i] = t[i];
  }
}*/

int
sys_testCall(void)
{
    cprintf("In test system call\n");

    //growproc(20000);

    /*createSwapFile(myproc());

    cprintf("sz = %d\n", myproc()->sz);


    char writeBuffer[4096];
    writeBuffer[0] = 0;
    cprintf("%s\n", writeBuffer);


    bufferCopy(writeBuffer, "Hello World", 4096);
    writeToSwapFile(myproc(), writeBuffer, 0, 4096);


    bufferCopy(writeBuffer, "Another string", 4096);
    writeToSwapFile(myproc(), writeBuffer, 4096, 4096);


    char readBuffer[4096];

    readFromSwapFile(myproc(), readBuffer, 0, 4096);
    cprintf("After read = %s\n", readBuffer);

    readFromSwapFile(myproc(), readBuffer, 4096, 4096);
    cprintf("After 2nd read = %s\n", readBuffer);



    cprintf("sz = %d\n", myproc()->sz);


*/



    return 0;

}








