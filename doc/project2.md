Design Document for Project 2: User Programs
============================================

## Group Members

* Ross Luo <ross.luo@berkeley.edu>
* Revekka Kostoeva <rkostoeva@berkeley.edu>
* Jiannan Jiang <jjn2015@berkeley.edu>
* Zane Christenson <zane.c@berkeley.edu>


# Task 1: 

## Data Structures & Functions: 
We will create a single function inside `process.c` that takes in the file name string and a valid esp stack pointer.
```
void add_argv_to_stack (char *file_name, void **esp)
```
This function will be responsible for chunking the file_name into words and correctly putting them onto the processes stack. We will follow the conceptual process of putting arguments on the user programs stack outlined in section `3.1.9` of the project spec.
`add_argv_to_stack` will be called from `start_process` after the `load` function has been called because the `load` function calls `setup_stack` so we must add the arguments after the programs initial page has been created.

## Algorithms:
Our functions logic is as follows 
  1. Build char array that contains words of file_name
  2. Use **esp pointer to put strings onto stack and keep track of the addresses of where the strings start.
  3. Using the addresses found above, put them onto the stack in the correct order and endianness.
  4. Push on pointer to first of the address (this is equivalent to a pointer to our char array)
  5. Push on number of arguments
  6. Push on return address (0 suffices)

## Synchronization:
No synchronization is necessary for implementing task 1

## Rationale:
We concluded that this logic should belong its own function with a definite possibility of being broken up even further to create nice abstraction barriers. Conceptually, the logic is straight forward. The implementation will need to ensure that proper edge cases are handled like multiple whitespace.


# Task 2: 

## Data Structures & Functions:
Each function will be responsible for pulling the correct arguments off the stack, validating their correctness before executing the necessary syscall. Each syscall will also be responsible for placing any return value in the `eax` property of the `intr_frame`.
In `userprog/syscall.c` we will implement a switch statement on the syscall number. The switch will call one of the various functions we create below based on the syscall number.
* `int practice (int i)` 
  * will increment the argument i by one and return
* `void halt (void)`
  * implement using `devices/shutdown.c/shutdown_power_off()`.
* `void exit (int status)`
  * Will handle deletion of our shared process structures explained below. will also call `process_exit` in `process.c` to handle process deletion.
* `pid_t exec (const char *cmd_line)`
  * Will handle creating a parent-child process relationship struct and then will call `process_execute`
* `int wait (pid_t pid)`
  * Will call our implementation of `process_wait` in `process.c`. Explained below.


Inside `userprog/syscall.c` will create a global linked list that contains parent-child process relationships that map a relationship when a process creates a child using `exec`. The shared structure will be defined in `userprog/syscall.h` as follows:
```
struct process_relationship {
    list_elem elem,       // keeps track of the list element in the linked list
    int exit_status,      // keeps track of the childs exit process
    int alive_count,      // keeps track of the number of processes pointing at our structure (max 2)
    semaphore,            // semaphore used for witing on a child thread
    lock relationship_lock, // used to prevent the child and parent from 
    thread* child,        // pointer to child thread
    thread* parent,       // pointer to parent thread
}
```

We will also add a property in `thread.c` in our `struct thread` that will keep track of the relationship with the parent.
```
struct thread {
  ...
  process_relationship * pr,
}
```

## Algorithms:
* It is important that we ensure the validity of the user program stack pointer and arguments passed to our syscall. We will handle these various errors with the following strategy. To ensure that the user is not accessing kernal memory, we will use `is_user_vaddr` inside `thread/vaddr.h` to bounds check the address. To prevent null pointers, we will also null check before we dereference. We will use `lookup_page` inside `pagedir.c` to validate that a virtual address is not unmapped memory
  
* The algorithm for `halt` and `practice` are fairly straight forward and will mainly involved accessing the function arguments from the stack, performing some operations then returning the value in the using `eax` pointer.

* `exec`: will be called by a process and spawn a child process. We will create a `process_relationship` that keeps track of some important information that will be used in `wait`.

* `wait` will utilize the `process_relationship` struct to wait for a child. It will do this by calling `sema_down` on the semaphore property which will block the process if the child had not already called `sema_up`. Once the process is unblocked it will use the `exit_status` property in the `process_relationship` struct to return the childs exit code.

