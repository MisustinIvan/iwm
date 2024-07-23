build:
	cc -o iwm *.c -Wall -Wextra -lX11 -lXft -lfontconfig -I /usr/include/freetype2

clean:
	rm /usr/local/bin/iwm

run:
	./iwm

install:
	cp iwm /usr/local/bin

test:
	./test.sh
