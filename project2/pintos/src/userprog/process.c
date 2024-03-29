#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

#include "filesys/inode.h"

static thread_func start_process NO_RETURN;
static bool load (const char *cmdline, void (**eip) (void), void **esp);

struct list_elem *find_child_p(struct process* p, int tid)
{
  struct list *p_list = &p->child_p;
  struct list_elem *e;
  for(e = list_begin(p_list); e != list_end(p_list); 
    e = list_next(e))
  {
    struct process *proc = list_entry(e, struct process, p_elem);
    if(proc->self_t->tid == tid) {
      return e;
    }
  }
  return NULL;
}

struct list_elem *find_finished_child_p(struct process* p, int pid)
{
  struct list *p_list = &p->finished_child;
  struct list_elem *e;
  for(e = list_begin(p_list); e != list_end(p_list); 
    e = list_next(e))
  {
    struct finished_process *proc = list_entry(e, 
      struct finished_process, finished_process_elem);
    if(proc->pid == pid) {
      return e;
    }
  }
  return NULL;
}

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *file_name) 
{
  //lock_acquire(&pwait_lock);
  char *fn_copy;
  tid_t tid;

  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page (0);
  if (fn_copy == NULL)
    return TID_ERROR;
  strlcpy (fn_copy, file_name, PGSIZE);


  /* this is because my process doesn't like having a long name*/
  char *prog_name, *save_ptr;
  char *file_name_copy = malloc(sizeof(char) * (strlen(file_name) + 1));
  strlcpy(file_name_copy, file_name, strlen(file_name) + 1);
  prog_name = strtok_r(file_name_copy, " ", &save_ptr);
  
  /* Create a new thread to execute FILE_NAME. */
  tid = thread_create (prog_name, PRI_DEFAULT, start_process, fn_copy);
  free(file_name_copy);
  if (tid == TID_ERROR) {
    //lock_release(&pwait_lock);
    palloc_free_page (fn_copy); 
  } else 
  {
  	// create successfully
    struct list_elem *child_p_elem = find_child_p(thread_current()->self_p, tid);
    if(child_p_elem == NULL) 
    {
      return -1;
    }
    struct process *child_p = list_entry(
      child_p_elem,
      struct process,
      p_elem
      );
    //lock_release(&pwait_lock);
    sema_down(&thread_current()->self_p->load_sema);
    if(child_p->exit_code == EXIT_CODE_LOAD_FAIL)
    {
      return -1;
    }
  } 
  return tid;
}

