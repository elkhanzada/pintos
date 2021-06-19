#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "threads/synch.h"

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
extern struct lock load_lock;
extern struct lock pexit_lock;
extern struct lock pwait_lock;
/* process thread */
struct process { 
	char name[16]; // DEBUG
	struct semaphore wait_sema; // force parent to wait.
	struct semaphore load_sema; // to make sure child doesn't crash during loading
	struct thread *self_t; // basically a thread, that may create other threads
	struct process *parent; // who is my dady
	struct list child_p;
	struct list_elem p_elem; // push it to my dady's child_thread
	struct list opened_f; // list of opened file.
	int n_file;
	int exit_code;
	bool waited_once;
	bool called_deny_write;

	bool had_pwait_lock;

	struct list finished_child;
	struct file *executable;
};

struct finished_process {
	int pid;
	int exit_code;
	struct list_elem finished_process_elem;
};

struct opened_file {
	struct process *opener;
	int fd; // file description (> 1)
	struct file *f_ptr;
	struct list_elem opened_file_elem;
};

#endif /* userprog/process.h */

