#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/init.h"

static int file_descriptor = 1;
static void syscall_handler (struct intr_frame *);
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
 bool create (const char *file, unsigned initial_size){
    return filesys_create(file,initial_size);
 }
int open (const char *file)
{
  struct file* my_file =  filesys_open(file);
  if(my_file!=NULL) {file_descriptor+=1; return file_descriptor;}
  else return -1;
}

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
  esp+=sizeof(int);
  switch (sysnum)
  {
  case 0:
    halt();
    break;
  case 1:;
    int exitVal = *((int* )esp);
    if(exitVal<-1000) exit(-1);
    exit(exitVal);
    break;
  case 4:;
    const char* file = *((char**)esp);
    if(file==NULL) exit(-1);
    esp+=sizeof(char*);
    unsigned initial_size = *((unsigned*)esp);
    esp+=sizeof(unsigned);
    bool ret1 = create (file,initial_size);
    f->eax = ret1;
    break;
  case 6:;
    const char* open_file = *((char**)esp);
    if(open_file==NULL) exit(-1);
    int ret2 = open(open_file);
    f->eax = ret2;
    break;
  case 9:;
    int fd = *((int*)esp);
    esp+=sizeof(int);
    const void* buff = *((void **) esp);
    esp+=sizeof(void*);
    unsigned size = *((unsigned*)esp);
    esp+=sizeof(unsigned);
    int ret3 = write(fd,buff,size);
    f->eax = ret3;
    break;
  default:
    break;
  }

}

