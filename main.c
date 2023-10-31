#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>
#include "mydefs.h"

float rand_normalized()
{
	return (float)rand() / RAND_MAX;
}

bool rand_bool(f32 percent_chance)
{
	return rand_normalized() < percent_chance;
}

typedef struct {
	bool is_shown;
	bool is_bomb;
	u8 dangerous_neighbors;
} Cell;

typedef struct {
	u16 width;
	u16 height;
	Cell* cells;
} Grid;

#define CELL_AT(g, x, y) (g).cells[((y) * (g).width + (x))]

Grid create_grid(u16 width, u16 height)
{
	return (Grid) {
			.width = width,
			.height = height,
			.cells = (Cell*)calloc(width * height, sizeof(Cell))
		};
}

bool is_cell_in_bounds(Grid* grid, i16 x, i16 y)
{
	return ((0 <= x && x < grid->width) && (0 <= y && y < grid->height));
}

u8 count_neighbors(Grid* grid, i16 x, i16 y)
{
	u8 count = 0;

	if (is_cell_in_bounds(grid, x - 1, y - 1))
		count += CELL_AT(*grid, x - 1, y - 1).is_bomb;
	
	if (is_cell_in_bounds(grid, x, y - 1))
		count += CELL_AT(*grid, x, y - 1).is_bomb;
	
	if (is_cell_in_bounds(grid, x + 1, y - 1))
		count += CELL_AT(*grid, x + 1, y - 1).is_bomb;
	
	if (is_cell_in_bounds(grid, x - 1, y))
		count += CELL_AT(*grid, x - 1, y).is_bomb;
	
	if (is_cell_in_bounds(grid, x + 1, y))
		count += CELL_AT(*grid, x + 1, y).is_bomb;
	
	if (is_cell_in_bounds(grid, x - 1, y + 1))
		count += CELL_AT(*grid, x - 1, y + 1).is_bomb;
	
	if (is_cell_in_bounds(grid, x, y + 1))
		count += CELL_AT(*grid, x, y + 1).is_bomb;
	
	if (is_cell_in_bounds(grid, x + 1, y + 1))
		count += CELL_AT(*grid, x + 1, y + 1).is_bomb;

	return count;
}

void cache_all_neighbors(Grid* grid)
{
	for (i16 y = 0; y < grid->height; y++)
			for (i16 x = 0; x < grid->width; x++)
				CELL_AT(*grid, x, y).dangerous_neighbors = count_neighbors(grid, x, y);
}

void randomize_grid(Grid* grid, f32 bomb_chance)
{
	for (u16 y = 0; y < grid->height; y++) {
		for (u16 x = 0; x < grid->width; x++) {
			CELL_AT(*grid, x, y).is_shown = false;
			CELL_AT(*grid, x, y).is_bomb = rand_bool(bomb_chance);
		}
	}
	
	cache_all_neighbors(grid);
}

void destroy_grid(Grid* grid)
{
	free(grid->cells);
}

static struct { i16 x; i16 y; } cursor = {0};

#define DEFAULT_GRID_WIDTH 10
#define DEFAULT_GRID_HEIGHT 10

static Grid grid = {0};
static struct termios savedtattr, tattr = {0};
static char cmd = '\0';
static bool running = true;

void display_grid(Grid* grid)
{
	for (i16 i = 0; i < grid->width; i++) printf(" = ");
	puts("");
	for (i16 y = 0; y < grid->height; y++) {
		for (i16 x = 0; x < grid->width; x++) {
			if (x == cursor.x && y == cursor.y)
				printf("[");
			else
				printf(" ");
			if (CELL_AT(*grid, x, y).is_shown)
				if (CELL_AT(*grid, x, y).is_bomb)
					printf("*");
				else
					printf("%hhu", CELL_AT(*grid, x, y).dangerous_neighbors);
			else
				printf("#");
			if (x == cursor.x && y == cursor.y)
				printf("]");
			else
				printf(" ");
		}
		puts("");
	}
	for (i16 i = 0; i < grid->width; i++) printf(" = ");
	puts("");
}

void redisplay_grid(Grid* grid)
{
	printf("%c[%hdA", 27, grid->height + 2);
	printf("%c[%hdD", 27, grid->width);
	
	display_grid(grid);
}

void open_cell(Grid* grid, i16 x, i16 y)
{
	CELL_AT(*grid, x, y).is_shown = true;
}

static void sigint_handler(int dummy)
{
	(void)dummy;
    running = false;
    tcsetattr(STDIN_FILENO, TCSANOW, &savedtattr);
    destroy_grid(&grid);
    exit(0);
}

static void sigcont_handler(int dummy)
{
	(void)dummy;
    tcsetattr(STDIN_FILENO, TCSANOW, &tattr);
    display_grid(&grid);
}

static void sigstop_handler(int dummy)
{
	(void)dummy;
	tcsetattr(STDIN_FILENO, TCSANOW, &savedtattr);
}

int main(void)
{
	srand(69);

	grid = create_grid(DEFAULT_GRID_WIDTH, DEFAULT_GRID_HEIGHT);
	
	if (isatty(STDIN_FILENO) == 0) {
		puts("ERROR: this is not a terminal!");
		return 1;
	}
	
	tcgetattr(STDIN_FILENO, &tattr);
	tcgetattr(STDIN_FILENO, &savedtattr);
	tattr.c_lflag &= ~(ICANON | ECHO);
	tattr.c_cc[VMIN] = 1;
	tattr.c_cc[VTIME] = 0;
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &tattr);
	
	randomize_grid(&grid, 0.28f);
	display_grid(&grid);

	signal(SIGINT, sigint_handler);
	signal(SIGCONT, sigcont_handler);
	signal(SIGSTOP, sigstop_handler);
	
	while (running) {
		tcsetattr(STDIN_FILENO, TCSAFLUSH, &tattr);
		cmd = getchar();
		tcsetattr(STDIN_FILENO, TCSANOW, &savedtattr);
		switch (cmd)
		{
			case 'q':
				running = false;
				break;
			case 'w':
				if (cursor.y > 0) cursor.y--;
				redisplay_grid(&grid);
				break;
			case 'a':
				if (cursor.x > 0) cursor.x--;
				redisplay_grid(&grid);
				break;
			case 's':
				if (cursor.y < grid.height - 1) cursor.y++;
				redisplay_grid(&grid);
				break;
			case 'd':
				if (cursor.x < grid.width - 1) cursor.x++;
				redisplay_grid(&grid);
				break;
			case ' ':
				open_cell(&grid, cursor.x, cursor.y);
				redisplay_grid(&grid);
				break;
			default:
				break;
		}
	}
	
	destroy_grid(&grid);
	tcsetattr(STDIN_FILENO, TCSANOW, &savedtattr);
	return 0;
}
