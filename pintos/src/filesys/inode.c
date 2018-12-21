#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "devices/block.h"


int hand;
static char zeros[BLOCK_SECTOR_SIZE];
static int hits;
static int misses;

int min(int a, int b){
  if (a<b)return a;
  else return b;
}

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos)
{
  ASSERT (inode != NULL);
  ASSERT (pos   >= 0);

  int block_num = pos / BLOCK_SECTOR_SIZE;
  if (block_num < 112)
    return inode->data.direct[block_num];
  else if (block_num<112 + 128){
    block_sector_t sector;
    cached_read(inode->data.indirect, (block_num - 112)*4, &sector, 4);
    return sector;    
  }
  else {
    block_sector_t sector1,sector2;
    int idx_1 = (block_num - 112 - 128) / 128;
    int idx_2 = (block_num - 112 - 128) % 128;
    cached_read(inode->data.doubly_indirect, idx_1*4, &sector1, 4);
    cached_read(sector1, idx_2*4, &sector2, 4);    
    return sector2;
  }
  return 0;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void)
{
  list_init (&open_inodes);
}

void cache_init (void)
{
  hand = 0;
  int i =0; 
  for (i=0;i<CACHE_SIZE;i++){
    Cache[i].valid = 0;
    Cache[i].dirty = 0;
    Cache[i].clock = 0;
    Cache[i].sector= 0;
    Cache[i].data = (uint8_t*)malloc (BLOCK_SECTOR_SIZE);
    lock_init (&(Cache[i].cache_block_lock));
  }
  memset (zeros, 0, BLOCK_SECTOR_SIZE);
  return;
}

bool inode_create (block_sector_t sector, off_t length){
  struct inode_disk *disk_inode = NULL;
  bool success = false;
  int i,j;
  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  int block_num = bytes_to_sectors(length);
  disk_inode = calloc (1, sizeof *disk_inode);

  if (disk_inode != NULL)
    {
      disk_inode->length = length;
      disk_inode->magic  = INODE_MAGIC;

      for (i=0;i<112;i++)disk_inode->direct[i]=0;
      disk_inode->indirect = 0;
      disk_inode->doubly_indirect = 0;

      if (block_num>=0 && create_data_block(block_num,disk_inode)){
        cached_write(sector, 0, disk_inode, BLOCK_SECTOR_SIZE);        
        for (i = 0; i < min(112, block_num); i++) {
          cached_write(disk_inode->direct[i], 0, zeros, BLOCK_SECTOR_SIZE);
        }
        if (block_num > 112){
          block_sector_t* indirect_buffer = malloc(BLOCK_SECTOR_SIZE);
          cached_read(disk_inode->indirect,0,indirect_buffer,BLOCK_SECTOR_SIZE);
          for (i = 0; i < min(128,block_num-112); i++){
            cached_write (indirect_buffer[i],0,zeros,BLOCK_SECTOR_SIZE);
          }
          free(indirect_buffer);
        }
        if (block_num > 112 + 128) {
          block_sector_t* double_buffer = malloc(BLOCK_SECTOR_SIZE);
          block_sector_t* indirect_buffer = malloc(BLOCK_SECTOR_SIZE);

          cached_read(disk_inode->doubly_indirect,0,double_buffer,BLOCK_SECTOR_SIZE);
          for (j=0; j< block_num - 112 - 128; j+=128) {
            cached_read(double_buffer[j/128],0,indirect_buffer,BLOCK_SECTOR_SIZE);
            for (i = 0; i < min(128,block_num - 112 -128 -j); i++) {
              cached_write (indirect_buffer[i],0,zeros,BLOCK_SECTOR_SIZE);
            }            
          }
          free(indirect_buffer);
          free(double_buffer);
        }
        success = true;
      }
      free (disk_inode);
    }
  return success;
}


