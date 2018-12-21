/* Writes a random 64KB file, then reads it byte by
  bytes. Then performs a syscall to check the cache's
  write_cnt; Should be on the order of 128. */

#include <random.h>
#include <stdio.h>
#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

#define TEST_SIZE 64*1024
static char buf[TEST_SIZE];
void
test_main (void)
{
  int i;
  CHECK (create ("test-file", TEST_SIZE), "created test file");
  int fd = open ("test-file");

  long long starting_write_cnt = cache_write_cnt ();

  /* write random 64KB file */
  random_bytes (buf, TEST_SIZE);
  for (i = 0; i < TEST_SIZE; i++) {
    write (fd, buf + i, 1);
  }
  close (fd);

  /* Read file byte by byte */
  fd = open ("test-file");
  for (i = 0; i < TEST_SIZE; i++) {
    char c;
    read (fd, &c, 1);
  }
  close (fd);

  /* Check that number of writes is on order of 128 */
  long long finishing_write_cnt = cache_write_cnt ();
  CHECK ((finishing_write_cnt - starting_write_cnt > 100) &&
    (finishing_write_cnt - starting_write_cnt < 200),
    "device writes should be on order of ~128");
}
