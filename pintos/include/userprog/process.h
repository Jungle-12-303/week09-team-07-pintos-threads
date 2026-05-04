#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include <stdbool.h>
#include "threads/thread.h"
#include "threads/synch.h"

struct file;

struct child_status {
	tid_t tid;
	int exit_status;
	bool waited;
	bool load_done;
	bool load_success;
	int ref_cnt;
	struct semaphore load_sema;
	struct semaphore exit_sema;
	struct list_elem elem;
};

tid_t process_create_initd (const char *file_name);
tid_t process_fork (const char *name, struct intr_frame *if_);
int process_exec (void *f_name);
int process_wait (tid_t);
void process_exit (void);
void process_user_init (struct thread *t);
void process_exit_with_status (int status) NO_RETURN;
void process_activate (struct thread *next);

int process_add_file (struct file *file);
struct file *process_get_file (int fd);
bool process_close_file (int fd);
void process_close_all_files (void);
bool process_duplicate_fds (struct thread *dst, struct thread *src);
struct child_status *process_find_child (tid_t tid);
void child_status_release (struct child_status *cs);

#endif /* userprog/process.h */