/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e))
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector)
        {
          inode_reopen (inode);
          return inode;
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  sema_init(&inode->sema, 1);
  cached_read (inode->sector,0, &inode->data,BLOCK_SECTOR_SIZE);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode)
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);

      /* Deallocate blocks if removed. */
      if (inode->removed)
        {
          int i=0;
          while (inode->data.direct[i]!=0){
            free_map_release (inode->data.direct[i], 1);
            i++;
          }

          block_sector_t* indirect_buffer = malloc(BLOCK_SECTOR_SIZE);
          inode_close_indirect(inode->data.indirect,indirect_buffer);

          if (inode->data.doubly_indirect!=0){
            block_sector_t* double_buffer   = malloc(BLOCK_SECTOR_SIZE);
            cached_read(inode->data.doubly_indirect, 0, double_buffer, BLOCK_SECTOR_SIZE);
            for (i=0;i<128;i++){
              inode_close_indirect(double_buffer[i],indirect_buffer);
            }
            free(double_buffer);
          }
          free(indirect_buffer);
        }
      cached_write(inode->sector,0,&(inode->data),BLOCK_SECTOR_SIZE);
      free (inode);
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode)
{
  sema_down (&inode->sema);
  ASSERT (inode != NULL);
  inode->removed = true;
  sema_up (&inode->sema);
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset)
{
  sema_down (&inode->sema);
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;

  while (size > 0)
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;
      cached_read(sector_idx, sector_ofs, buffer + bytes_read,chunk_size);

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  sema_up (&inode->sema);
  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
 */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset)
{
  if (size + offset > inode->data.length){
    if (!inode_resize(&(inode->data), size+offset))
      return false;
  }

  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;
  if (inode->deny_write_cnt)
    return 0;

  sema_down (&inode->sema);
  while (size > 0)
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;
      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Write full sector directly to disk. */
          cached_write(sector_idx, 0, buffer + bytes_written, BLOCK_SECTOR_SIZE);
        }
      else
        {
          /* We need a bounce buffer. */
          if (bounce == NULL)
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

          if (sector_ofs > 0 || chunk_size < sector_left)
            cached_read(sector_idx, 0, bounce,BLOCK_SECTOR_SIZE); 
          else
            memset (bounce, 0, BLOCK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          cached_write(sector_idx, 0, bounce,BLOCK_SECTOR_SIZE);          
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  free (bounce);
  sema_up (&inode->sema);
  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode)
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode)
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}


// helper functions
void  cached_read(block_sector_t sector, int sector_ofs, const void* buffer, int size){
  int cache_idx = sector_num_to_cache_idx(sector);
  memcpy (buffer,Cache[cache_idx].data + sector_ofs, size);
  release_lock_for_cache_block(cache_idx);
  return;
}

void cached_write(block_sector_t sector, int sector_ofs, const void* buffer, int size){
  int cache_idx = sector_num_to_cache_idx(sector);
  Cache[cache_idx].dirty = 1;
  memcpy (Cache[cache_idx].data + sector_ofs,buffer, size);
  release_lock_for_cache_block(cache_idx);
}

//try find block in the cache, return cache idx if find one, -1 o.w.
int block_in_cache(const block_sector_t sector){
  int i;
  for (i=0;i<CACHE_SIZE;i++)
  {
    if (Cache[i].valid ==1 && Cache[i].sector==sector)
      return i;
  }
  return -1;
}


int evict_and_overwrite(block_sector_t sector){
  int i;
  //find a free block
  for (i=0;i<CACHE_SIZE;i++)
  {
    if (Cache[i].valid ==0){
      Cache[i].valid = 1;
      Cache[i].clock = 0;
      Cache[i].dirty = 0;
      Cache[i].sector= sector;
      memset (Cache[i].data, 0, BLOCK_SECTOR_SIZE);
      block_read (fs_device, Cache[i].sector, Cache[i].data);
      return i;
    } 
  } 

  // if there is no free block, evict a block by clock algorithm
  while (true){
    if (Cache[hand].clock!=0){
      Cache[hand].clock -= 1;
      hand = (hand+1)%CACHE_SIZE;
    }
    else{
      evict_cache(hand);
      int i=hand;
      hand = (hand+1)%CACHE_SIZE;
      Cache[i].valid = 1;
      Cache[i].clock = 0;
      Cache[i].dirty = 0;
      Cache[i].sector= sector;
      memset (Cache[i].data, 0, BLOCK_SECTOR_SIZE);
      block_read (fs_device, Cache[i].sector, Cache[i].data);
      return i;
    }
  }
}