* `exit` when a process exists, it will have to check to see if it has a parent_relationship. If it does, this means that a process is a child. It will call `sema_up` which will unblock the parent if it is waiting. Whether the exiting process is a parent or a child, it will decrement `alive_count` by one regardless. If the `alive_count` reaches `0` this means that both the child and parent are dead/dying thus it is time to delete this `process_relationship` so that it does not create a memory leak.


## Synchronization:
A lock will have to be used when manipulating our shared `process_relationships` to prevent race conditions when we update the struct.

## Rationale:
The structure we chose for handling parent-child relationships holds the minimum amount of information that we need to prevent memory leaks, allow for a parent to wait for a child, and also allow for the parent to access the childs exit status. We needed some shared data structure to allow for communication between processes.


# Task 3: 

## Data Structures & Functions:
As in task 2, the switch statement in `userprog/syscall.c` on the syscall number. The switch will call one of the various functions we create below based on the syscall number.
* `void create ()`
* `void remove ()`
* `void open (`
* `void filesize ()`
* `void read ()`
* `void write ()`
* `void seek ()`
* `void tell ()`
* `void close ()`

Each function will be responsible for pulling the correct arguments off the stack, validating their correctness, and calling the correct pintos library function to handle the filesys operation. We will utilize the following pintos filesys functions in our implementation:
* `bool filesys_create (const char *name, off_t initial_size)`
* `bool filesys_remove (const char *name)`
* `struct file * filesys_open (const char *name)`
* `off_t file_length (struct file *file)`
* `off_t file_read (struct file *file, void *buffer, off_t size)`
* `off_t file_write (struct file *file, const void *buffer, off_t size)`
* `void file_seek (struct file *file, off_t new_pos)`
* `off_t file_tell (struct file *file)`
* `void file_close (struct file *file)`

## Algorithms:
We will will have to handle the translation from file descriptors to file pointers. This will be a mapping from file descripter (`int fd`) to a file pointer (`struct file *file`).  To do this, we will need to construct and maintain a file descriptor table for each process. Since Pintos has one thread per process, we can place this table inside the `thread struct` as an array with `MAX=128` as detailed in the project specs. This table will need to be cloned when a process forks. We can implement this inheritance in `thread_create` by accessing the parent thread through `running_thread()`. Opening a file will add a fd to the table in O(n) time. Closing and removing from the table will be O(1).


## Synchronization:
We will implement a single global lock as synchronization for file sys operations. We will put this lock in `userprog/syscall.c` and utilize it for each file system call that is made by a user program. This will prevent possible reads/writes/etc on the same file causing race conditions.

Additionally, we will also disable interupts during the the file sys calls to prevent a process from being interupted while it has acquired the lock on the entire file system. This means that no other process would be able to access any file while the first process has the file system lock.

We have to make sure that a currently executing program does not write/change its own executable code store in a file. To do this, we will utilize the `file_allow_write` and `file_deny_write` when process is created and destroyed.

## Rationale:
We choose to utilize as much existing functionality that already existed in pintos with the `filesys/` operations. The main difference between the api of the user program file syscalls and the pintos filesys calls is the use of the `int` vs `struct * file`. This is naturally an opportunity for us to map `int fd`'s to `struct * file`'s in our fd tables. We can modify the fd table whenever we perform `open()` and `close()`.


# Additional Questions:

1. Test Name: `sc-bad-sp.c` Test case invokes a system call with the stack pointer (%esp) set to a bad address. In line 18, takes PC, subtracts 64 MB, and move it into esp register. This is an invalid esp address becase it lies way below the code segment of that process.

2. Test Name: `sc-bad-arg.c`
In line 14, test case issues `"movl $0xbfffffffc, %%esp"` moves value `0xbfffffffc` to the esp register, setting the esop pointer to the top of the stack since `0xbfffffffc` is our `PHYS_BASE`. Next move `"mov1 %0, (%%esp)"` moves `SYS_EXIT` to the `esp` without offset, so the ststem call number goes to the top of the stack. Then when the system call is called, we look for the arguments above, but since the exit code is stored at the stack boundary, the call arguments will be stored outside the boundary, thus invalide access. We kill the user process. 

