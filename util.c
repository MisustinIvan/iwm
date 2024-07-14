#include <stdio.h>
#include <stdlib.h>

void panic(char *msg) {
	printf("Panic: %s\n", msg);
	exit(EXIT_FAILURE);
}
