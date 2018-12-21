#ifndef FILESYS_FILESYS_H
#define FILESYS_FILESYS_H

#include <stdbool.h>
#include "filesys/off_t.h"
#include "filesys/file.h"
#include "filesys/directory.h"
#include "threads/thread.h"

/* Sectors of system file inodes. */
#define FREE_MAP_SECTOR 0       /* Free map file inode sector. */
#define ROOT_DIR_SECTOR 1       /* Root directory file inode sector. */

struct fd_obj {
    struct file* file_ptr;
    struct dir* dir_ptr;
    bool is_dir;
};

/* Block device that contains the file system. */
struct block *fs_device;

void filesys_init (bool format);
void filesys_done (void);
bool filesys_create (const char *name, off_t initial_size);
bool filesys_create_r (const char *path, off_t initial_size, bool is_dir);
bool filesys_chdir (struct thread *t, char *path);
struct file *filesys_open (const char *name);
bool filesys_open_r (const char *path, struct fd_obj *dest);
bool filesys_remove (const char *name);
bool is_dir_open (struct dir *);

#endif /* filesys/filesys.h */
