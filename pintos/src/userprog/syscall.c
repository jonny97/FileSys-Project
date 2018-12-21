#include <stdio.h>
#include <syscall-nr.h>
#include "userprog/syscall.h"
#include "userprog/pagedir.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "threads/malloc.h"
#include "userprog/process.h"
#include "threads/vaddr.h"
static void syscall_handler (struct intr_frame *);

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

void
sys_exit (struct intr_frame *f, int code)
{
  struct thread *t = thread_current ();

  if (t->pr==NULL){
    f->eax = code;
    printf("%s: exit(%d)\n", (char *) &thread_current ()->name, code);
    thread_exit ();    
  }

  t->pr->exit_status = code;
  sema_up(&t->pr->relationship_sema);
  sema_up(&t->pr->child_started);

  lock_acquire(&t->pr->relationship_lock);
  t->pr->alive_count--;
  lock_release(&t->pr->relationship_lock);

  /* If last member in relationship, free memory of struct */
  if (t->pr->alive_count == 0) {
    free (t->pr);
  }

  f->eax = code;
  printf("%s: exit(%d)\n", (char *) &thread_current ()->name, code);
  thread_exit ();
}

static struct fd_obj*
sys_fd_lookup (int fd)
{
  if (fd < 0 || fd >= FD_MAX) {
    return (struct fd_obj *) -1;
  }

  struct thread *t = thread_current ();
  return t->fd_table[fd];
}

