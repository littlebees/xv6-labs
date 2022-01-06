#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

char*
strncpy(char *s, const char *t, int n)
{
  char *os;

  os = s;
  while(n-- > 0 && (*s++ = *t++) != 0)
    ;
  while(n-- > 0)
    *s++ = 0;
  return os;
}

int
main(int argc, char *argv[])
{
  char buf[10][32];
  char tmp[32];
  char *cmd[10];
  for (int i=1;i<argc;i++)
     strncpy(buf[i-1], argv[i], strlen(argv[i]));
  
  while (read(0, tmp, 32) > 0) { // 虛假喚醒!!
    char *start = tmp;
    int end = argc-1;
    for(int len=strlen(tmp),i=0;i<len;i++)
      if (tmp[i] == ' ' || tmp[i] == '\n') {
        tmp[i] = 0;
        strcpy(buf[end++], start);
        start = tmp+i+1;
      }
    for (int i=0;i<end;i++)
      cmd[i] = (char*)&(buf[i]);
    if (fork())
      wait(0);
    else
      exec(buf[0], cmd);
  }

  exit(0);
}
