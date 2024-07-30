CC = cc
# compiler flags
# Wall: all warnings
# Wextra: extra warnings
# I: include path
CFLAGS = -Wall -Wextra -I /usr/include/freetype2
# linker flags
# lX11: X11 library
# lXft: Xft library
# lfontconfig: fontconfig library
LDFLAGS = -lX11 -lXinerama -lXft -lfontconfig

iwm: *.c
	$(CC) $(CFLAGS) -o iwm *.c $(LDFLAGS)

# define DEBUG -> conditional compilation
debug: *.c
	$(CC) $(CFLAGS) -DDEBUG -o iwm *.c $(LDFLAGS)

clean:
	rm /usr/local/bin/iwm

install:
	cp iwm /usr/local/bin

test:
	./test.sh
