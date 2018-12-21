/* Tries to run tell() on an invalid fd, which must terminate with exit code -1. */

#include <syscall.h>
#include "tests/main.h"

void 
test_main (void)
{
	tell (0x20101234);
}