3. A test to check for filesize being called on an invalid file descriptor; should exit with code -1 for error. In the test case we call filesize on a file descriptor that we do not currently have.
      ```
      /* Tries to close an invalid fd, which must terminate with exit code -1. */

      #include <syscall.h>
      #include "tests/main.h"

      void
      test_main (void)
      {
        int bad_fd = 0x20101234;
        filesize (bad_fd);
      }
      ```

4. GDB Question( can be individually done) 
   1. Name of thread: `"main"`. Address of thread: `0xc000e000`. What other threads are present in pintos at this time?
      Other threads: `"idle"`. dumplist 
      ```
      0xc000e000 {tid = 1, status = THREAD_RUNNING, name = "main", '\000' <repeats 11 times>, stack = 0xc000ee0c "\210", <incomplete sequence \357>, priority = 31, allelem = {prev = 0xc0034b50 <all_list>, next = 0xc0104020}, elem = {prev = 0xc0034b60 <ready_list>, next = 0xc0034b68 <ready_list+8>}, pagedir = 0x0, magic = 3446325067}
      
      0xc0104000 {tid = 2, status = THREAD_BLOCKED, name = "idle", '\000' <repeats 11 times>, stack = 0xc0104f34 "", priority = 0, allelem = {prev = 0xc000e020, next = 0xc0034b58 <all_list+8>}, elem = {prev = 0xc0034b60 <ready_list>, next = 0xc0034b68 <ready_list+8>}, pagedir = 0x0, magic = 3446325067}
      ```

   2. The backtrace is: 
      ```
      #0  process_execute (file_name=file_name@entry=0xc0007d50 "args-none") at ../../userprog/process.c:32
      #1  0xc002025e in run_task (argv=0xc0034a0c <argv+12>) at ../../threads/init.c:288
      #2  0xc00208e4 in run_actions (argv=0xc0034a0c <argv+12>) at ../../threads/init.c:340
      #3  main () at ../../threads/init.c:133
      ```
      Lines of code corresponding to backtrace:
      ```
      0: process_execute (const char *file_name)
      1: process_wait (process_execute (task));
      2: a->function (argv);
      3: run_actions (argv);
      ```
     
   3. Thread Name: `args-none\000\000\000\000\000\000`. Thread Address: `0xc010a000`. Other threads: `"main"`, `"idle"`. There struct threads are:
      ```
      main => {tid = 1, status = THREAD_BLOCKED, name = "main", '\000' <repeats 11 times>, stack = 0xc000eebc "\001", priority = 31, allelem = {prev = 0xc00
      34b50 <all_list>, next = 0xc0104020}, elem = {prev = 0xc0036554 <temporary+4>, next = 0xc003655c <temporary+12>}, pagedir = 0x0, magic = 3446325067}

      idle => {tid = 2, status = THREAD_BLOCKED, name = "idle", '\000' <repeats 11 times>, stack = 0xc0104f34 "", priority = 0, allelem = {prev = 0xc000e020
      , next = 0xc010a020}, elem = {prev = 0xc0034b60 <ready_list>, next = 0xc0034b68 <ready_list+8>}, pagedir = 0x0, magic = 3446325067}

      args-none => {tid = 3, status = THREAD_RUNNING, name = "args-none\000\000\000\000\000\000", stack = 0xc010afd4 "", priority = 31, allelem = {prev = 0xc0104
      020, next = 0xc0034b58 <all_list+8>}, elem = {prev = 0xc0034b60 <ready_list>, next = 0xc0034b68 <ready_list+8>}, pagedir = 0x0, magic = 3446325067}
      ```

   4. It is created in `process_execute` on line 45
      ```
      tid = thread_create (file_name, PRI_DEFAULT, start_process, fn_copy);
      ```
   
   5. The hex address is: `0x0804870c`
   
   6. After loading the symbols, `btpagefault` cmd now gives
      ```
      _start (argc=<error reading variable: can't compute CFA for this frame>, argv=<error reading variable: can't compute CFA for this frame>) at ../../lib/user/entry.c:9
      ```
   
   7. We have not implemented argument passing yet so the the program crashes when it tries to read the arg values.








