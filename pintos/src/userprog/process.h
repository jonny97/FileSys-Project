#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

/* A structure used to pass data from parent to child process.
 Useful for transferring current working directory, process 
 relationship, and the new processes filename and args */
struct process_inheritance {
  char *filename;
  struct process_relationship *pr;
  struct dir* cwd;
};

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
int count_args(char *file_name);
void add_args_to_stack(char *argv[], int argc, char **esp);
bool validate_filename(const char *filename);


#endif /* userprog/process.h */
