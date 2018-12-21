#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/interrupt.h"

void syscall_init (void);
void sys_exit (struct intr_frame*, int);

struct process_relationship {
	struct list_elem elem;		/* Linked list to keep track of list elements. */
	bool has_waited;
	int exit_status;	/* Child exit status */
	int alive_count; 	/* Number of processes pointing at our structure (Max 2). */
	struct lock relationship_lock; /* lock used to ensure only one thread at a time can access alive_count */
	struct semaphore relationship_sema; /* the parent will call sema down to wait on a child. */
	struct semaphore child_started;
	tid_t child_tid;
};

#endif /* userprog/syscall.h */
