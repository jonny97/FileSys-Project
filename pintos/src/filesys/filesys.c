#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format)
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  cache_init ();
  free_map_init ();

  if (format)
    do_format ();

  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void)
{
  int i=0;
  for (i=0;i<CACHE_SIZE;i++){
    evict_cache(i);
  }
  for (i=0;i<CACHE_SIZE;i++){
    free(Cache[i].data);
  }
  free_map_close ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size)
{
  block_sector_t inode_sector = 0;
  struct dir *dir = dir_open_root ();
  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size)
                  && dir_add (dir, name, inode_sector, false));
  if (!success && inode_sector != 0)
    free_map_release (inode_sector, 1);
  dir_close (dir);

  return success;
}

/* Recursively traverses path into subdirectories
  and then attempts to create a file/dir with an
  initial_size and depending on is_dir. */
bool
filesys_create_r (const char *path, off_t initial_size, bool is_dir)
{
  if (path == NULL || strlen(path) == 0) {
    return false;
  }

  struct dir *dir_ptr;
  if (path[0] == '/') {
    dir_ptr = dir_open_root ();
  } else {
    dir_ptr = dir_reopen (thread_current ()->cwd);
  }

  char path_copy[strlen(path)];
  strlcpy (path_copy, path, strlen(path) + 1);
  char *save_ptr;
  char *cur = strtok_r(path_copy, "/", &save_ptr);
  char *next = strtok_r(NULL, "/", &save_ptr);
  struct inode *inode_ptr;
  while (next != NULL) {
    if (!dir_lookup(dir_ptr, cur, &inode_ptr)) {
      dir_close (dir_ptr);
      return false;
    } else {
      dir_close(dir_ptr);
      dir_ptr = dir_open(inode_ptr);
      cur = next;
      next = strtok_r(NULL, "/", &save_ptr);
    }
  }

  bool success;
  block_sector_t inode_sector = 0;
  if (is_dir) {
    success = (dir_ptr != NULL && free_map_allocate (1, &inode_sector)
      && dir_create (inode_sector, initial_size) 
      && dir_add (dir_ptr, cur, inode_sector, true));

    // Create . and .. dir_entries
    struct inode *child_inode;
    dir_lookup (dir_ptr, cur, &child_inode);
    struct dir *dir_child = dir_open (child_inode);
    dir_add (dir_child, ".", dir_child->inode->sector, true);
    dir_add (dir_child, "..", dir_ptr->inode->sector, true);

  } else {
    success = (dir_ptr != NULL && free_map_allocate (1, &inode_sector)
      && inode_create (inode_sector, initial_size) 
      && dir_add (dir_ptr, cur, inode_sector, false));
  }

  dir_close (dir_ptr);
  if (!success && inode_sector != 0)
    free_map_release (inode_sector, 1);
  return success;
}

/* changes directory */
bool
filesys_chdir (struct thread *t, char *path)
{
  struct dir *dir_ptr;
  if (strcmp(path, "/") == 0) {
    dir_close (t->cwd);
    t->cwd = dir_open_root ();
    return t->cwd != NULL;
  } else if (path[0] == '/') {
    dir_ptr = dir_open_root ();
  } else {
    dir_ptr = dir_reopen (t->cwd);
  }

  char path_copy[strlen(path)];
  strlcpy (path_copy, path, strlen(path) + 1);
  char *save_ptr;
  char *chunk = strtok_r(path_copy, "/", &save_ptr);
  struct inode *inode_ptr;
  while (chunk != NULL) {
    if (!dir_lookup(dir_ptr, chunk, &inode_ptr)) {
      dir_close (dir_ptr);
      return false;
    } else {
      dir_close(dir_ptr);
      dir_ptr = dir_open(inode_ptr);
      chunk = strtok_r(NULL, "/", &save_ptr);
    }
  }

  dir_close (t->cwd);
  t->cwd = dir_ptr;
  return true;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  struct dir *dir = dir_open_root ();
  struct inode *inode = NULL;

  if (dir != NULL)
    dir_lookup (dir, name, &inode);
  dir_close (dir);

  return file_open (inode);
}

/* Opens the file with the given NAME recursively and handles abs vs rel paths.
   Returns true if successful in opening file or dir. Adds ptrs to fd_obj arg.*/
bool
filesys_open_r (const char *path, struct fd_obj *dest)
{
  if (path == NULL || strlen(path) == 0) {
    return false;
  }

  struct dir *dir_ptr;
  if (strcmp(path, "/") == 0) {
    dest->dir_ptr = dir_open_root ();
    dest->is_dir = true;
    return dest->dir_ptr != NULL;
  } else if (path[0] == '/') {
    dir_ptr = dir_open_root ();
  } else {
    dir_ptr = dir_reopen (thread_current ()->cwd);
  }

  char path_copy[strlen(path)];
  strlcpy (path_copy, path, strlen(path) + 1);
  char *save_ptr;
  char *cur = strtok_r(path_copy, "/", &save_ptr);
  char *next = strtok_r(NULL, "/", &save_ptr);
  struct inode *inode_ptr;
  while (next != NULL) {
    if (!dir_lookup(dir_ptr, cur, &inode_ptr)) {
      dir_close (dir_ptr);
      return false;
    } else {
      dir_close(dir_ptr);
      dir_ptr = dir_open(inode_ptr);
      cur = next;
      next = strtok_r(NULL, "/", &save_ptr);
    }
  }

  struct dir_entry e;
  if (!lookup (dir_ptr, cur, &e, NULL)) {
    dir_close (dir_ptr);
    return false;
  }
  
  if (e.is_dir) {
    dest->dir_ptr = dir_open (inode_open (e.inode_sector));
    dest->is_dir = true;
  } else {
    dest->file_ptr = file_open (inode_open (e.inode_sector));
    dest->is_dir = false;
  }

  dir_close (dir_ptr);
  return dest->dir_ptr != NULL || dest->file_ptr != NULL;
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *path)
{
  struct dir *dir_ptr;
  if (strcmp(path, "/") == 0) {
    return false;
  } else if (path[0] == '/') {
    dir_ptr = dir_open_root ();
  } else {
    dir_ptr = dir_reopen (thread_current ()->cwd);
  }

  char path_copy[strlen(path)];
  strlcpy (path_copy, path, strlen(path) + 1);
  char *save_ptr;
  char *cur = strtok_r(path_copy, "/", &save_ptr);
  char *next = strtok_r(NULL, "/", &save_ptr);
  struct inode *inode_ptr;
  while (next != NULL) {
    if (!dir_lookup(dir_ptr, cur, &inode_ptr)) {
      dir_close (dir_ptr);
      return false;
    } else {
      dir_close(dir_ptr);
      dir_ptr = dir_open(inode_ptr);
      cur = next;
      next = strtok_r(NULL, "/", &save_ptr);
    }
  }

  bool success = dir_ptr != NULL && dir_remove (dir_ptr, cur);
  dir_close (dir_ptr);
  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();

  // Create . and .. dir_entries
  struct dir *root = dir_open_root ();
  dir_add (root, ".", root->inode->sector, true);
  dir_add (root, "..", root->inode->sector, true);
  printf ("done.\n");
}

bool
is_dir_open (struct dir *d)
{
  int inum = (int)d->inode->sector;
  struct thread *t = thread_current ();
  int fd;
  for (fd = 3; fd < FD_MAX; fd++) {
    if (t->fd_table[fd]->is_dir
      && (int)t->fd_table[fd]->dir_ptr->inode->sector == inum) {
      return true;
    }
  }
  return false;
}
