#include "types.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "sock.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "sockparam.h"

///
/// TODO: Create a structure to maintain a list of sockets
/// Should it have locking?
///

///helpful functions

static void
bufferCopy(char *s, const char *t, int n){
  for(int i=0; i<n; i++){
    s[i] = t[i];
  }
}

static void
bufferClear(char *s){
  s[0] = 0;
}

struct {
  struct spinlock lock;
  struct sock sockets[NSOCK];
} stable;

int portToSocketIdxMap[NPORT];

void
sinit(void)
{
  //cprintf("***In sinit()***\n");
  ///
  /// TODO: Write any initialization code for socket API
  /// initialization.
  ///

  int i;

  ///initializing stable
  for(i = 0; i < NSOCK; i++){
    stable.sockets[i].local_port = INVALID_PORT;
    stable.sockets[i].remote_port = INVALID_PORT;
    stable.sockets[i].state = CLOSED;
    stable.sockets[i].owner_pid = INVALID_PID;
    stable.sockets[i].data_available = FALSE;
  }
  initlock(&stable.lock, "stable");

  ///initializing port to socket map
  for(i = 0; i < NPORT; i++){
    portToSocketIdxMap[i] = INVALID_SOCKET_INDEX;
  }

  //cprintf("In sinit(): FINISHED INITIALIZATION\n");

}

int
listen(int lport) {
  //cprintf("***In listen()***\n");
  //cprintf("In listen(): lport = %d\n", lport);

  ///
  /// TODO: Put the actual implementation of listen here.
  ///

  ///error check
  if(portToSocketIdxMap[lport]!=INVALID_SOCKET_INDEX){
    cprintf("In listen(): PORT ALREADY IN USE\n");
    return E_INVALID_ARG;
  }

  acquire(&stable.lock);

  ///finding available local socket
  int sockIdx = INVALID_SOCKET_INDEX;
  for(int i=0; i<NSOCK; i++){
    if(stable.sockets[i].state == CLOSED){
      sockIdx = i;
      break;
    }
  }
  //cprintf("In listen(): sockIdx = %d\n", sockIdx);

  if(sockIdx == INVALID_SOCKET_INDEX){
    cprintf("In listen(): NO USABLE SOCKET AVAILABLE\n");
    release(&stable.lock);
    return E_FAIL;
  }


  ///going to listening mode
  stable.sockets[sockIdx].local_port = lport;
  stable.sockets[sockIdx].state = LISTENING;
  stable.sockets[sockIdx].owner_pid = myproc()->pid;

  ///mapping this local port to socket index
  portToSocketIdxMap[lport] = sockIdx;

  ///remote port not set yet
  sleep(&stable.sockets[sockIdx], &stable.lock);
  //cprintf("In listen(): AWAKENED FROM SLEEP!\n");

  //stable.sockets[sockIdx].remote_port = 20;

  if(stable.sockets[sockIdx].remote_port == INVALID_PORT){
    cprintf("In listen(): REMOTE PORT IS NOT SET!\n");
    release(&stable.lock);
    return E_CONNECTION_ERROR;
  }
  else{
    //cprintf("In listen(): REMOTE PORT IS SET TO = %d\n", stable.sockets[sockIdx].remote_port);
  }

  stable.sockets[sockIdx].state = CONNECTED;

  release(&stable.lock);

  cprintf("In listen(): CONNECTION SUCCESSFULLY ESTABLISHED!\n");

  return SOCKET_OPERATION_SUCCESSFUL;


}

