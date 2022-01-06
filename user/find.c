#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

char* getFilename(char *path)
{
  char *start = path;
  int len = strlen(path);
  path += len;
  for(;path != start && *path != '/';path--) ;
  if (*path == '/')
    path++;
  return path;
}

int isSubstring(char* s1, char* s2)
{
    int M = strlen(s1);
    int N = strlen(s2);
 
    /* A loop to slide pat[] one by one */
    for (int i = 0; i <= N - M; i++) {
        int j;
        for (j = 0; j < M; j++)
            if (s2[i + j] != s1[j])
                break;
 
        if (j == M)
            return 1;
    }
 
    return 0;
}

int isNotDots(char* name) {
  int len = strlen(name);
  return len >= 3 || (len == 2 && name[0] != '.' && name[1] != '.') || (len == 1 && name[0] != '.');
}

void
find(char *path, char *pat, int has)
{
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  if((fd = open(path, 0)) < 0){
    fprintf(2, "ls: cannot open %s\n", path);
    return;
  }

  if(fstat(fd, &st) < 0){
    fprintf(2, "ls: cannot stat %s\n", path);
    close(fd);
    return;
  }

  switch(st.type){
  case T_FILE:
    if (has)
      printf("%s\n", path);
    break;
  case T_DIR:
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      printf("ls: path too long\n");
      break;
    }
    strcpy(buf, path);
    p = buf+strlen(buf);
    *p++ = '/';
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      if(de.inum == 0)
        continue;
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      if(stat(buf, &st) < 0){
        printf("ls: cannot stat %s\n", buf);
        continue;
      }
      char* filename = getFilename(buf);
      
      if (isNotDots(filename)) {
        find(buf, pat, has || isSubstring(pat, filename));
      }
        
    }
    break;
  }
  close(fd);
}

int
main(int argc, char *argv[])
{
  find(argv[1], argv[2], 0);
  exit(0);
}
