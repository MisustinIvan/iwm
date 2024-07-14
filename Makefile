build:
	gcc -o iwm *.c -Wall -Wextra -lX11

run:
	./iwm

install:
	cp iwm /usr/local/bin

test:
	./test.sh