int
connect(int rport, const char* host) {
  //cprintf("***In connect()***\n");
  //cprintf("In connect(): rport = %d\n", rport);
  //cprintf("In connect(): host = %s\n", host);

  ///
  /// TODO: Put the actual implementation of connect here.
  ///


  ///error check
  if(portToSocketIdxMap[rport]==INVALID_SOCKET_INDEX){
    cprintf("In connect(): PORT NOT IN USE\n");
    return E_INVALID_ARG;
  }

  acquire(&stable.lock);

  ///finding available local port
  int lport = INVALID_PORT;
  for(int i=0; i<NPORT; i++){
    if(portToSocketIdxMap[i]==INVALID_SOCKET_INDEX){
      lport = i;
      break;
    }
  }
  //cprintf("In connect(): lport = %d\n", lport);

  if(lport == INVALID_PORT){
    cprintf("In connect(): NO USABLE PORT AVAILABLE\n");
    release(&stable.lock);
    return E_FAIL;
  }


  ///finding available local socket
  int sockIdx = INVALID_SOCKET_INDEX;
  for(int i=0; i<NSOCK; i++){
    if(stable.sockets[i].state == CLOSED){
      sockIdx = i;
      break;
    }
  }
  //cprintf("In connect(): sockIdx = %d\n", sockIdx);

  if(sockIdx == INVALID_SOCKET_INDEX){
    cprintf("In connect(): NO USABLE SOCKET AVAILABLE\n");
    release(&stable.lock);
    return E_FAIL;
  }


  ///finding server socket
  int remoteSockIdx = portToSocketIdxMap[rport];
  //cprintf("In connect(): remoteSockIdx = %d\n", remoteSockIdx);

  if(remoteSockIdx == INVALID_SOCKET_INDEX){
    cprintf("In connect(): INVALID SERVER SOCKET\n");
    release(&stable.lock);
    return E_NOTFOUND;
  }
  else if(stable.sockets[remoteSockIdx].state != LISTENING){
    cprintf("In connect(): INVALID SERVER STATE\n");
    return E_WRONG_STATE;
  }

  ///configuring local socket
  stable.sockets[sockIdx].local_port = lport;
  stable.sockets[sockIdx].remote_port = rport;
  stable.sockets[sockIdx].state = CONNECTED;
  stable.sockets[sockIdx].owner_pid = myproc()->pid;

  portToSocketIdxMap[lport] = sockIdx;


  ///writing lport to rport of server socket and awake it
  stable.sockets[remoteSockIdx].remote_port = lport;

  wakeup(&stable.sockets[remoteSockIdx]);

  release(&stable.lock);

  cprintf("In connect(): CONNECTION SUCCESSFULLY ESTABLISHED!\n");

  return lport;
}

int
send(int lport, const char* data, int n) {
  //cprintf("***In send()***\n");
  //cprintf("In send():  port = %d\n", lport);
  //cprintf("In send():  data = %s\n", data);
  //cprintf("In send():  n = %d\n", n);

  ///
  /// TODO: Put the actual implementation of send here.
  ///

  acquire(&stable.lock);

  ///validating local socket
  int sockIdx = portToSocketIdxMap[lport];
  if(sockIdx == INVALID_SOCKET_INDEX){
      cprintf("In send(): INVALID LOCAL SOCKET\n");
      release(&stable.lock);
      return E_NOTFOUND;
  }
  else if(stable.sockets[sockIdx].owner_pid != myproc()->pid){
    cprintf("In send(): INVALID OWNER PROCESS\n");
    release(&stable.lock);
    return E_ACCESS_DENIED;
  }
  else if(stable.sockets[sockIdx].state != CONNECTED){
    cprintf("In send(): INVALID SOCKET STATE\n");
    release(&stable.lock);
    return E_WRONG_STATE;
  }


  ///validating remote port
  int rport = stable.sockets[sockIdx].remote_port;
   if(rport == INVALID_PORT){
    cprintf("In send(): REMOTE PORT IS NOT SET!\n");
    release(&stable.lock);
    return E_CONNECTION_ERROR;
  }

  ///validating remote socket
  int remoteSockIdx = portToSocketIdxMap[rport];
  //cprintf("In send(): remoteSockIdx = %d\n", remoteSockIdx);

  if(remoteSockIdx == INVALID_SOCKET_INDEX){
    cprintf("In send(): INVALID REMOTE SOCKET\n");
    release(&stable.lock);
    return E_NOTFOUND;
  }
  else if(stable.sockets[remoteSockIdx].state != CONNECTED){
    cprintf("In send(): INVALID REMOTE STATE\n");
    release(&stable.lock);
    return E_WRONG_STATE;
  }

  if(stable.sockets[remoteSockIdx].data_available == TRUE){ //wait for the buffer to be cleared
    sleep(&stable.sockets[remoteSockIdx], &stable.lock);
    //cprintf("In send(): AWAKENED FROM SLEEP!\n");
  }

  bufferCopy(stable.sockets[remoteSockIdx].buffer, data, n);
  stable.sockets[remoteSockIdx].data_available = TRUE;

  wakeup(&stable.sockets[remoteSockIdx]);

  release(&stable.lock);

  //cprintf("In send(): DATA SUCCESSFULLY SENT!\n");

  return SOCKET_OPERATION_SUCCESSFUL;
}


