Design Document for Project 3: File System
==========================================

## Group Members

* Ross Luo <ross.luo@berkeley.edu>
* Revekka Kostoeva <rkostoeva@berkeley.edu>
* Jiannan Jiang <jjn2015@berkeley.edu>
* Zane Christenson <zane.c@berkeley.edu>


# Task 1: Buffer Cache

## Data Structures & Functions:
```
inode.c

#define CACHE_SIZE 64

int hand;

struct cached_block BLOCK_CACHE[CACHE_SIZE];
struct cached_block {
    struct inode* pointer;
    struct inode data;
    int clock;
    int valid;
    int dirty;
    lock block_lock;
};
```

Functions:
```
void cache_read_block(struct cache_block *block)
block_address_load ()
int block_address_evict()
bool bring_block_to_cache(int i)// bring a block to cache indexed at i
```


## Algorithms:
We plan on implementing a 64 item fully-associative cache. We will use an array of size 64 to store the `cache_block` structure defined above that will use clock for its replacement policy. The write policy of the cache will be write-back which will occur when:
1) The block is evicted from the cache
2) The device is shutdown. We will use the `filesys_done` hook for tracking this event.


Everytime we access a block of the cache, we will increment the clock bit by 1.

Clock replacement will be implemented as follows:
1) global `int hand` will be used to track the clock hand and point to a specific element of the cache array
2) `int clock` inside each cache entry will be used to mark whether it is newly replaced (1) or eligible for eviction (0)

```
block_address_load (){
	When we are trying to find an inode block in the cache, we compare the pointer of the inode with the 64 inode entries in the cache. If there is one such inode, return inode
	If there is no such inode, we need to evict one block before we can load. So, we try to acquire the lock for eviction:
	while ( inode_not_in_cache ){
		acquire_lock_to_evict()
	}
	if (inode_in_cache){
		release lock, return the block
	}
	else: call block_address_evict()
		   bring_block_to_cache()
	release eviction lock
}

block_address_evict (){
	some_thread_idle = true
	while(true){
		while (some_thread_idle){
			some_thread_idle = false
	          		loop through all cache blocks, starting from the hand:
				if this block is being using, continue
				else: 
					some_thread_idle = true
					decrement its clock bit, 
					if the clock bit is 0, 
				    	  get the writing access to the file that owns this block, so no other thread can read/write this file
		     			  evict this block by writing all the data into the disk (this may be done in the background)  
					  return the index
					else: continue			
		}
		// if all threads are being used, block the current thread, and wait for some block to be idle
		thread_block
	}
}

bool bring_block_to_cache(int i){
	acquire the block_lock for cache at index i
	copy the data from the disk into the cache block.
	release block_lock
	return success
}
```

## Synchronization:

* We have a global cache for all processes. 

* We do not allow multiple evictions from the cache at any moment. Thus, we will have a lock for the eviction function. In addition, during eviction, we must acquire a write lock for the file we are evicting which will prevent read accesses. Processes will be free to acquire read locks to other files in the cache at this point. 

* When evicting, the process will first look through the cache with clock hand to try to find a block to evict 1.If there is such block, the eviction function will acquire the evicting previledge of the cache, write access to the file,and evict the block in the cache 2. If these is no such block, the process has to wait.



## Rationale:

* We need a cache to intercept calls whenever inodes are needed by the inode functions such as inode_read, inode_write, inode_remove. We decided on a fully associate write-back cache with a clock replacement policy - associative because we need to accomodate a replacement policy. Each cache entry needs bits to represent whether it's valid or dirty to accomodate write-backs and replacements. We also need a clock bit to accomodate the replacement policy.

# Task 2: Extensible Files

## Data Structures & Functions:

```
struct inode_disk
  {
    block_sector_t direct[12];          /* 12 direct pointers */
    block_sector_t indirect;            /* singly indirect pointer
    block_sector_t doubly indirect;     /* doubly indirect pointer
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
    uint32_t unused[113];               /* Not used. */
  };

bool inode_create (block_sector_t, off_t length);
off_t inode_read_at (struct inode *, void *, off_t size, off_t offset);
off_t inode_write_at (struct inode *, const void *, off_t size, off_t offset);
static block_sector_t byte_to_sector (const struct inode *inode, off_t pos)
```

