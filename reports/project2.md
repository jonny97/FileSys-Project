Final Report for Project 2: User Programs
=========================================

## Group Members

* Ross Luo <ross.luo@berkeley.edu>
* Revekka Kostoeva <rkostoeva@berkeley.edu>
* Jiannan Jiang <jjn2015@berkeley.edu>
* Zane Christenson <zane.c@berkeley.edu>

# Alterations to Initial Design

## Task 1:

* We made no changes to this part of our design when implementing it.

## Task 2:

* We introduced alterations in our proposed process_relationship structure. We added `bool has_waited` to keep track of whether the parent has already called `wait()` on a child since a parent can wait on a specific child only once. We also added another semaphore and renamed our existing semaphore for clarity since we introduced a second one and now our structure contains `semaphore relationship_sema` and `semaphore child_started`. We also realized that we no longer need to keep track of the parent thread and so we removed `thread *parent` pointer. We also realized that we can retrieve the child by its tid when the parent calls `start_process()` since the thread returned by `thread_current()` at that point is the child thread. Since there is only 1 thread per process, we used tid instead of pid.

* In our final implementation of `wait()` we utilized the `has_waited` boolean inside the child's `process_relationship` structure to make sure the parent thread has not called wait on that child already, since a parent can only wait on any given child once. Then we use the `semaphore relationship_sema` as outlined in our initial design document. 

* In our final implementation of `exit()` we decided to not check if the process exiting has children for it is unnecessary since children can be orphans. Instead, we simply alter the `exit_status` of the process and exit it. 

## Task 3:

* We used a semaphore instead of a lock because it provide us with everything we need (eg. a state and a waiting list).

* We found we didn't need to disable interrupts during the fd syscalls since semaphore operations were atomic. 

* File deny write happens during loading in process.c while file allow write happens in SYS_EXIT.

* To validate passed in fd's, we make sure it is not null, is part of user virtual address, and is part of the thread's page allocation, a check we did not explicitly mention in our initial design document.

# Student Testing Report:

## Test 1: my-test-1.c

* Feature Tested: This tests determines if `filesize()` can be called on an invalid file descriptor. 

* Mechanics Overview: Pintos begins with established file descriptors only for STDIN, STDOUT, and STDERR. Therefore, an fd of `0x20101234` is invalid. We attempt to determine the file size at that fd by calling `filesize (0x20101234)`, which should either fail silently or terminate with an exit code of -1. 

* Pintos kernel output from userprog/build/tests/userprog/my-test-1.output:
```Copying tests/userprog/my-test-1 to scratch partition...
qemu -hda /tmp/MLstsZluqf.dsk -m 4 -net none -nographic -monitor null
PiLo hda1
Loading..........
Kernel command line: -q -f extract run my-test-1
Pintos booting with 4,088 kB RAM...
382 pages available in kernel pool.
382 pages available in user pool.
Calibrating timer...  78,540,800 loops/s.
hda: 5,040 sectors (2 MB), model "QM00001", serial "QEMU HARDDISK"
hda1: 175 sectors (87 kB), Pintos OS kernel (20)
hda2: 4,096 sectors (2 MB), Pintos file system (21)
hda3: 101 sectors (50 kB), Pintos scratch (22)
filesys: using hda2
scratch: using hda3
Formatting file system...done.
Boot complete.
Extracting ustar archive from scratch device into file system...
Putting 'my-test-1' into the file system...
Erasing ustar archive...
Executing 'my-test-1':
(my-test-1) begin
my-test-1: exit(-1)
Execution of 'my-test-1' complete.
Timer: 72 ticks
Thread: 3 idle ticks, 67 kernel ticks, 2 user ticks
hda2 (filesys): 61 reads, 206 writes
hda3 (scratch): 100 reads, 2 writes
Console: 869 characters output
Keyboard: 0 keys pressed
Exception: 0 page faults
Powering off...
```

* Pintos kernel output from userprog/build/tests/userprog/my-test-1.result:
`PASS`

* If the kernel had improperly implemented a process file descriptor table in which it does not check if the arguement is a valid open file, then this test would produce output from filesize indicating that the kernel returned a filesize for a non-existent file. Conceptually this does not make sense as there should be no output for a fd that correspounds to no file.

