#include <unistd.h>

__attribute__((noinline)) void foo_block(void) {
  for(;;) sleep(1);
}

__attribute__((noinline)) void foo_outer(void) {
  foo_block();
}
