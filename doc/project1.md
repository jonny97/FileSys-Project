Design Document for Project 1: Threads
======================================

## Group Members

* Ross Luo <ross.luo@berkeley.edu>
* Revekka Kostoeva <rkostoeva@berkeley.edu>
* Jiannan Jiang <jjn2015@berkeley.edu>
* Zane Christenson <zane.c@berkeley.edu>

# Task 1: Alarm Clock

## Data Structures & Functions: 

* Add a new enum to `thread_status` called `THREAD_SLEEPING` inside of `thread.h`
  ```c
  enum thread_status
  {
    THREAD_RUNNING,     /* Running thread. */
    THREAD_READY,       /* Not running but ready to run. */
    THREAD_BLOCKED,     /* Waiting for an event to trigger. */
    THREAD_SLEEPING,    /* Thread sleeping until woken. */
    THREAD_DYING        /* About to be destroyed. */
  }
  ```

* Add another linked list to `thread.c` that will hold all sleeping threads in order of wake time in absolute clock `ticks`
  ```c
  static struct list sleep_list;
  ```

* Add a new function in `thread.c` called `thread_sleep` that takes in a wake-up time in absolute tick (ticks since OS boot) and puts a thread to sleep
  ```c
  void thread_sleep(int_64 wake_up_time)
  ```

* Add a new function in `thread.c` called `thread_wake` that is responsible for waking up a sleeping thread.
  ```c
  void thread_wake(struct thread* thread_to_wake)
  ```

* Modify existing function `timer_interrupt` inside `timer.c` to check at each tick if there is one or more sleeping threads that need to be woken and calls `thread_wake` if needed

* Modify existing function `timer_sleep` inside `timer.c` to put current thread to sleep by calculating wake time and calling `thread_sleep`

  
## Algorithms:
#### Main idea
The general idea behind this implementation is to store sleeping threads in a linked list where the thread at the head of the list is the the thread in the 'lightest' sleep (soonest to be woken up). The thread at the tail of the list is the thread in the deepest sleep. So at each clock tick, we check if the lightest sleep thread is ready to be woken, if it is, we wake it and also check the next sleeping thread. However, if it is not ready to woken, we don't need to check any of the other sleeping threads as they will all have an equal or later wake up time. Overall, checking our sleeping threads is `O(1) (Amortized)`.

#### Sleeping a thread
To put a thread to sleep, we yield its current execution, we change its `thread_status` to `THREAD_SLEEPING`, and insert it into our linked list `sleep_lst` in the correct order by its wake-up time. This operation takes `O(n)` time where `n` is the current number of sleeping threads. Since the sleeping thread is no longer in the `ready_lst` it will not be scheduled to run until it is woken up.

#### Waking a thread
As mentioned in our main idea, checking for a sleeping thread takes `O(1) (Amortized)`. Then to actually wake up a thread we remove it from our `sleep_lst`, change its status back to `THREAD_READY` and insert it into the back of our `read_lst`.

## Synchronization:
We must make sure that when we call `timer_sleep` in `timer.c` that we finish the process of sleeping the thread without being interrupted. If we do get interrupted the middle, we could end up with a thread that is not in any list and is lost in the void. So we must make the the sleeping process does not get interrupted during this.

Additionally, since linked lists are not thread-safe, we must make sure that two threads don't simulataneously attempt to sleep themselves and alter the `sleep_lst` at the same time as this could result in undefined behavior. A lock could prevent this from ocurring.

## Rationale:
We agreed that we must keep track of sleeping threads some how and the best way to do that was to use a new status and new list to store them. There were a couple choices for the order of this linked list, as stated above we choose the solution that opimated checking for sleeping thread to wake not putting a thread to sleep because we must check the former at every single clock tick so it is important that it is as fast as possible.

# Task 2: Priority Scheduler

