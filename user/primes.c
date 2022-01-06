#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void put(int *p, int n) {
	write(p[1], &n, sizeof(int));
}

int get(int *p) {
	int ret;
	int state = read(p[0], &ret, sizeof(int));
	if (state <= 0)
		return state;
	else
		return ret;
}

void endWrite(int *p) {
	close(p[1]);
}

void endRead(int *p) {
	close(p[0]);
}

void go(int *p) {
	// WARN: get a item one time!!
	int b = get(p);
	if (b > 0) {
		printf("prime %d\n", b);
		int n = get(p);
		if (n > 0) {
			int pp[2];
			pipe(pp);
			if (fork() == 0) {
				endWrite(pp);
				go(pp);
			} else {
				endRead(pp);
				for(;n > 0;n=get(p))
					if (n % b != 0)
						put(pp,n);
				endWrite(pp);
				wait(0);
			}
			
		}
	}
	endRead(p);
}

int main() {
	int p[2];
	pipe(p);
	if (fork() == 0) {
		endWrite(p);
		go(p);
	}
	else {
		endRead(p);
		for (int n=2;n<36;n++)
			put(p,n);
		endWrite(p);
		wait(0);
	}
	exit(0);
}