void evict_cache(int i){
  if (Cache[i].valid==0)
    return;
  if (Cache[i].dirty==1){
    block_write (fs_device, Cache[i].sector, Cache[i].data);
  }
  Cache[i].valid=0;
  return;
}

//this function returns the cache entry of the sector, if the sector is not in cache, load it into the cache
// bring block sector into cache, acquire the reading lock of that cache.
int sector_num_to_cache_idx(const block_sector_t pointer){
  int cache_idx = block_in_cache(pointer);
  if (cache_idx==-1){
    //block is not in cache
    misses++;
    acquire_lock_for_evicting();
    cache_idx = block_in_cache(pointer);
    if (cache_idx!=-1){
      //block now in the cache
      release_lock_for_evicting();
    } else {
      cache_idx = evict_and_overwrite(pointer);
      release_lock_for_evicting();
    }
  } else {
    hits++;
  }
  acquire_lock_for_cache_block(cache_idx);
  return cache_idx;
}




void acquire_lock_for_cache_block(int cache_idx ){
  lock_acquire (&(Cache[cache_idx].cache_block_lock));
  return;
}
void release_lock_for_cache_block(int cache_idx){
  Cache[cache_idx].clock=1;
  lock_release (&(Cache[cache_idx].cache_block_lock));
  return;
}

void acquire_lock_for_evicting(){
  int i;
  for (i=0;i<CACHE_SIZE;i++){
    acquire_lock_for_cache_block(i);
  }
  return;
}

void release_lock_for_evicting(){
  int i;
  for (i=0;i<CACHE_SIZE;i++){
    release_lock_for_cache_block(i);
  }
  return;
}

// task 2 helper functions:
// given an array of direct pointers, allocate a block for each
bool create_data_block(int num,struct inode_disk *disk_inode){
  int i;
  for (i=0;i<=min(112,num);i++) {
    if (!free_map_allocate (1, &(disk_inode->direct[i]))){
      return false;
    }
  }

  if (num>112) {
    if (!free_map_allocate (1, &disk_inode->indirect)){
      return false;
    }
    block_sector_t* indirect_buffer = malloc(BLOCK_SECTOR_SIZE);
    if (false == create_data_block_indirect(min(num-112,128),disk_inode->indirect,indirect_buffer)){
      return false;
    }
    
      if (num>112+128){
        if (!free_map_allocate (1, &disk_inode->doubly_indirect)){
          return false;
        }
        block_sector_t* double_buffer = malloc(BLOCK_SECTOR_SIZE);
        memset (double_buffer, 0, BLOCK_SECTOR_SIZE);
        for (i=0;i<num-112-128;i+=128){
          if (!free_map_allocate (1, &(double_buffer[i/128]))){
            return false;
          }
          if (false == create_data_block_indirect(min(num-112-128-i,128),double_buffer[i/128],indirect_buffer)){
            return false;
          }  
        }
        cached_write(disk_inode->doubly_indirect,0,double_buffer,BLOCK_SECTOR_SIZE);
        free(double_buffer);
      }
    free(indirect_buffer);
  }

  return true;
}


