CC=gcc

CFLAGS=-s -O2 -Wall -Werror
DEBUGCFLAGS=-g -Wall -Werror

.PHONY: debug, clean

minesweeper: minesweeper.c
	$(CC) $(CFLAGS) minesweeper.c -o minesweeper

debug:
	$(CC) $(DEBUGCFLAGS) minesweeper.c -o minesweeper

clean:
	rm -f minesweeper
