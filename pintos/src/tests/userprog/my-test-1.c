/* Tries to determine filesize of an invalid fd, which must terminate with exit code -1. */

#include <syscall.h>
#include "tests/main.h"

void
test_main (void)
{ 
  int size;
  size = filesize (0x20101234);
  msg(size);
}