/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *file_name_)
{
  //lock_acquire(&pwait_lock);
  char *file_name = file_name_;
  struct intr_frame if_;
  bool success;

  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;

  if_.eflags = FLAG_IF | FLAG_MBS;
  
  success = load (file_name, &if_.eip, &if_.esp);
  if(!success && thread_current()->self_p->exit_code) {
    thread_current()->self_p->exit_code = EXIT_CODE_LOAD_FAIL;
  }
  //lock_release(&pwait_lock);
  sema_up(&thread_current()->self_p->parent->load_sema);
  
  /* If load failed, quit. */
  palloc_free_page (file_name);
  if (!success) 
    thread_exit ();

  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int
process_wait (tid_t child_tid UNUSED) 
{
  /*if(thread_current()->self_p->had_pwait_lock == true) {
    if(thread_current()->self_p->exit_code != EXIT_CODE_INIT) {
      return thread_current()->self_p->exit_code;
    } else {
      return -1;
    }
  }*/
  // if(lock_held_by_current_thread (&pwait_lock)) {
  //   printf("\n\t\tprocess_wait: Thread already own pwait_lock: %s %d\n\n",thread_current()->name, thread_current()->tid);
  // } else {
  //   printf("\n\t\tprocess_wait: Thread to acquire pwait_lock: %s %d\n\n",thread_current()->name, thread_current()->tid);
  // }
  //lock_acquire(&pwait_lock);
  thread_current()->self_p->had_pwait_lock = true;
  struct list_elem *child_p_elem = find_child_p(thread_current()->self_p, child_tid);
    if(child_p_elem == NULL) 
    {
      // child-simple runs so fast it's gone when
      // process_wait(child-simple) is called.
      struct list_elem *e = find_finished_child_p(thread_current()->self_p, child_tid);
      if(e == NULL) {
        //lock_release(&pwait_lock);
        return -1;
      } else {
        struct finished_process *fp = list_entry(e, struct finished_process, finished_process_elem);
        int exit_code = fp->exit_code;
        list_remove(e);
        free(fp);
        //lock_release(&pwait_lock);
        return exit_code;
      }
    }
    struct process *child_p = list_entry(
      child_p_elem,
      struct process,
      p_elem
      );
  // force thread_current () to be blocked
  //printf("\n\n%s forces block %s\n\n\n", child_p->name, thread_current ()->name);
  //lock_release(&pwait_lock);
  if(!child_p->waited_once)
  {
    child_p->waited_once = true;
    sema_down(&(child_p->wait_sema));
    struct list_elem *e = find_finished_child_p(thread_current()->self_p, child_tid);
    if(e != NULL) {
        struct finished_process *fp = list_entry(e, struct finished_process, finished_process_elem);
        int exit_code = fp->exit_code;
        list_remove(e);
        free(fp);
    }
    list_remove(&child_p->p_elem); 
    return child_p->exit_code;
  } else
  {
    list_remove(&child_p->p_elem); 
    return -1;
  }
}

/* Free the current process's resources. */
void
process_exit (void)
{
  //printf("\n\t\t%s\n\n",thread_current()->name);
  // if(lock_held_by_current_thread (&pwait_lock)) {
  //   printf("\n\t\tprocess_EXIT: Thread already own pwait_lock %s %d\n\n",thread_current()->name, thread_current()->tid);
  // } else {
  //   printf("\n\t\tprocess_EXIT: Thread to acquire pwait_lock: %s %d\n\n",thread_current()->name, thread_current()->tid);
  // }
  //lock_acquire(&pwait_lock);
  
  
  struct thread *cur = thread_current ();
  uint32_t *pd;
  if(cur->self_p->exit_code == EXIT_CODE_PAGE_FAULT) {
    cur->self_p->exit_code = -1; 
    if(cur->self_p->called_deny_write == true)
      file_allow_write(cur->self_p->executable);
  } else 
  if(cur->self_p->called_deny_write == true)
  {
    file_close(cur->self_p->executable);
  }
  if(cur->self_p->exit_code == EXIT_CODE_INIT) {
    cur->self_p->exit_code = -1;
  }
  if(cur->self_p->parent != NULL) // exclude root process.
  {
	  sema_up(&cur->self_p->wait_sema);

    struct finished_process* fp = malloc(sizeof(*fp));
    fp->exit_code = cur->self_p->exit_code;
    fp->pid = cur->tid;
    list_push_back(&cur->self_p->parent->finished_child, &(fp->finished_process_elem));
  }
//  file_close(cur->self_p->executable);
  /*close all files it opened*/
  while(!list_empty(&cur->self_p->opened_f))
  {
    struct list_elem *e = list_pop_back(&cur->self_p->opened_f);
    struct opened_file *openedFile = list_entry(e, 
      struct opened_file, opened_file_elem);
    file_close(openedFile->f_ptr);
    list_remove(e);
    free(openedFile);
  }
  
  //lock_release(&pwait_lock);
  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = cur->pagedir;
  if (pd != NULL) 
    {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      cur->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp, char *file_name, char *save_ptr, int argc);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *file_name, void (**eip) (void), void **esp) 
{
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;
  /* additional variables to extract file name & args */
  char *prog_name, *save_ptr, *arg;
  int argc = 0;
  bool white_space = false;
  char *file_name_copy = malloc(sizeof(char) * (strlen(file_name) + 1));
  strlcpy(file_name_copy, file_name, strlen(file_name) + 1);
  prog_name = strtok_r(file_name_copy, " ", &save_ptr);
  i = 0;
  for(; i < strlen(file_name); ++i) 
  {
    if(file_name[i] == ' ') {
      if(!white_space) 
      {
        white_space = true;
        ++argc;
      }
    } else {
      white_space = false;
    }
  }
  lock_acquire(&pwait_lock);

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL) 
    goto done;
  process_activate ();

  /* Open executable file. */
  file = filesys_open (prog_name);
  if (file == NULL) 
    {
      printf ("load: %s: open failed\n", file_name);
      goto done; 
    }
  file_deny_write(file);
  t->self_p->called_deny_write = true;

  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
    {
      printf ("load: %s: error loading executable\n", file_name);
      goto done; 
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else 
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }

  /* Set up stack. */
  if (!setup_stack (esp, prog_name, save_ptr, argc + 1))
    goto done;

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;

 done:
  t->self_p->executable = file; // store own exe file.
  free(file_name_copy);
  lock_release(&pwait_lock);
  /* We arrive here whether the load is successful or not. */
  //file_close (file);
  return success;
}