## Data Structures & Functions:
To keep track of priorities, we need an array of size `PRI_MAX - PRI_MIN` so `64` items long. Each index will contain a Pintos linked list that will act as a queue of threads with the same priority. Illustrated in the pseudo-picture below.
```
ready_threads                (array of queues)
  [63] : A->B->C             (linked list)
  [62] : D
  [61] : ...
  ...
  [1] : ...
  [0] : ...
```
We check our array starting at PRI_MAX and go down in priority until we find an index that contains at least one item in the queue. We pop that thread and send it to the scheduler.

* Inside `thread.h` we will need to add a new property to the thread structure called `donated_priority`. This will be used to calculate the threads effective priority in the event that its priority gets boosted by a higher priority thread. This must be a new property so that we can retain the original priority after the donation ends.
  ```c
  struct thread
  {
    /* Owned by thread.c. */
    tid_t tid;                          /* Thread identifier. */
    enum thread_status status;          /* Thread state. */
    char name[16];                      /* Name (for debugging purposes). */
    uint8_t *stack;                     /* Saved stack pointer. */
    int priority;                       /* Priority. */
    int donated_priority                /* Contains the donated priority */
    ...
  };
  ```

* Replace `ready_lst` in `thread.c` with the array of queues structure above.
  ```c
  static struct list[PRI_MAX] ready_queues
  ```

* Modify `thread_init` to initialized `ready_queues`

* Modify `next_thread_to_run` in `thread.c` to check `ready_queues` and dequeue the next highest priority thread to run. It passes this to the scheduler.

* Inside `sync.c`, modifications must be made to parts of `lock_init`, `lock_acquire`, `lock_try_acquire`, and `lock_release`. These changes will essentially spy on locks created by threads and keep track of which thread owns which locks. These changes will be explained in greater detail in the next section. 


## Algorithms
#### Choosing the next thread to run 
  Choosing the next thread to run will require iterating through  `ready_queue` starting at the highest priority and moving down to the lowest priority. At each index, we will check if the linked list at the index is empty. If it is, we move to the next priority, else, we pop the first thread and make sure that it is not sleeping or blocked and is indeed ready to run. If it passes these conditions then the thread gets passed to the scheduler.

#### Computing the effective priority
  The priority given to a thread will dictate its position inside of our data structure. We will define a thread's effective priority as:
  ```c
  int effective_priority = MAX(thread->priority, thread->donated_priority);
  ```

#### Acquiring a Lock
  The process of acquiring a lock will have to be modified to allow for priority donation. If a thread attempts to acquire a lock and the lock is currently unowned, no further action needs to be taken. However, if acquiring the lock fails because it is currently already owned by another thread, we must take action to ensure that we don't run into synchronization issues. We will solve this in the next section.
  
#### Changing a thread’s priority
 If the thread attempting to acquire the lock has a higher priority than the one who current owns it, we might run into an issue where the blocked thread keeps getting scheduled but is indefinitely waiting for a resource to be unlocked. If this is the case, we must set a donated priority on the owner thread. We will do this by calling `set_priority` which will update `donated_priority` to:
  ```c
  thread->donated_priority = MAX(thread->donated_priority, new_priority);
  ```
  We use `MAX` so that if the thread already has a `donated_priority` that is higher than the current blocked thread, no the priority will not get lowered. `set_priority` will also re-position the thread inside our `ready_queue` since its effective priority has been changed.

#### Releasing a Lock
When a lock is released, we get a chance to see if the effective priority of a thread needs to be altered. For example, if A donated its higher priority to B so that B could finish it's processing on some resource, B's priority must be de-prioritized back to its original priority after the donation period has ended. We can track this event with the calls made by threads who are releasing the locks. B will call `release_lock` when it is finished giving us the opportunity to remove the `donated_priority` property on the thread and re-prioritize it in our queue back to its original priority.

#### Priority scheduling for conditional variables.
Since conditional variables use locks interanally for their functionality, if we handle the usage of lock in a thread-safe manor, then conditional variables should also be thread-safe.

