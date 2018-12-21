#ifndef FILESYS_INODE_H
#define FILESYS_INODE_H

#include <stdbool.h>
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/off_t.h"
#include "devices/block.h"
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "threads/synch.h"

struct bitmap;

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44
#define CACHE_SIZE 64

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    block_sector_t direct[112];          /* 12 direct pointers */
    block_sector_t indirect;            /* singly indirect pointer */
    block_sector_t doubly_indirect;     /* doubly indirect pointer */
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
    uint32_t unused[12];               /* Not used. */
  };

/* In-memory inode. */
struct inode
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;             /* Inode content. */
    struct semaphore sema;              /* Lock used to to provide mutual 
                                          exclusion on files and directories */
  };

struct cached_block {
    int clock;                          /* Used for clock algorithm evicition */
    int valid;                          /* tracks cache block validity */
    int dirty;                          /* Tracks changes to cache block not written to disk */
    block_sector_t sector;              /* The sector storing the data */
    uint8_t* data;                      /* size : [BLOCK_SECTOR_SIZE] */
    struct lock cache_block_lock;
};
struct cached_block Cache[CACHE_SIZE];
int min(int a, int b);


void inode_init (void);
bool inode_create (block_sector_t, off_t);
struct inode *inode_open (block_sector_t);
struct inode *inode_reopen (struct inode *);
block_sector_t inode_get_inumber (const struct inode *);
void inode_close (struct inode *);
void inode_remove (struct inode *);
off_t inode_read_at (struct inode *, void *, off_t size, off_t offset);
off_t inode_write_at (struct inode *, const void *, off_t size, off_t offset);
void inode_deny_write (struct inode *);
void inode_allow_write (struct inode *);
off_t inode_length (const struct inode *);


// cache helper function
void cache_init (void);
void cached_read(block_sector_t sector, int sector_ofs, const void* buffer, int size);
void cached_write(block_sector_t sector, int sector_ofs, const void* buffer, int size);
int  sector_num_to_cache_idx(const block_sector_t );
int  block_in_cache(const block_sector_t sector);

int  evict_and_overwrite(block_sector_t sector);
void evict_cache(int i);

// cache sync helper function
void acquire_lock_for_cache_block(int cache_idx);
void release_lock_for_cache_block(int cache_idx);
void acquire_lock_for_evicting(void);
void release_lock_for_evicting(void);
int get_cache_hit_rate (void);
int get_cache_hits (void);
int get_cache_misses (void);
void clear_cache_hit_rate (void);
long long get_cache_write_cnt (void);


// task 2 helper functions
bool inode_resize(struct inode_disk *inode, off_t size);
bool create_data_block(int num,struct inode_disk *disk_inode);


// helper functions given an indirect pointer
void inode_close_indirect(block_sector_t indirect,block_sector_t* indirect_buffer);
bool create_data_block_indirect(int num,block_sector_t indirect,block_sector_t* indirect_buffer);
bool inode_resize_indirect(struct inode_disk *id,int size,int num_resized_block,block_sector_t* indirect,block_sector_t* indirect_buffer);
bool old_inode_create (block_sector_t, off_t);

#endif /* filesys/inode.h */
