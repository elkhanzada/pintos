#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "devices/shutdown.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "threads/synch.h"

#include "threads/vaddr.h"
#include "userprog/process.h"
static void syscall_handler (struct intr_frame *);
void sys_halt (void);
int  sys_write (int fd, const void *buffer, unsigned size);
bool sys_remove (const char *file);
bool sys_create (const char *file, unsigned initial_size);
int sys_read (int fd, void *buffer, unsigned size);
int sys_wait (tid_t pid);
void sys_exit (int status);
int sys_exec(char *file_name);
int sys_open(const char *file);
int sys_filesize (int fd);
void sys_seek(int fd, int pos);
unsigned sys_tell (int fd);
void sys_close (int fd);
struct opened_file *find_opened_file (struct process *p, int fd);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}
int 
get_sys_arg(int *esp, int offset) {
	return * (int *)pagedir_get_page(thread_current()->pagedir, esp + offset);
}
void 
check_uaddr(struct thread *cur, const void *uaddr) {
	if(uaddr >= PHYS_BASE || !pagedir_get_page(cur->pagedir, uaddr)) {
		sys_exit(-1);
	}
}
static void
syscall_handler (struct intr_frame *f UNUSED) 
{
 check_uaddr(thread_current (), (int *)f->esp);
 enum SYSCALL_NUMBER number = *(int*)f->esp;
  	switch (number) {
	  	case SYS_HALT:
			{
				sys_halt ();
	  			break;
			}
	  	case SYS_EXIT: 
	  		{
	  			check_uaddr(thread_current (), (int *)f->esp + 1);
	  			sys_exit(get_sys_arg(f->esp, 1));
	  			break;
	  		}
	  	case SYS_WAIT:
		  	{
		  		check_uaddr(thread_current (), (int *)f->esp + 1);
				f->eax = sys_wait(get_sys_arg(f->esp, 1));
		  		break;	
		  	}
	  	case SYS_CREATE:
	    	{
	    		check_uaddr(thread_current (), (int *)f->esp + 4);
	    		check_uaddr(thread_current (), (int *)f->esp + 5);
				f->eax = sys_create(get_sys_arg(f->esp, 4), get_sys_arg(f->esp, 5));
	  			break;
	    	}
	  	case SYS_REMOVE:
	    	{
	    		check_uaddr(thread_current (), (int *)f->esp + 1);
          		f->eax = sys_remove(get_sys_arg(f->esp, 1));
		    	break;
	    	}
	  	case SYS_OPEN:
	    	{
	    		check_uaddr(thread_current (), (int *)f->esp + 1);
				f->eax = sys_open(get_sys_arg(f->esp, 1));
	  			break;
	    	}
	  	case SYS_FILESIZE:
		    {
		    	check_uaddr(thread_current (), (int *)f->esp + 1);
		    	f->eax = sys_filesize(get_sys_arg(f->esp, 1));
		        break;
		    }
	  	case SYS_READ:
	        {
	        	check_uaddr(thread_current (), (int *)f->esp + 5);
	    		check_uaddr(thread_current (), (int *)f->esp + 6);
	    		check_uaddr(thread_current (), (int *)f->esp + 7);
	        	f->eax = sys_read (get_sys_arg(f->esp, 5), get_sys_arg(f->esp, 6), get_sys_arg(f->esp, 7));
	  			break;
	        }
	  	case SYS_WRITE:
	  		{
	  			check_uaddr(thread_current (), (int *)f->esp + 5);
	    		check_uaddr(thread_current (), (int *)f->esp + 6);
	    		check_uaddr(thread_current (), (int *)f->esp + 7);
          		f->eax = sys_write(get_sys_arg(f->esp, 5), get_sys_arg(f->esp, 6), get_sys_arg(f->esp, 7));
	  			break;
	  		}
	  	case SYS_SEEK:
		  	{
		  		check_uaddr(thread_current (), (int *)f->esp + 4);
	    		check_uaddr(thread_current (), (int *)f->esp + 5);
		  		sys_seek(get_sys_arg(f->esp, 4), get_sys_arg(f->esp, 5));
		  		//printf("seek\n"); <-- rox-child requires this
         		 break;
		  	}
		case SYS_TELL:
			{
				check_uaddr(thread_current (), (int *)f->esp + 1);
				f->eax = sys_tell(get_sys_arg(f->esp, 1));
       			 break;
			}
	  	case SYS_CLOSE:
		    {
		    	check_uaddr(thread_current (), (int *)f->esp + 1);
		    	sys_close(get_sys_arg(f->esp, 1));
		  		break;
		    }
		case SYS_EXEC:	
			{
				check_uaddr(thread_current (), (int *)f->esp + 1);
				f->eax = sys_exec(get_sys_arg(f->esp, 1));
				break;
			}
	  	default:
		  	{
	    
      		}
	  }
}
void 
sys_close (int fd)
{
	lock_acquire(&pwait_lock);
	struct thread *cur = thread_current ();
	struct opened_file *openedFile = find_opened_file(cur->self_p, fd);
	if(!openedFile)
	{
		lock_release(&pwait_lock);
		sys_exit (-1);
	} else {
		file_close(openedFile -> f_ptr);
		list_remove(&openedFile -> opened_file_elem);
	}
	lock_release(&pwait_lock);
}

