#include "timer.h"
#include <stdio.h>

void foo(void *arg) {
  while(1) { printf("A"); co_yield(); }
}

void bar(void *arg) {
  while(1) { printf("B"); co_yield(); }
}

// 0x7ffff7477010

int main() {
  struct co *co1 = co_start("A", foo, NULL);
  struct co *co2 = co_start("B", bar, NULL);
  co_wait(co1);
  co_wait(co2);
}

// 0x7ffff7477010
// 0x7fffffffe050
// 0x55555555539b