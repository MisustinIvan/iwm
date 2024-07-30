#include <stdio.h>
#include <stdlib.h>

void panic(char *msg) {
#ifdef DEBUG
	printf("Panic: %s\n", msg);
#endif
	exit(EXIT_FAILURE);
}