int
recv(int lport, char* data, int n) {
  //cprintf("***In recv()***\n");
  //cprintf("In recv():  port = %d\n", lport);
  //cprintf("In recv():  n = %d\n", n);

  ///
  /// TODO: Put the actual implementation of recv here.
  ///

  acquire(&stable.lock);

  ///validating local socket
  int sockIdx = portToSocketIdxMap[lport];
  if(sockIdx == INVALID_SOCKET_INDEX){
      cprintf("In recv(): INVALID LOCAL SOCKET\n");
      release(&stable.lock);
      return E_NOTFOUND;
  }
  else if(stable.sockets[sockIdx].owner_pid != myproc()->pid){
    cprintf("In recv(): INVALID OWNER PROCESS\n");
    release(&stable.lock);
    return E_ACCESS_DENIED;
  }
  else if(stable.sockets[sockIdx].state != CONNECTED){
    cprintf("In recv(): INVALID SOCKET STATE\n");
    release(&stable.lock);
    return E_WRONG_STATE;
  }


  if(stable.sockets[sockIdx].data_available == FALSE){ //wait for the buffer to be written
    sleep(&stable.sockets[sockIdx], &stable.lock);
    //cprintf("In recv(): AWAKENED FROM SLEEP!\n");
  }

  bufferCopy(data, stable.sockets[sockIdx].buffer, n);

  bufferClear(stable.sockets[sockIdx].buffer);
  stable.sockets[sockIdx].data_available = FALSE;


  wakeup(&stable.sockets[sockIdx]);

  release(&stable.lock);

  //cprintf("In recv(): DATA SUCCESSFULLY RECEIVED! : %s\n", data);

  return SOCKET_OPERATION_SUCCESSFUL;

}

int
disconnect(int lport) {
  //cprintf("***In disconnect()***\n");
  //cprintf("In disconnect(): lport = %d\n", lport);

  ///
  /// TODO: Put the actual implementation of disconnect here.
  ///

  acquire(&stable.lock);

  ///error check
  int sockIdx = portToSocketIdxMap[lport];
  if(portToSocketIdxMap[lport]==INVALID_SOCKET_INDEX || stable.sockets[sockIdx].state != CONNECTED){
    cprintf("In disconnect(): INVALID PORT or SOCKET STATE\n");
    release(&stable.lock);
    return E_NOTFOUND;
  }
  else if(stable.sockets[sockIdx].owner_pid != myproc()->pid){
    cprintf("In disconnect(): INVALID OWNER PROCESS\n");
    release(&stable.lock);
    return E_ACCESS_DENIED;
  }

  stable.sockets[sockIdx].local_port = INVALID_PORT;
  stable.sockets[sockIdx].remote_port = INVALID_PORT;
  stable.sockets[sockIdx].state = CLOSED;
  stable.sockets[sockIdx].owner_pid = INVALID_PID;
  bufferClear(stable.sockets[sockIdx].buffer);
  stable.sockets[sockIdx].data_available = FALSE;

  portToSocketIdxMap[lport] = INVALID_SOCKET_INDEX;

   release(&stable.lock);

  //cprintf("In disconnect(): SUCCESSFULLY DISCONNECTED!\n");

  return SOCKET_OPERATION_SUCCESSFUL;
}
