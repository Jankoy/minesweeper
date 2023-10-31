CC=gcc

CFLAGS=-s -O2 -Wall

.PHONY: install

minesweeper: main.c
	$(CC) main.c -o minesweeper $(CFLAGS)

install: minesweeper
	sudo cp minesweeper /usr/local/games