unsigned 
sys_tell (int fd)
{
	lock_acquire(&pwait_lock);
	struct thread *cur = thread_current ();
	struct opened_file *openedFile = find_opened_file(cur->self_p, fd);
	int res = 0;
	if(!openedFile) {
		res = -1;
	}
	else  {
		res = file_tell(openedFile->f_ptr);
	}
	lock_release(&pwait_lock);
	return res;
}

void
sys_seek(int fd, int pos)
{
	lock_acquire(&pwait_lock);
	struct thread *cur = thread_current ();
	struct opened_file *openedFile = find_opened_file(cur->self_p, fd);
	if(!openedFile) {
		lock_release(&pwait_lock);
		sys_exit(-1);
	}
	else  {
		file_seek(openedFile->f_ptr, pos);	
		lock_release(&pwait_lock);
	}
}
void
sys_exit (int status) 
{
	struct process *p = thread_current () -> self_p;
	if(p) 
		p -> exit_code = status;
        if(status == EXIT_CODE_PAGE_FAULT)
          printf("%s: exit(%d)\n",thread_current () ->name, -1);
	else
      	  printf("%s: exit(%d)\n",thread_current () ->name, status);
	thread_exit ();
}
void 
sys_halt (void) 
{
	shutdown_power_off();
}

int 
sys_filesize (int fd) 
{
	lock_acquire(&pwait_lock);
	int res = 0;
	struct opened_file *openedFile = find_opened_file(thread_current ()->self_p, fd);
	if(!openedFile)
		res = -1;
	else
		res = file_length(openedFile->f_ptr);
	lock_release(&pwait_lock);
	return res;
}

int 
sys_open(const char *file)
{
	check_uaddr(thread_current (), file);
	lock_acquire(&pwait_lock);
	struct file *f = filesys_open(file);
	int res = 0;

	if(f == NULL) {
		res = -1;
	} else 
	{
		struct thread *cur = thread_current ();
		int desc = cur -> self_p -> n_file;
		struct opened_file *openedFile = malloc(sizeof(*openedFile));
		openedFile->f_ptr = f;
		openedFile->fd = desc;
		list_push_back(&cur->self_p->opened_f, &openedFile->opened_file_elem);
		cur -> self_p -> n_file ++;
		res = desc; 
	}
	lock_release(&pwait_lock);
	return res;
}

struct opened_file *find_opened_file (struct process *p, int fd) 
{
	struct list_elem *e;
	for(e = list_begin(&p->opened_f); e != list_end(&p->opened_f);
		e = list_next(&p->opened_f)) 
	{
		struct opened_file *openedFile = list_entry(e, 
			struct opened_file, opened_file_elem);
		if(openedFile->fd == fd) 
		{
			return openedFile;
		}
	}
	return NULL;
}

int
sys_read (int fd, void *buffer, unsigned size)
{
	check_uaddr(thread_current (), buffer);
	lock_acquire(&pwait_lock);
	int bytes_read = 0;
	if(fd == 0) {
		int i = 0;
		for(; i < size; ++i) {
			*((char*)buffer + i) = input_getc();
		}
		bytes_read = size;
	} else {
		struct thread *cur = thread_current ();
		struct opened_file *openedFile = find_opened_file(cur->self_p, fd);
		if(openedFile == NULL) {
			bytes_read= -1;
		}
		else {
			bytes_read= file_read(openedFile->f_ptr, buffer, size);
		}
	}
	lock_release(&pwait_lock);
	return bytes_read;
}
int 
sys_write (int fd, const void *buffer, unsigned size)
{
	check_uaddr(thread_current (), buffer);
	lock_acquire(&pwait_lock);
	int res = 0;
	if(fd == 1)
	{
		putbuf(buffer, size);
		res = size;
	} else {
		struct opened_file *openedFile = find_opened_file(
			thread_current ()->self_p, fd);
		if(!openedFile)
			res = -1;
		/*printf("\n\n\nhere %s %d\n\n\n", thread_current ()->name, 
			thread_current ()->self_p->executable==NULL);*/
		res = file_write(openedFile->f_ptr, buffer, size);
	}
	lock_release(&pwait_lock);
	return res;
}
bool 
sys_remove (const char *file)
{
	check_uaddr(thread_current (), file);
	return filesys_remove(file);
}
bool 
sys_create (const char *file, unsigned initial_size)
{
	check_uaddr(thread_current (), file);
	return filesys_create(file, initial_size);
}
int 
sys_wait (tid_t pid)
{
	int ret = process_wait(pid);	
	//printf("\n\nsys_wait: %s tid %d vs pid %d returns %d\n\n\n", 
	//	thread_current ()->name, thread_current ()->tid, pid, ret);
	return ret;
}
int 
sys_exec(char *file_name)
{
	//printf("\n\n from %s, tid=%d, exec %s\n\n\n", 
	//	thread_current()->name,
	//	thread_current()->tid,
	//	file_name);
	check_uaddr(thread_current(), file_name);

	return process_execute(file_name);

	/*
	char *save_ptr;
	char *name = calloc (1, strlen(file_name)+1);
	strlcpy(name, file_name, strlen(file_name) + 1);
	name = strtok_r(name, " ", &save_ptr);
	struct file *f = filesys_open(name);  //check whether the file exists. 
	free(name);
	if (f) 
	{
		file_close(f);
		return process_execute(file_name);
	} else 
	{
		return -1;
	}*/
}


