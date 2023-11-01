CC=gcc

CFLAGS=-s -O2 -Wall -Werror

.PHONY: clean

minesweeper: minesweeper.c
	$(CC) minesweeper.c -o minesweeper $(CFLAGS)

clean:
	rm -f minesweeper