## Algorithms:


`bool inode_create (block_sector_t, off_t length)`: we will create blocks sequentially, until we have enough blocks for the length.

`off_t inode_read_at(struct inode *, void *, off_t size, off_t offset)`: 
We would check if the inode is in the cache, if not, pull the inode into the cache (resolving evicting if necessary).
Second, we will first check if the process is allowed to read the inode. If not, this thread will be blocked and will be alerted when the inode is already. 
Then we trace through the offset and read the block that contains the offset.

`off_t inode_write_at (struct inode *, const void *, off_t size, off_t offset)`:
Check if the inode is in the cache, if not, pull the inode into the cache (resolving evicting if necessary).
Second, we will first check if the process is allowed to write the inode (no one is reading, writing, waiting to read the inode). If not, this thread will be blocked and will be alerted when the inode is already. 
Change dirty bit to 1
Then we trace through the offset and create the data block with all 0 if any of the data block is not allocated.
The detailed design of this function will heavily resemble the solution of discussion 11's resize functions. This function carefully takes care about the cases when the disk is full and we cannot allocate new disks. The function can always rollback to the original state after the file system exhaustion.

`inumber (int fd)` will use our `struct fd_obj` (discussed in Task 3) to return the unique inode number.

## Synchronization:
* We will have one pair of reading-writing condition variables for each inode. Basically, reading an inode has high priority over write, reading can be shared, but when writing, the process need the exclusive right, as we did in lecture
* When reading an inode(both file and directory), We need to lock the inode from any process wants to write it, but allow simultanenous reading.
* When writing an inode(both file and directory), We need to lock the inode from any process wants to read or write it.
* When removing a file(or directory) from a directory, we are essentially equivalent to writing every files in the directory, this file, and parent directory. We need "writing" access to all these files.
* remove/delete needs the writing access of the file or directory

## Rationale:

There are extensions for this part of the project.
1) We need to support files up to 8 MiB
2) We need to prevent external fragmentation by allocating non consecutive blocks.
3) We must be able to dynamically increase disk allocation as we extend files we write

To do this, we need to extend our inode_disk struct to include 12 direct pointers and 2 indirect pointers, up from 1 direct pointer. This allows us to reference enough disk sectors up to the 8MiB requirement. This also means we can reference non-contiguous sectors with the indirect pointers.

We have to change every function in `inode.c` that originally referenced the original direct pointer, start, to now be able to reference a variable number of appropriate pointers. Specifically, these are `inode_create`, `inode_read_at`, `inode_write_at`, and `byte_to_sector`.

* Now that we have a variable number of pointers, we need logic to decide how to dynamically allocate more disk sectors with free-map-allocate. We can either allocate 1 block size at a time or do some kind of binary search to find the largest free contiguous sector at a time. We know each file block is 512 B.

* If our file length is between 0 to 512*12 = 6 KB, we can determine how many direct pointers to allocate through with round_up(length/512). 

* If it is between 6KB and (512/4 * 512B) + 6KB = 70 KB, we have to assign all the direct pointers, plus assign round_up((length-6KB)/512B) of the pointers in the indirect pointer.

* If it is between 70 KB and 8 MiB, we do all of the above plus assign the necessary direct pointers through the doubly indirect pointer.


# Task 3: Subdirectories 

## Data Structures & Functions: 
* `#define MAX_PATH_LEN 512` defines a max file path limit
* Add `char[]` property `cwd` to track each processes current working directory.
```
struct thread {
    ...
    char cwd[MAX_PATH_LEN];
    ...
};
```

* We will create a new struct responsible for storing a file descriptor object
```c
struct fd_obj {
    struct file* file_ptr;
    struct dir* dir_ptr;
    bool is_dir;
}
```

* We will modify the `struct dir_entry` to help us recurse.
```c
struct dir_entry
  {
      ...
      bool is_dir;
      ...
  };
```

