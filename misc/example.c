#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

int main() {
  for (int i = 0; i < 10; i++) {
    printf("%2d. pid: %d\n", i, getpid());
  }
  return 0;
}