#### Unresolvable thread blocks
In the case that A attempts to acquire a lock that B owns, we escalate B's priority however B may be sleeping or blocked itself which prevents it from running. In the event of this, we will have to set A's status to blocked. so that we can continue execution with another thread until it becomes unblocked. We will utilize the waiting list of the semaphore to store the blocked threads. Once the lock is released, we can set the waitlisted items to unblocked so they can resume execution.

## Synchronization
The current implementation is thread-safe. So our goal is to implement priority scheduling without causing any synchronization issues. Our implementation will correctly take priority into account, our only concern is that we make sure that we don't run into a situation where the highest priority thread is blocked and continues to get scheduled yielding an infinite loop of execution. The case where this would happen is if a higher priority thread, A, tries to acquire a lock that is currently ownded by a lower priority thread B. Our defense for this is (as stated above), will handle this situation by escalating the priority of B to A's priority. This ensures that B will run before A if need be. This implementation will also work for recursive dependencies. If A depends on B which depends on C, B will get escalated to As priority, then when attempting to acquire a lock that is owned by C, C will get its effective priority escalated to that of B (which is already at As). We may have to use interruption handlers to prevent synchronization issues durring our function calls listed above.

## Rationale
We choose this implementation for two reasons: it's runtime and the abstraction barrier that exists with lock creation. We merely spy on  the lock functions that get called and then perform some property changes on the thread and do some linked-list shuffling for our priority queue. This abstraction barrier allows the threads to handle all the calls to acquire/release lock and we enforce those changes as seen by the scheduler. Our runtime to donate a thread's priorty is O(64) to find the correct queue, then O(1) to insert it at the front. So overall, our handling of priority donation is O(1) constant time.

# Task 3: Multi-level Feedback Queue Scheduler

## Data Structures & Functions
* Inside `flags.h` there is the flag `FLAG_MBS` that we can use to track whether or not the advanced scheduler is enabled. We will modify our task 2 solution to use this flag before we attempt to do any priority donation.

* The data structures of this task will be the same as the `ready_queue` used in Task 2. To summarize, we have 64 double linked lists stored in an array each of which designate a given priority.

* We will need a variable to track average load times that will be initialized to 0:
  ```c
  int Load_avg = 0;
  ```

* We will also add a property to the the thread struct that will hold that threads `recent_cpu` value.

* We will add a new property called `nice` to the thread struct.

* We will implement the following functions:
  ```c
  // returns the total number of ready threads
  int thread_get_ready_threads(void);

  // calculate the load average
  int thread_get_load_avg(void);
  
  // returns the current threads nice value
  int thread_get_nice(void);
  
  // sets the current threads nice value
  int thread_set_nice(void);
  
  // gets the current threads priority
  int thread_get_priority(void);
  
  // described below
  int thread_get_recent_cpu(void);
  ```


## Algorithms
This Scheduler is all about how to choose the next thread to run, so anything besides what has been discussed below will be following implementations in task 2. We add the following functions to support advanced scheduling.

`int thread_get_ready_threads(void);`
Get the number of threads that are ready to run by summing up the number of threads in our 64 length doubly linked list. This can be done in O(1) if we keep track of the number of threads in each DLL.

`int thread_get_load_avg(void);`
Calculates the load average by the given formula: with load_avg variable which keeps track of the recent load_avg, and we can get the number of threads in ready state by calling `thread_get_ready_threads()`.
	
`int thread_get_nice(void);`
Nice is a attribute of the current thread, we can use `current_thread` then access the struct to retreieve this value.

`int thread_set_nice(void);`
this function allows us to update the niceness of a thread, and then calculate the new priority of the thread, if it is no longer of the highest priority, yield immediately.

`int thread_get_priority(void);`
By calling `thread_get_nice()` and `thread_get_recent_cpu()` to calculate priority of a thread.

`int thread_get_recent_cpu(void);`
Calculate as a function of load_avg, nice, and recent_cpu_prev. 
Recent `cpu = (2 x load_avg)(2 x load_avg + 1) x recent_cpu + nice`


