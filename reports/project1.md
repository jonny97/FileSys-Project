Final Report for Project 1: Threads
===================================

## Group Members

* Ross Luo <ross.luo@berkeley.edu>
* Revekka Kostoeva <rkostoeva@berkeley.edu>
* JiannanJiang <jjn2015@berkeley.edu>
* Zane Christenson <zane.c@berkeley.edu>

# Alterations to Initial Design

## Task 1:

* When putting a thread to sleep, we no longer yield its current execution; we set its status to 'THREAD_SLEEPING' and put it on the sleep_list. We then call scheduler to pick the next thread to run. We realized after implementing this section that we could have avoided the use of the 'THREAD_SLEEPING' status and creating new functions thread_sleep() and thread_wake() but just utilizing 'THREAD_BLOCKED' status and the functions `thread_block()` and `thread_unblock()` instead. But we found 'THREAD_SLEEPING' to be a more intuitive status representation and thus decided to keep that.
 
* To ensure that two threads do not simultaneously attempt to sleep themselves, we said we would use locks. We now simply disable interrupts before putting a thread to sleep and enable interrupts at the end of the the thread_sleep function. Since both thread_wake and thread_sleep are called from functions in timer.c that are synchronous. 

## Task 2:
* First of all, we did not account for the specific cases when we should yield and reschedule in our initial design document. In our final implementation, whenever the priority of any thread changes (donation, lock_release, directly set priority), and whenever the readylists was pushed with another thread, we always yield the current running thread to make sure that, at any given point of time, the priority of the running thread is always greater or equal to any priorities of the sleeping threads.

* We added a donate_priority function in thread.c to donate priority recursively because we did not account for this case before the design review when Alon gave us an example that poked a hole in our design regarding the lack of recursive priority donation.

* The specific way to donate priority: the donate_priority is only called when the running thread is trying to acquire a lock that other thread owns. We keep a record of what lock this thread wants and donate its priority. Now, while the thread getting the donated priority is also blocked because this thread also waits for another lock, this thread will try to donate the priority to the thread that owns the lock it wants until we reach a thread that is not blocked by any locks or a thread has a higher priority than previous threads.

* Originally, we wanted to keep track of a thread's donated priority by introducing the donated_priority property in the thread structure. Instead, we simply compute a thread's effective priority each time and thus no longer need the donated_priority property. This implementation, although seems heavy as first, actually makes sense as it garuantees the correctness of calculating the priority.

* We added a lock_list Pintos list to a thread in thread.h to keep track of all locks a thread owns. Before the design review, we assumed that a thread could only hold one lock. With a lock_list, we can now support a thread owning multiple locks. The reason to add lock_list is to calculate the effective priority correctly after the thread releases some locks.

* The way to calculate the effective priority, in priority scheduling, is to loop through all the threads that are blocked by the locks owned by the thread in question, get the max effetective priority among all these threads (so here we are recursively computing the effective priority of child threads), and return the larger between this maximum and the priority of original thread.

* We did not think of semaphores and conditional variables in our initial design doc as we do not know much about them. In this implementation, whenever the condition variable signals, we will loop through all the waiters of this condition and only put the thread with the highest priority into the ready list. Whenever a sema_up is called, the semaphore will pop all the waiting threads into the ready list.


## Task 3:

* We underestimated the use of `fixed_point` numbers in our design spec. We mistakenly assumed we would be storing `recent_cpu` and `load_avg` as `int`s. Using `fixed_point`s and making sure the correct order of operation and casting of data types was a larger part of task 3 then we anticipated.

* We moved the update to recent_cpu of a thread into a new function update_recent_cpu which updates the recept cpu property for all threads using the equation provided in the spec. The function thread_get_recent_cpu no longer updates the recent cpu property and only returns 100 times the current thread's recent_cpu value. This allowed us to separate when we look at a thread's recent_cpu vs. when we actually update it.

* We also moved the update to the global load_avg out of the thread_get_load_avg function into a new update_load_avg function which updates the global load_avg as in accordance to the equation provided in the spec. This allowed us to separate when we look at the load average vs. when we need to recalculate and update it.


# Project Reflection

## What did each member do?

* Ross Luo: Initial design, debugging Task 3, Final design document.
* Revekka Kostoeva: Initial design, Task 1, debugging Task 3, Final design document.
* Jiannan Jiang: Initial design, implemented Task 2, debugging Task 3, Final design document.
* Zane Christenson: Initial design, implemented Task 1, implemented Task 3, Final design document.

## What went well?

Assembling together in person to create the initial design layed a solid foundation for the rest of the project since everyone was on the same page and knew each detail of the design. This made it easy to split up the work and to cooperate - someone who did not fully write a task could still debug it. Coding together was also helpful in that little details regarding the implementation could be ironed out once encountered - it would have been impossible to predict all changes to our initial design and it was convenient to discuss further design changes with another group member rather than relying on one brain to consider the consequences of a change.

## What could be improved?

One addition to our group programming sessions which would improve them further would be to move our sessions to a place with a screen so that when working on the same piece of code, the driver could cast their screen for the rest to see and follow live. One way to accomplish this in the future would involve utilizing a screen sharing service. Another way we could address this problem would be to meet in a room with a projector. Ideally, we would be able to cast the main coding screen for all members to follow, allowing them to pull up Pintos documentation or other util function files on their laptops for quicker reference.  

I think we are also a bit behind, or ignorant when conducting the initial document design, which is full of mistakes when looking back. Part of the reason was that the initial design doc was due well before we learned about locks, semaphores, and conditional variables. In addition, many of the details of syncronization that we were ignorant about were hard to imagine before implementation. Hopefully, next time we will know better what we are going to do when finishing the initial document.  