/* load() helpers. */

static bool install_page (void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
    return false; 

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file)) 
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) 
    return false; 

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
  
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      /* Get a page of memory. */
      uint8_t *kpage = palloc_get_page (PAL_USER);
      if (kpage == NULL)
        return false;

      /* Load this page. */
      if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
        {
          palloc_free_page (kpage);
          return false; 
        }
      memset (kpage + page_read_bytes, 0, page_zero_bytes);

      /* Add the page to the process's address space. */
      if (!install_page (upage, kpage, writable)) 
        {
          palloc_free_page (kpage);
          return false; 
        }

      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
    }
  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
/* Example:
Address     Name            Data        Type
0xbffffffc  argv[3][...]    "bar\0"     char[4]
0xbffffff8  argv[2][...]    "foo\0"     char[4]
0xbffffff5  argv[1][...]    "-l\0"      char[3]
0xbfffffed  argv[0][...]    "/bin/ls\0" char[8]
-----------^ these things put in any order---------
0xbfffffec  word-align      0           uint8_t
0xbfffffe8  argv[4]         0           char *
0xbfffffe4  argv[3]         0xbffffffc  char *
0xbfffffe0  argv[2]         0xbffffff8  char *
0xbfffffdc  argv[1]         0xbffffff5  char *
0xbfffffd8  argv[0]         0xbfffffed  char *
0xbfffffd4  argv            0xbfffffd8  char **
0xbfffffd0  argc            4           int
0xbfffffcc  return address  0           void (*) ()
*/
static bool
setup_stack (void **esp, char *file_name, char *save_ptr, int argc) 
{
  uint8_t *kpage;
  bool success = false;

  kpage = palloc_get_page (PAL_USER | PAL_ZERO);
  if (kpage != NULL) 
    {
      success = install_page (((uint8_t *) PHYS_BASE) - PGSIZE, kpage, true);
      if (success) 
      {
        *esp = PHYS_BASE;
        /* Helper variables */
        char *arg = NULL;
        char **argv = malloc(sizeof(char *) * (argc + 2)); // stores address of these args.
        int i = 0;
        argc = 0;
        /* WRITE EVERY THING TO STACK */
        // write app name
          //printf("%s\n",file_name);
	  *esp -= strlen(file_name) + 1;
          memcpy(*esp, file_name, strlen(file_name) + 1);
          argv[0] = *esp;
        // write arguments
          while(arg = strtok_r(NULL, " ", &save_ptr)) 
          {
            *esp -= strlen(arg) + 1;
            memcpy(*esp, arg, strlen(arg) + 1);
	    //printf("%s\n",*esp);
            argv[++argc] = *esp;
          }
        // last one is NULL 
        argv[++argc] = NULL;
        // word-align
        *esp -= (unsigned)*esp % 4;
        //addresses (*esp) of NULL + args + app_name
        for(i = argc; i >= 0; --i)
        {
          *esp -= 4;
	  //if(i < argc) printf("%s\n",argv[i]);
          *((int *)*esp) = argv[i];
        }
        //starting address of the args array that has been pushed above.
        *esp -= sizeof(char**);
        *((int *)*esp) = (int)(*esp + sizeof(char**));
        // argc
        *esp -= 4;
        *((int *)*esp) = argc;
        //NULL return address
        *esp -= 4;
        *((int *)*esp) = 0;
	//printf("%d\n", argc);
      }
      else
        palloc_free_page (kpage);
    }
  return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}

