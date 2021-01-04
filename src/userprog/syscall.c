#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/init.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{

  void *esp = f->esp;
  int sysnum = *((int*)esp);
  switch (sysnum)
  {
  case 9:
    esp+=sizeof(int);
    int fd = *((int*)esp);
    esp+=sizeof(int);
    const void* buff = *((void **) esp);
    esp+=sizeof(void*);
    unsigned size = *((unsigned*)esp);
    printf("%d\n",write(fd,buff,size));
    break;
  
  default:
    break;
  }

  thread_exit();  
}
void halt(void){
  shutdown_power_off();
}
void exit(int status){

}
int write(int fd,const void *buff, unsigned size){
  if(fd==1){
      putbuf(buff,size);
    return size;
  }

}
