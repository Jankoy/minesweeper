CC=gcc

CFLAGS=-s -O2 -Wall -Werror

.PHONY: clean

minesweeper: main.c
	$(CC) main.c -o minesweeper $(CFLAGS)

clean:
	rm -f minesweeper
