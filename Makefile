CC=gcc

CFLAGS=-s -O2 -Wall

minesweeper: main.c
	$(CC) main.c -o minesweeper $(CFLAGS)
