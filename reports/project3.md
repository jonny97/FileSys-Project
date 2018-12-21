Final Report for Project 3: file system
===================================

## Group Members

* Ross Luo <ross.luo@berkeley.edu>
* Revekka Kostoeva <rkostoeva@berkeley.edu>
* JiannanJiang <jjn2015@berkeley.edu>
* Zane Christenson <zane.c@berkeley.edu>

# Alterations to Initial Design

## Task 1:
### Cache Block
We decided against keeping track of a pointer to an `inode` struct within our `cached_block` struct and instead store the secor number of the disk location associated with that cache block.

* We added a new function `void cache_init(void)` to deal with initializing the cache.
* We altered the function `void cache_read_block (struct cache_block *block)` to now take in new parameters: `void cached_read (block_sector_t sector, int sector_ofs, const void* buffer, int size)`. We needed the `sector` and `sector_ofs` parameters because we now tied cache blocks to their cached data through a sector number. We needed the `buffer` and `size` to write data to the cached block.
* We added a new function `void cached_write(block_sector_t sector, int sector_ofs, const void* buffer, int size)` as a helper to write to the cache. This replaced the originally proposed `bool brint_block_to_cache(int i)` function.
* We added a new function `int sector_num_to_cache_idx(const block_sector_t)` to help us locate data in the cache based on the sector number of the location on disk./
* We added a new function `int block_in_cache (const block_sector_t sector)` to help us locate the block in the cache corresponding to each sector.
* We added a new function `int evict_and_overwrite(block_sector_t sector)` to help us streamline dealing with capacity misses in our cache.
* We replaced our `int block_address_evict()` function with `void evict_cache(int i)` because we do not need to return a value when evicting a cache block and we need the index `i` of the cache block we want to evict so we evict the right block.

### Cache eviction
* We realized during testing that we would need to evict all blocks from cache on system shutdown to force these dirty blocks to be written to disk to allow for file system persistence. This was a simple fix that we added to the `filesys_done` hook.

## Task 2:
### The `inode_disk` structure:
* We altered the original 12 direct pointers to now be 112 direct pointers because we realized that we had unused bytes which we could put to use by making them into direct pointers. This way, we do not have useless bytes in a sector block. This improves the efficiency of accessing data in smaller files.

### Function modifications
* While not mentioned in our original design doc, we also had to change the implementation of `inode_open` and `inode_close` to accomodate the cache and new disk struct.


## Task 3:
### Current working directory
We decided against storing the current working directory of each thread as char array `char cwd[MAX_PATH_LEN];`
and instead opted for a better solution of a dir pointer `struct dir *cwd;` while this complicated things like rel/abs path validation, it made the overal design much easier to implement.

### file descriptor handling
Our `struct fd_obj` and updates to the `fd_table` worked out perfectly as described in our design doc. There were no changes to this part of the implementation.

### Synchronization changes
Rather than going with the more complicated readers and writers scheme, we choose to provide file read/write exclusion with a single semaphore on each file. We up/downed this semaphore during `inode_read`s and `inode_writes` this provided us with the correct file system synchronization requirements.

### '.' and '..' handling
As described in our design doc, we added two additional `dir_entry`s to each directory and this worked out great. The only thing that we did not for see is that the direct `.` and the current directory must have the same inumber. This was an easy fix as we just had to make sure that `.` and `..` were the same pointers to the corresponding `sectors` rather than new pointers entirely.


# Project Reflection
## What did each member do?
* Zane Christenson: Initial design document, fixed proj2 code, implemented task 3, worked on task 2, wrote cache tests and testing report, worked on final report.
* Revekka Kostoeva: Initial design document; task 2; final report - task 1, task 2.
* Jiannan Jiang: Initial design document; task 1; task 2.
* Ross Luo: Initial design document.

## What went well?

* We did a better job of breaking up this project into discrete tasks that could be completed simultaneously. This required good coordination between the active members and a very cohesive design. I believe we accomplished this well.

## What could be improved?

* We ultimately had to use our slip days to complete this project. This put unnecessary stress on us to complete in time and could have been better resolved if we just set better deadlines and expectations at the start.

# Student Testing Report:
## Test 1 - Cache Hit Rate
### Description
This test case creates, opens, and sequentially reads from a file on a cold cache. It checks cache hit rate after the first sequential read, then performs same operation again. Since this file has less blocks than the total size of the cache, we expect that the hit rate would increase since on the second read it is accessing the same blocks that should be in the cache already.

### Mechanics
* Test file size is `50*512` bytes in length.
* Created static local variables that store hits and misses in cache.
* Created a userprog accessible syscall to access cache hit rate.

