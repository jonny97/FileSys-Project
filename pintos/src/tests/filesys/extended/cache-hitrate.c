/* Opens and sequentially reads from a file on a cold cache,
   checks cache hit rate then performs same operation again.
   Hit rate should increase since blocks are already cached. */

#include <random.h>
#include <stdio.h>
#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

#define TEST_SIZE 50*512
static char buf[TEST_SIZE];
void
test_main (void)
{
  CHECK (create ("test-file", TEST_SIZE), "created test file");
  int fd = open ("test-file");

  /* write random bytes */
  random_bytes (buf, TEST_SIZE);
  CHECK (write (fd, buf, sizeof buf) == TEST_SIZE, "write random bytes");
  close (fd);

  /* Read all bytes then Test cache hit rate */
  fd = open ("test-file");
  CHECK (read (fd, buf, sizeof buf) == TEST_SIZE, "read all bytes");
  int first_hitrate = cache_hitrate ();
  close (fd);

  /* read all bytes and test cache hit rate again, should improve */
  fd = open ("test-file");
  CHECK (read (fd, buf, sizeof buf) == TEST_SIZE, "read all bytes again");
  int second_hitrate = cache_hitrate ();

  CHECK (second_hitrate > first_hitrate, "Second read should have greater hit rate!");
  close (fd);
}