bool inode_resize(struct inode_disk *id, off_t size){
  int i;
  for (i = 0; i < 112; i++) {
    if (size <= BLOCK_SECTOR_SIZE * i && id->direct[i] != 0) {
      free_map_release (id->direct[i],1);
      id->direct[i] = 0;
    }
    if (size > BLOCK_SECTOR_SIZE * i && id->direct[i] == 0) {
      if (!free_map_allocate (1, &id->direct[i])){
        id->direct[i] = 0;
        inode_resize(id, id->length);
        return false;        
      }
      if (id->direct[i] == 0) {
        inode_resize(id, id->length);
        return false;
      }
    }
  }
  
  if (id->indirect == 0 && size <= 112 * BLOCK_SECTOR_SIZE) {
    id->length = size;
    return true;
  }

  block_sector_t* indirect_buffer = malloc(BLOCK_SECTOR_SIZE);
  if(!inode_resize_indirect(id,size,112,&id->indirect,indirect_buffer)){
    return false;
  }


  //double
  if (id->doubly_indirect == 0 && size <= (112 + 128) * BLOCK_SECTOR_SIZE) {
    id->length = size;
    return true;
  }

  int num_resized_block = 112 + 128;
  block_sector_t* double_buffer = malloc(BLOCK_SECTOR_SIZE);
  if (id->doubly_indirect == 0){
    if (!free_map_allocate (1, &id->doubly_indirect)){
      id->doubly_indirect = 0;
      free(double_buffer);
      free(indirect_buffer);
      inode_resize(id, id->length);
      return false;        
    }
    memset(double_buffer,0,BLOCK_SECTOR_SIZE);    
  }else{
    cached_read(id->doubly_indirect,0,double_buffer,BLOCK_SECTOR_SIZE);
  }

  for (i=0;i<128;i++){
    if (double_buffer[i] == 0 && size <= num_resized_block * BLOCK_SECTOR_SIZE) {
      id->length = size;
      break;
    }
    if (size > (num_resized_block) * BLOCK_SECTOR_SIZE){
      if(!inode_resize_indirect(id,size,num_resized_block,&double_buffer[i],indirect_buffer)){
        free(double_buffer);
        return false;
      }     
    }
    num_resized_block+=128;
  }

  cached_write(id->doubly_indirect,0,double_buffer,BLOCK_SECTOR_SIZE);
  free(double_buffer);
  free(indirect_buffer);
  id->length = size;
  return true;
}

int get_cache_hit_rate () {
  int denom = misses + hits == 0 ? 1 : misses + hits;
  return 100 * hits / denom;
}


int get_cache_hits () {
  return hits;
}

int get_cache_misses () {
  return misses;
}

void clear_cache_hit_rate () {
  hits = 0;
  misses = 0;
}

long long get_cache_write_cnt () {
  return get_device_write_cnt (fs_device);
}

void inode_close_indirect(block_sector_t indirect,block_sector_t* indirect_buffer){
  int i;
  if (indirect!=0){
    cached_read(indirect, 0, indirect_buffer,BLOCK_SECTOR_SIZE);
    for (i=0;i<128;i++){
      if (indirect_buffer[i]!=0)free_map_release (indirect_buffer[i], 1);
    }
    free_map_release (indirect, 1);
  }
}

bool create_data_block_indirect(int num, block_sector_t indirect, block_sector_t* indirect_buffer){
  int i;
  memset (indirect_buffer, 0, BLOCK_SECTOR_SIZE);
  for (i=0;i<num;i++){
    if (!free_map_allocate (1, &(indirect_buffer[i]))){
      return false;
    }      
  }
  cached_write(indirect,0,indirect_buffer,BLOCK_SECTOR_SIZE);
  return true;
}

bool inode_resize_indirect(struct inode_disk *id,int size,int num_resized_block,block_sector_t* indirect,block_sector_t* indirect_buffer){
  int i;
  if (*indirect == 0){
    if (!free_map_allocate (1, indirect)){
      *indirect = 0;
      inode_resize(id, id->length);
      free(indirect_buffer);
      return false;        
    }
    memset(indirect_buffer,0,BLOCK_SECTOR_SIZE);    
  }else{
    cached_read(*indirect,0,indirect_buffer,BLOCK_SECTOR_SIZE);
  }

  for (i = 0; i < 128; i++){
    if (size <= (num_resized_block+i) * BLOCK_SECTOR_SIZE && indirect_buffer[i]!=0){
      free_map_release (indirect_buffer[i],1);
      indirect_buffer[i] = 0;
    }
    if (size > (num_resized_block+i) * BLOCK_SECTOR_SIZE && indirect_buffer[i]==0){
      if (!free_map_allocate (1, &indirect_buffer[i])){
        *indirect = 0;
        inode_resize(id, id->length);
        free(indirect_buffer);
        return false;        
      }      
    }    
  }
  cached_write(*indirect,0,indirect_buffer,BLOCK_SECTOR_SIZE);
  return true;
}