#### Choosing the next thread to run:
Just pick the highest priority thread, and perform task 2 scheduler to revolve any concurrency issues. However, in this implementation, if the highest-priority queue contains multiple threads, then they run in “round robin” order: we require each thread to run a bit of the certain ticks of time, then yield, giving the other threads with the highest priority a chance to run certain ticks of time.

#### Calculating Load Average every TIMER_FREQ ticks
  Every tick, we check:
  ```c
  if (timer_ticks % TIMER_FREQ == 0) {
		thread_get_load_avg(void);
		thread_get_recent_cpu(void);
  }
  ```


## Synchronization
See we are only manipulating kernal variables and the currently running thread, there is little chance for us to run into synchronization issues. We may have to use interruption handlers to prevent synchronization issues durring our function calls listed above.

## Rationale
This solution most relies on the formulas given in the spec. This fact, paired with 

# Additional Questions

## Question 1  
OPTION 1: We create three threads A, B, C with priorities 3, 2, and 1 respectively. Thread C obtains a lock on resource R1 and we add it to our ready queue along with B and A. So our current queue looks like:
  ```
  Priority | Thread
  3          A
  2          B
  1          C
  ```
  A tries to run because it has highest priority, however it attempts to acquire a lock on resource R1 and fails, thus the thread gets blocked. Because of this, B gets executed next then C, and lastly A. 
  If our effective priorities worked, we would expect C to get boosted to A's priority allow it to finish first, followed by A, and lastly B.
  ```
  EXPECTED: C, A, B
  BUGGY PROGRAM: B, C, A
  ```


  OPTION 2: 
  Consider the following case:
  ```
  63
  62
  61
  60 A
  ```
  …
  Sleeping B,C,D
  Thread A with priority 60 gets executed, acquired the lock for A. In the meanwhile when A is running, Thread B and C wakes up with B priority 61, C priority 62. Now, if A called timer_sleep, A will go sleep, and the scheduler will try to schedule C, fail when acquiring lock of A, then try to schedule B, again fail when acquiring lock of A. Now both B and C are in waiters of sema of A. As follows:
  63
  62 C(acquired lock of A)
  61 B(acquired lock of A)
  60 A(locked A)
  …
  Sleeping D
  Then say D wakes up with priority 63, which acquires a lock of B.
  Now say A wakes up, and releases its lock, when sema_up() looking into the waiter list, it picks C because base priority of C is higher than that of B. However, if we have correctly implemented priority donation, the sema_up() should pick B instead because the effective priority of B is actually 63.

  
## Question 2
|  Timer Ticks | R(A)  | R(B)  |  R(C) | P(A) | P(B) | P(C) | thread to run |
|---|---|---|---|---|---|---|---|
| 0 | 0 | 0 | 0 | 63 | 61 | 59 | A |
| 4 | 4  | 0 | 0  |  62 | 61  | 59  | A  |
| 8 | 8 | 0 | 0 |  61 | 61  | 59  | A  |
| 12 | 12 | 0 | 0 | 60  |  61 | 59  | B  |
| 16 |  12 | 4 | 0 |  60 |  60 | 59  |  A |
| 20 | 16 | 4 |  0 |  59 |  60 | 59  | B  |
| 24 | 16 | 8 |  0 |  59 |  59 | 59  | A  |
| 28 | 20 | 8 |  0 |  58 |  59 |  59 | B  |
| 32 | 20 | 12 |  0 |  58 | 58  | 59  |  C |
| 36 |  20 | 12 |  4 | 58  | 58  | 58   | A |

## Question 3
Yes. At time tick 8,16,24, we have at least two threads has the same priority. From the spec, it is unclear how we are going to break the tie. We would like to use a round robin method but the priority is not constant. Thus, we just stick to alphabetical order( A>B>C), execute A when A is tied with other threads, execute B when B is tied with C, and B and C both have higher priorities than A.
