#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/init.h"

static int file_descriptor = 1;
static struct file* my_file = NULL;
static int cur_status = -1;
static int cur_pid = -1;
static void syscall_handler (struct intr_frame *);
void halt(void){
  shutdown_power_off();
}
void exit(int status){
  cur_status = status;
  cur_pid = thread_tid();
  printf("%s: exit(%d)\n",thread_name(),status);
  thread_exit();
}
int exec (const char* cmd){
    int tid = process_execute(cmd);
    struct thread* child = findThread(tid);
    if(child==NULL) return -1;
    while(!child->isExiting){
      thread_yield();
    }
    return tid;
}

int wait(int pid){
  struct thread* child = findThread(pid);
  if(child==NULL)if(cur_pid==pid)return cur_status;
  if(thread_current()->child_tid!=pid||child==NULL) return -1;
  while (!child->isExiting)
  {
    thread_yield();
  }
  return cur_status;
}
int write(int fd,const void *buff, unsigned size){
  if(fd==1){
      putbuf(buff,size);
      return size;
  }else
  {
    return file_write(my_file,buff,size);

  }
  
}
 bool create (const char *file, unsigned initial_size){
    return filesys_create(file,initial_size);
 }

bool remove (const char *file){
  return filesys_remove (file);
}


int open (const char *file)
{
  my_file =  filesys_open(file);
  if(my_file!=NULL) {file_descriptor+=1; return file_descriptor;}
  else return -1;
}
int read (int fd, void *buffer, unsigned size){
  return file_read(my_file, buffer, size);
}

int filesize(int fd){
  return file_length(my_file);
}

void seek (int fd, unsigned position) {
  file_seek(my_file,position);
}

unsigned tell (int fd) {
  return file_tell(my_file);
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
  case 2:;
    const char* cmd = *((char**)esp);
    esp+=sizeof(char*);
    int pid_exec = exec (cmd);
    f->eax = pid_exec;
    break;
  case 3:;
    int pid_wait = *((int*)esp);
    esp+=sizeof(int);
    f->eax = wait(pid_wait);
    break;
  case 4:;
    const char* create_file = *((char**)esp);
    if(create_file==NULL) exit(-1);
    esp+=sizeof(char*);
    unsigned initial_size = *((unsigned*)esp);
    esp+=sizeof(unsigned);
    bool ret_create = create (create_file,initial_size);
    f->eax = ret_create;
    break;
  case 5:;
    const char* remove_file = *((char**)esp);
    if(remove_file==NULL) exit(-1);
    esp+=sizeof(char*);
    bool ret_remove = remove(remove_file);
    f->eax = ret_remove;
    break;
  case 6:;
    const char* open_file = *((char**)esp);
    if(open_file==NULL) exit(-1);
    int ret_open = open(open_file);
    f->eax = ret_open;
    break;
  case 7:;
    int fd_size = *((int*)esp);
    esp+=sizeof(int);
    f->eax = filesize(fd_size);
    break;
  case 8:;
    int fd_read = *((int*)esp);
    esp+=sizeof(int);
    const void* buff_read = *((void**)esp);
    //printf(" size %u ", *buff_read);
    esp+=sizeof(void*);
    unsigned size_read = *((unsigned*)esp);
    int ret_read = read (fd_read, buff_read, size_read);
    f->eax = ret_read;
    break;
  case 9:;
    int fd_arg = *((int*)esp);
    esp+=sizeof(int);
    const void* buff_arg = *((void **) esp);
    esp+=sizeof(void*);
    unsigned size_arg = *((unsigned*)esp);
    esp+=sizeof(unsigned);
    int ret_arg = write(fd_arg,buff_arg,size_arg);
    f->eax = ret_arg;
    break;
  case 10:;
    int fd_seek = *((int *)esp);
    esp+=sizeof(int);
    unsigned position = *((unsigned*)esp);
    esp+=sizeof(unsigned);
    seek(fd_seek,position);
    break;
  case 11:;
    int fd_tell = *((int*)esp);
    esp+=sizeof(int);
    unsigned ret_tell = tell(fd_tell);
    f->eax = ret_tell;
    break;
  default:
    break;
  }

}

