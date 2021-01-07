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
  //printf("%d\n",sysnum);
  esp+=sizeof(int);
  switch (sysnum)
  {
  case 1:
    exit(0);
    break;  
  case 9:;
    int fd = *((int*)esp);
    esp+=sizeof(int);
    //hex_dump(esp,esp,100,1);
    const void* buff = *((void **) esp);
    esp+=sizeof(void*);
    unsigned size = *((unsigned*)esp);
    esp+=sizeof(unsigned);
    //printf("size %d\n",size);
    int returnVal = write(fd,buff,size);
    f->eax = returnVal;
    //printf("%d\n",returnVal);
    break;
  
  default:
    break;
  }

}
void halt(void){
  shutdown_power_off();
}
void exit(int status){
  printf("%s: exit(%d)\n",thread_name(),status);
  thread_exit();
}
int write(int fd,const void *buff, unsigned size){
  if(fd==1){
      putbuf(buff,size);
      return size;
  }

}
