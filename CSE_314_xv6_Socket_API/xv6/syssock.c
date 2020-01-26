#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "sockparam.h"

static int
mystrcmp(const char *p, const char *q)
{
  while(*p && *p == *q)
    p++, q++;
  return (uchar)*p - (uchar)*q;
}

int
sys_listen(void)
{
  //cprintf("***In sys_listen()***\n");

  int port = 0;

  ///
  /// TODO: Write your code to get and validate port no.
  ///

  ///validating port
  argint(0, &port);
  //cprintf("In sys_listen():  port = %d\n", port);

  if(port < 0 || port >= NPORT){
    cprintf("In sys_listen(): INVALID PORT NUMBER\n");
    return E_INVALID_ARG;
  }

  return listen(port);
}

int
sys_connect(void)
{
  //cprintf("***In sys_connect()***\n");

  int port = 0;
  char *host = 0;

  ///
  /// TODO: Write your code to get and validate port no., host.
  /// Allow connection to "localhost" or "127.0.0.1" host only
  ///

  ///validating port
  argint(0, &port);
  //cprintf("In sys_connect(): port = %d\n", port);

  if(port < 0 || port >= NPORT){
    cprintf("In sys_connect(): INVALID PORT NUMBER\n");
    return E_INVALID_ARG;
  }

  ///validating host
  argstr(1, &host);
  //cprintf("In sys_connect(): host = %s\n", host);

  if(mystrcmp(host, "localhost")!=0 && mystrcmp(host, "127.0.0.1")!=0){
    cprintf("In sys_connect(): INVALID HOST ADDRESS\n");
    return E_INVALID_ARG;
  }


  return connect(port, host);
}

int
sys_send(void)
{
  //cprintf("***In sys_send()***\n");

  int port = 0;
  char* buf = 0;
  int n = 0;

  ///
  /// TODO: Write your code to get and validate port no., buffer and buffer size
  ///

  ///validating port
  argint(0, &port);
  //cprintf("In sys_send():  port = %d\n", port);

  if(port < 0 || port >= NPORT){
    cprintf("In sys_send(): INVALID PORT NUMBER\n");
    return E_INVALID_ARG;
  }


  ///validating buf
  argptr(1, &buf, sizeof(buf));
  //cprintf("In sys_send():  buf = %s\n", buf);

  if(buf==0){
    cprintf("In sys_send(): INVALID BUFFER\n");
    return E_INVALID_ARG;
  }

  ///gettiing n
  argint(2, &n);
  //cprintf("In sys_send():  n = %d\n", n);

  if(n>SOCKET_BUFFER_SIZE){
    cprintf("In sys_send(): INVALID NUMBER OF CHARS\n");
    return E_INVALID_ARG;
  }

  return send(port, buf, n);
}

int
sys_recv(void)
{
  //cprintf("***In sys_recv()***\n");

  int port = 0;
  char* buf = 0;
  int n = 0;

  ///
  /// TODO: Write your code to get and validate port no., buffer and buffer size
  ///

  ///validating port
  argint(0, &port);
  //cprintf("In sys_recv():  port = %d\n", port);

  if(port < 0 || port >= NPORT){
    cprintf("In sys_recv(): INVALID PORT NUMBER\n");
    return E_INVALID_ARG;
  }


  ///validating buf
  argptr(1, &buf, sizeof(buf));
  //cprintf("In sys_recv():  &buf = %d\n", &buf);

  if(buf==0){
    cprintf("In sys_recv(): INVALID BUFFER\n");
    return E_INVALID_ARG;
  }


  ///gettiing n
  argint(2, &n);
  //cprintf("In sys_recv():  n = %d\n", n);
  if(n>SOCKET_BUFFER_SIZE){
    cprintf("In sys_recv(): INVALID NUMBER OF CHARS\n");
    return E_INVALID_ARG;
  }

  return recv(port, buf, n);
}

int
sys_disconnect(void)
{
  //cprintf("***In sys_disconnect()***\n");

  int port = 0;

  ///
  /// TODO: Write your code to get and validate port no.
  ///

  argint(0, &port);

  //cprintf("In sys_disconnect():  port = %d\n", port);

  if(port < 0 || port >= NPORT){
    cprintf("In sys_disconnect():INVALID PORT NUMBER\n");
    return E_INVALID_ARG;
  }

  return disconnect(port);
}





