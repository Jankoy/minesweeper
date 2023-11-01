CC=gcc

CFLAGS=-s -O2 -Wall -Werror

.PHONY: install

minesweeper: main.c
	$(CC) main.c -o minesweeper $(CFLAGS)

install: minesweeper
	sudo cp minesweeper /usr/local/games