* Add synchronization variables to inodes to allow for concurrent reading and writing.
```c
struct inode
{
    ...
    int AW; /* active writers */
    int WW; /* waiting writers */
    int AR; /* actice readers */
    int WR; /* waiting readers */
    lock rwlock; /* lock for critical read and write sections */
    condition okToRead; /* conditional variable used to signal */
    condition okToWrite; /* conditional variable used to signal */
    ...
}
```

## Algorithms:
### Update threads fd table (project 2)
We will change our fd table from going from `int` to `file*` to a new mapping `int` to `struct fd_obj`. This will allow us to handle file descriptors of both file and directory type. Depending on the type, we will call separate functions for file i/o like `open`.

### Supporting sub-directories
Each `dir_entry` can now be a file or a directory. We will transform the `lookup` function to recurse on the tokens of a path `"path/to/entry"` using `is_dir` to figure out if the entry leads to another directory.

### Handling `.` and `..` directories
We will add these directory entries to each new directory created. `.` will have parent directory pointing to itself, while `..` has a pointer to its parent.

### Tracking CWD
We plan on setting a reasonable limit on the path length files and directories. Currently we believe that `512` is a good guess for `MAX_PATH_LEN`. We will store the current working directory for each process inside the thread struct. When a process spawns a child, the current working directory needs to passed to child. We will modify our project 2 `exec` call to allow this.

### Validating absolute and relative paths
For some `char * path`, we will tokenize the path `"path/to/file_or_dir"` into sections and traverse our `dir_entry`s recursively starting at root. If we run into a non-existent path, we then append the current working directory to the process `"cwd/path/to/file_or_dir"` and try again.

### Update project 2 syscalls
We will update our i/o syscalls to prevent removing, reading, and writing of directories by user programs as this should only be done through `chdir`, `mkdir`, and modified by the kernel. We prevent these operations on `fd` that correspond to directories.

### Recursively free sectors (dir_entries)
When a file is removed we must free the `dir_entry`. For deleting directories, we need to recursively free and remove all subdirectories and files.

### Directory Deletion
We will not allow removing directories if they have files within them, it should be an easy check. 
We will allow deleting the directory of other processes `cwd`. Our current scheme will have any file i/o on these `cwd` fail. 

## Synchronization:
### Removing our global lock from project 2
* We will remove our global lock and replace it with reader/writer synchronization model as seen in lecture. We will use the variables in the inode and handle this logic in `inode.c`
* We will have one condition variable for each inode. Basically, reading an inode has high priority over write, reading can be shared, but when writing, the process need the exclusive right, as we did in lecture
* When reading an inode(both file and directory), We need to lock the inode from any process wants to write it, but allow simultanenous reading.
* When writing an inode(both file and directory), We need to lock the inode from any process wants to read or write it.
* When removing a file(or directory) from a directory, we are essentially writing every files in the directory, this file, and parent directory. We need "writing" access to all these files.

## Rationale:
We believe that the using this file descriptor table that maps an `int` to `fd_obj` is a nice abstraction barrrier that will allow us to easily see if `is_dir` and then call the correct file i/o operation.

# Additional Questions:
### Write-Behind
For write behind, we suggest that the creation of a new kernel thread that periodically scans the cache for outdated, dirty items and writes them to disk. After it completes this, it goes to sleep for some amount of time. We would need to add an additional property to our `cache_block` which is a timestamp in kernal ticks. This would be used to track how recently something has been used and get updated upon cache hit. If the item is older than some threshold, it is elgible for write-behind. We would also create a function `void check_cache_for_write_behind()` which would be called when the thread wakes that would handle this maintenance. 
  
### Read-Ahead
The job of read ahead is to improve temporal and spatial locality. For spatial locality we could assign a new kernel thread whose job is to check periodically recently added items in the cache and fetch 2-4 more sequential blocks asynchronously. However, since our cache is so small, this could result in needlessly replacing other blocks that are more frequently used. To handle temporal locality (popular items or recently used), the job is a bit more difficult. Our replacement policy should ensure as best it can that popular items remain in the cache.