### Output & Results
`cache-hitrate.output`
```
>cat tests/filesys/extended/cache-hitrate.output

Copying tests/filesys/extended/cache-hitrate to scratch partition...
Copying tests/filesys/extended/tar to scratch partition...
qemu -hda /tmp/upLkdVdWon.dsk -hdb tmp.dsk -m 4 -net none -nographic -monitor null
PiLo hda1
Loading...........
Kernel command line: -q -f extract run cache-hitrate
Pintos booting with 4,088 kB RAM...
382 pages available in kernel pool.
382 pages available in user pool.
Calibrating timer...  209,510,400 loops/s.
hda: 1,008 sectors (504 kB), model "QM00001", serial "QEMU HARDDISK"
hda1: 176 sectors (88 kB), Pintos OS kernel (20)
hda2: 236 sectors (118 kB), Pintos scratch (22)
hdb: 5,040 sectors (2 MB), model "QM00002", serial "QEMU HARDDISK"
hdb1: 4,096 sectors (2 MB), Pintos file system (21)
filesys: using hdb1
scratch: using hda2
Formatting file system...done.
Boot complete.
Extracting ustar archive from scratch device into file system...
Putting 'cache-hitrate' into the file system...
Putting 'tar' into the file system...
Erasing ustar archive...
Executing 'cache-hitrate':
(cache-hitrate) begin
(cache-hitrate) created test file
(cache-hitrate) write random bytes
(cache-hitrate) read all bytes
(cache-hitrate) read all bytes again
(cache-hitrate) Second read should have greater hit rate!
(cache-hitrate) end
cache-hitrate: exit(0)
Execution of 'cache-hitrate' complete.
Timer: 101 ticks
Thread: 0 idle ticks, 90 kernel ticks, 11 user ticks
hdb1 (filesys): 323 reads, 519 writes
hda2 (scratch): 235 reads, 2 writes
Console: 1219 characters output
Keyboard: 0 keys pressed
Exception: 0 page faults
Powering off...
```
```
> cat tests/filesys/extended/cache-hitrate.result

PASS
```

### Potential Bugs
* If the cache was going to disk to fetch blocks every single read/write and not storing them in the cache correctly, this test case would register 0% hit rate and catch the error. When the test checks to see if the cache hit rate on the second read is strictly greater than, it would fail and alert us to the problem.
* If the cache was returning the same data block every single time rather than the correct data block requested, then all cache hit rate would be 100% on all reads. Again since the test checks for a strictly greater than improvement of the cache rate, this would also fail the test and alert us to the problem.
  
## Test 2 - Cache Coalescing
### Description
This test writes a random 64KB file byte by byte, then reads it byte bytes. It then performs a syscall to check the cache's `write_cnt`. The `write_cnt` Should be on the order of 128 since 64KB corresponds to roughly 128 disk blocks that must be written.

### Mechanics
* 64KB was chosen since it is twice the size of the cache and thus would mean that all `512` size blocks would get evicted after the first 32KB.
* A syscall had to be implemeted along with some internal functions to nice retreive the `write_cnt` from the `fs_device` struct. The test utilizes this syscall.

### Outputs & Results
```
>cat tests/filesys/extended/cache-coalesce.output

Copying tests/filesys/extended/cache-coalesce to scratch partition...
Copying tests/filesys/extended/tar to scratch partition...
qemu -hda /tmp/6sU22TZ9sp.dsk -hdb tmp.dsk -m 4 -net none -nographic -monitor null
PiLo hda1
Loading...........
Kernel command line: -q -f extract run cache-coalesce
Pintos booting with 4,088 kB RAM...
382 pages available in kernel pool.
382 pages available in user pool.
Calibrating timer...  287,539,200 loops/s.
hda: 1,008 sectors (504 kB), model "QM00001", serial "QEMU HARDDISK"
hda1: 176 sectors (88 kB), Pintos OS kernel (20)
hda2: 236 sectors (118 kB), Pintos scratch (22)
hdb: 5,040 sectors (2 MB), model "QM00002", serial "QEMU HARDDISK"
hdb1: 4,096 sectors (2 MB), Pintos file system (21)
filesys: using hdb1
scratch: using hda2
Formatting file system...done.
Boot complete.
Extracting ustar archive from scratch device into file system...
Putting 'cache-coalesce' into the file system...
Putting 'tar' into the file system...
Erasing ustar archive...
Executing 'cache-coalesce':
(cache-coalesce) begin
(cache-coalesce) created test file
(cache-coalesce) device writes should be on order of ~128
(cache-coalesce) end
cache-coalesce: exit(0)
Execution of 'cache-coalesce' complete.
Timer: 309 ticks
Thread: 0 idle ticks, 78 kernel ticks, 231 user ticks
hdb1 (filesys): 529 reads, 733 writes
hda2 (scratch): 235 reads, 2 writes
Console: 1125 characters output
Keyboard: 0 keys pressed
Exception: 0 page faults
Powering off...
```
```
> cat tests/filesys/extended/cache-coalesce.result

PASS
```

### Potential Bugs
* If the syscall for write had a bug that returned a postive len but did not actually write to the disk, then our write_cnt would far lower than expected in the bounds of the test case causing it to fail.
* If the kernel wrote to disk after each byte that was written to the system, we would expect a `write-cnt` much much larger than the 100-200 expected in the test case. This would be a sign that the system is not properly coalescing writes and cause the test to fail.

## Improving the Pintos Testing System
* Running tests in pintos is a bit of pain. If you want to run a single test, you must specify a long command (or even two if you want to run the pearl script part of it too) and run it from the `build` folder. This generally makes testing a bit slower and requires some copying and pasting of long commands into terminals.
* Adding tests to pintos isn't too hard but it could be nicer. The `.c` portion of the test is straight forward but its a bit annoying that you must also create the `.ck` file with the expected output. This means that if you edit the `.c` test even slightly, you usually have to go edit the `.ck` file too. I could see this as being a potential source of bugs if a developer makes a change and forgets to update both files.
* Debugging through gdb is difficult; every project part so far required a slightly different command so simply referring back to Project 1 spec was not enough.