* If the kernel treated the argument passed into filesize as a file pointer rather than an integer file descriptor it could attempt to check the filesize of a invalid file pointer which could prevent this test case from exiting gracefully if the task produces an exception thus leading to a failed test.

## Test 1: my-test-2.c

* Feature Tested: This tests determines if `tell()` can be called on an invalid file descriptor. 

* Mechanics Overview: Pintos begins with established file descriptors only for STDIN, STDOUT, and STDERR. Therefore, an fd of `0x20101234` is invalid. We attempt to determine the file size at that fd by calling `tell (0x20101234)`, which should either fail silently or terminate with an exit code of -1. 

* Pintos kernel output from userprog/build/tests/userprog/my-test-2.output:
```
Copying tests/userprog/my-test-2 to scratch partition...
qemu -hda /tmp/cflclQ7mIj.dsk -m 4 -net none -nographic -monitor null
PiLo hda1
Loading..........
Kernel command line: -q -f extract run my-test-2
Pintos booting with 4,088 kB RAM...
382 pages available in kernel pool.
382 pages available in user pool.
Calibrating timer...  75,264,000 loops/s.
hda: 5,040 sectors (2 MB), model "QM00001", serial "QEMU HARDDISK"
hda1: 175 sectors (87 kB), Pintos OS kernel (20)
hda2: 4,096 sectors (2 MB), Pintos file system (21)
hda3: 101 sectors (50 kB), Pintos scratch (22)
filesys: using hda2
scratch: using hda3
Formatting file system...done.
Boot complete.
Extracting ustar archive from scratch device into file system...
Putting 'my-test-2' into the file system...
Erasing ustar archive...
Executing 'my-test-2':
(my-test-2) begin
my-test-2: exit(-1)
Execution of 'my-test-2' complete.
Timer: 73 ticks
Thread: 4 idle ticks, 67 kernel ticks, 2 user ticks
hda2 (filesys): 61 reads, 206 writes
hda3 (scratch): 100 reads, 2 writes
Console: 869 characters output
Keyboard: 0 keys pressed
Exception: 0 page faults
Powering off...
```

* Pintos kernel output from userprog/build/tests/userprog/my-test-2.result:
`PASS`

* If the kernel had improperly implemented a process file descriptor table in which it does not check if the arguement is a valid open file, then this test would produce output from tell() indicating that the kernel returned a value from a non-existent file. Conceptually this does not make sense as there should be no output for a fd that correspounds to no file.

* If the kernel treated the argument passed into tell() as a file pointer rather than an integer file descriptor it could attempt to execute tell() on an invalid file pointer which could prevent this test case from exiting gracefully if the task produces an exception thus leading to a failed test.

* If the kernel does not have synchronization implemented then calling close() and tell() on a file might encounter race conditions, thus closing the file before calling tell() and ending up calling tell() on an invalid file pointer which could prevent this test case from exiting gracefully if the task produces an exception thus leading to a failed test.

## Pintos Testing System Improvements

* One improvement we suggest is to determine a way to run a test case and have it check the result in one script, rather than having to run a pintos and then a perl command to check the output of the pintos command. 

* Another improvement we suggest is a better file structure for tests. It was annoying to have to navigate to tests/ then userprog/ to run `make check` and then to build/ to be able to run individual tests. Furthermore, we then had to navigate further to find the outputs (.output and .result) files.

# Project Reflection

## What did each member do?

* Ross Luo: Initial design, Task 3 switch skeleton, Debugging Task 2 and 3
* Revekka Kostoeva: Initial design, Task 1, Task 2, Task 3, Student Tests, User Exceptions, Final Report
* Jiannan Jiang: Debugging Task 1, 2, 3, Task 3 cleanup
* Zane Christenson: Initial design, implemented Task 1, Task 2, and Task 3, Final Report

## What went well?

* Our project design phase went pretty well as we were able to narrow down our implementation to a much better degree than last time. After the lectures on semaphores and locks, we were more knowledgeable and comfortable with them and could properly determine where we would need to employ them and how we can employ them.

* We were able to parallelize this task a bit better than the last project since we placed Task 2 and Task 3 in a switch statement.

## What could be improved?

* With the rest of the classes piling up and the midterm season upon us, we could have planned better and set deadlines for completing parts of the project to stay on route. 
 