static void
syscall_handler (struct intr_frame *f UNUSED)
{
  uint32_t* args = ((uint32_t*) f->esp);
  struct thread *t = thread_current ();

  if (!is_user_vaddr(args) || !pagedir_get_page(thread_current()->pagedir, args)) {
	  sys_exit(f, -1);
  }

  // SYSCALL 4-12 = Filesys
  if ((int) args[0] >= 4 && (int) args[0] <= 12) {
    switch(args[0]) {
      case SYS_CREATE: {

        if (!args[1] || !is_user_vaddr((void*)args[1]) || !pagedir_get_page(thread_current()->pagedir, (void*)args[1])) {
          sys_exit(f, -1);
        }
        char *filename = (char *) args[1];
        unsigned initial_size = args[2];
        if (filename == NULL) {
          sys_exit (f, -1);
        } else {
          f->eax = filesys_create_r (filename, initial_size, false);
        }
        break;
      }
      case SYS_REMOVE: {

        if(!args[1] || !is_user_vaddr((void*)args[1]) || !pagedir_get_page(thread_current()->pagedir, (void*)args[1])) {
          sys_exit(f, -1);
        }
        char *filename = (char *) args[1];
        if (filename == NULL) {
          sys_exit (f, -1);
        } else {
          f->eax = filesys_remove (filename);
        }
        break;
      }
      case SYS_OPEN: {

        if (!args[1] || !is_user_vaddr((void*)args[1]) || !pagedir_get_page(thread_current()->pagedir, (void*)args[1])) {
            sys_exit(f, -1);
            break;
        }
        char *filename = (char *) args[1];
        if (filename == NULL) {
          f->eax = -1;
          break;
        }

        int fd = request_fd(t);
        if (!filesys_open_r (filename, t->fd_table[fd])) {
          f->eax = -1;
        } else {
          f->eax = fd;
        }
        break;
      }
      case SYS_FILESIZE: {
        if (!is_user_vaddr((void*)&args[1]) || !pagedir_get_page(thread_current()->pagedir, (void*)&args[1])) {
          sys_exit(f, -1);
        }
        int fd = (int) args[1];
        struct fd_obj *ptr = sys_fd_lookup (fd);

        if (ptr->is_dir) {
          f->eax = -1;
          break;
        }

        struct file *file_ptr = ptr->file_ptr;

        if ((int)file_ptr == -1) {
          sys_exit (f, -1);
        } else if (file_ptr == NULL) {
          f->eax = -1;
        } else {
          off_t len = file_length (file_ptr);
          f->eax = len; 
        }
        break;
      }
      case SYS_READ: {
        if (!args[2] || !is_user_vaddr((void*)args[2]) || !pagedir_get_page(thread_current()->pagedir, (void*)args[2])) {
          sys_exit(f, -1);
        }
        int fd = (int) args[1];
        char *buffer = (char *) args[2];
        unsigned size = args[3];
        struct fd_obj *ptr = sys_fd_lookup (fd);

        if (ptr->is_dir) {
          f->eax = -1;
          break;
        }

        struct file *file_ptr = ptr->file_ptr;
        
        if (fd == STDIN_FILENO) {
          // Handle read from STDIN
          unsigned i = 0;
          while (i < size) {
            *(buffer + i) = input_getc();
            i++;
          }
          f->eax = size;
        } else if ((int)file_ptr == -1) {
          sys_exit (f, -1);
        } else if (file_ptr == NULL) {
          f->eax = -1;
        } else {
          off_t len = file_read (file_ptr, buffer, size);
          f->eax = len;
        }
        break;
      }
      case SYS_WRITE: {
        if (!args[2] || !is_user_vaddr((void*)args[2]) || !pagedir_get_page(thread_current()->pagedir, (void*)args[2])) {
          sys_exit(f, -1);
        }
        int fd = (int) args[1];
        char *buffer = (char *) args[2];
        unsigned size = args[3];

        if (fd == STDOUT_FILENO) {
          putbuf (buffer, size);
          f->eax = size;
        } else if (fd == STDIN_FILENO) {
          // Handle write to STDIN
        } else {
          struct fd_obj *ptr = sys_fd_lookup (fd);

          if (ptr->is_dir) {
            f->eax = -1;
            break;
          }

          struct file *file_ptr = ptr->file_ptr;
          if ((int)file_ptr == -1) {
            sys_exit (f, -1);
          } else if (file_ptr == NULL) {
            f->eax = -1;
          } else {
            off_t len = file_write (file_ptr, buffer, size);
            f->eax = len;
          }
        }
        break;
      }
      case SYS_SEEK: {
        if(!is_user_vaddr((void*)&args[1]) || !pagedir_get_page(thread_current()->pagedir, (void*)&args[1])) {
          sys_exit(f, -1);
        }
        int fd = (int) args[1];
        unsigned position = args[2];
        struct fd_obj *ptr = sys_fd_lookup (fd);

        if (ptr->is_dir) {
          f->eax = -1;
          break;
        }

        struct file *file_ptr = ptr->file_ptr;
        if ((int)file_ptr == -1) {
          sys_exit (f, -1);
        } else if (file_ptr == NULL) {
          f->eax = -1;
        } else {
          file_seek (file_ptr, position);
        }
        break;
      }
      case SYS_TELL: {
        if(!is_user_vaddr((void*)&args[1]) || !pagedir_get_page(thread_current()->pagedir, (void*)&args[1])) {
          sys_exit(f, -1);
        }
        int fd = (int) args[1];

        struct fd_obj *ptr = sys_fd_lookup (fd);

        if (ptr->is_dir) {
          f->eax = -1;
          break;
        }

        struct file *file_ptr = ptr->file_ptr;
        if ((int)file_ptr == -1) {
          sys_exit (f, -1);
        } else if (file_ptr == NULL) {
          f->eax = -1;
        } else {
          off_t offset = file_tell (file_ptr);
          f->eax = offset;
        }
        break;
      }
      case SYS_CLOSE: {
        int fd = (int) args[1];
        struct fd_obj *ptr = sys_fd_lookup (fd);
        if ((int)ptr->file_ptr == -1) {
          sys_exit (f, -1);
        } else if (ptr->file_ptr == NULL && ptr->dir_ptr == NULL) {
          f->eax = -1;
        } else if (ptr->file_ptr != NULL) {
          file_close (ptr->file_ptr);
          free_fd (t, fd);
        } else {
          dir_close (ptr->dir_ptr);
          free_fd (t, fd);
        }
        break;
      }
    }
  } else {
    switch(args[0]) {
      case SYS_CHDIR: {
        char *dir_name = (char *) args[1];
        f->eax = filesys_chdir (t, dir_name);
        break;
      }
      case SYS_MKDIR: {
        const char *dir_name = (const char *) args[1];
        f->eax = filesys_create_r (dir_name, 16, true);
        break;
      }
      case SYS_READDIR: {
        int fd = (int) args[1];
        char *name = (char *) args[2];
        struct dir* dir_ptr = t->fd_table[fd]->dir_ptr;
        f->eax = dir_readdir(dir_ptr, name);
        break;
      }
      case SYS_ISDIR: {
        int fd = (int) args[1];
        f->eax = t->fd_table[fd]->is_dir;
        break;
      }
      case SYS_INUMBER: {
        int fd = (int) args[1];
        struct fd_obj* fd_obj_ptr = t->fd_table[fd];
        if (fd_obj_ptr->is_dir) {
          f->eax = (int)fd_obj_ptr->dir_ptr->inode->sector;
        } else {
          f->eax = (int)fd_obj_ptr->file_ptr->inode->sector;
        }
        break;
      }
      case SYS_HALT: {
        shutdown_power_off ();
        break;
      }
      case SYS_EXIT: {
        file_allow_write(t->file_ptr);
        file_close (t->file_ptr);
        
        if (!is_user_vaddr(&args[1]) || !pagedir_get_page(thread_current()->pagedir, &args[1])) {
          sys_exit(f, -1);
        }

        int status = (int) args[1];
    
        sys_exit(f, status);
        break;
      }
      case SYS_EXEC: {
        // Check for bad-ptr
        if (!is_user_vaddr((void*)args[1]) || !pagedir_get_page(thread_current()->pagedir, (void*)args[1])) {
          sys_exit(f, -1);
        } 

        char *file_name = (char *) args[1];
        f->eax = process_execute (file_name);
        break;
      }
      case SYS_WAIT: {
        tid_t tid = (tid_t) args[1];

        struct list_elem *e;
        for (e = list_begin (&t->child_processes); e != list_end (&t->child_processes); e = list_next (e)) {
          struct process_relationship *pr = list_entry (e, struct process_relationship, elem);
          if (pr->child_tid == tid) {
            if (!pr->has_waited) {
              sema_down(&pr->relationship_sema);
              f->eax = pr->exit_status;
              pr->has_waited = 1;
            } else {
              f->eax = -1;
            }
            return;
          }
        }

        f->eax = -1;
        break;
      }
      case SYS_PRACTICE: {
        f->eax = args[1] + 1;
        break;
      }
      case SYS_CACHE_HITRATE: {
        f->eax = get_cache_hit_rate ();
        break;
      }
      case SYS_CACHE_WRITE_CNT: {
        f->eax = get_cache_write_cnt ();
        break;
      }
    }
  }
}
