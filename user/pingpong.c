#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void closeR(int *p) { close(p[0]); }
void closeW(int *p) { close(p[1]); }

int
main(int argc, char *argv[])
{
  int p2c[2], c2p[2];
  pipe(p2c), pipe(c2p);

  if (fork()) {
    int tmp = 0;
    closeR(p2c);
    closeW(c2p);
    write(p2c[1], &tmp, sizeof(int));
    read(c2p[0], &tmp, sizeof(int));
    printf("%d: received pong\n", getpid());
    closeW(p2c);
    closeR(c2p);
  } else {
    int tmp = 1;
    closeW(p2c);
    closeR(c2p);
    read(p2c[0], &tmp, sizeof(int));
    printf("%d: received ping\n", getpid());
    write(c2p[1], &tmp, sizeof(int));
    closeR(p2c);
    closeW(c2p);
  }
  exit(0);
}
