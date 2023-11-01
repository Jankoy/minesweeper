#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>

float rand_normalized()
{
	return (float)rand() / RAND_MAX;
}

bool rand_bool(float percent_chance)
{
	return rand_normalized() < percent_chance;
}

typedef struct {
	bool is_shown;
	bool is_bomb;
	uint8_t dangerous_neighbors;
} Cell;

typedef struct {
	uint16_t width;
	uint16_t height;
	struct { int16_t x; int16_t y; } cursor;
	Cell* cells;
} Grid;

#define CELL_AT(g, x, y) (g).cells[((y) * (g).width + (x))]

static Grid create_grid(uint16_t width, uint16_t height)
{
	return (Grid) {
			.width = width,
			.height = height,
			.cursor = { .x = 0, .y = 0 },
			.cells = (Cell*)calloc(width * height, sizeof(Cell))
		};
}

static bool is_cell_in_bounds(Grid* grid, int16_t x, int16_t y)
{
	return ((0 <= x && x < grid->width) && (0 <= y && y < grid->height));
}

static uint8_t count_neighbors(Grid* grid, int16_t x, int16_t y)
{
	uint8_t count = 0;

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

static void cache_all_neighbors(Grid* grid)
{
	for (int16_t y = 0; y < grid->height; y++)
			for (int16_t x = 0; x < grid->width; x++)
				CELL_AT(*grid, x, y).dangerous_neighbors = count_neighbors(grid, x, y);
}

static void randomize_grid(Grid* grid, float bomb_chance)
{
	for (uint16_t y = 0; y < grid->height; y++) {
		for (uint16_t x = 0; x < grid->width; x++) {
			CELL_AT(*grid, x, y).is_shown = false;
			CELL_AT(*grid, x, y).is_bomb = rand_bool(bomb_chance);
		}
	}
	
	cache_all_neighbors(grid);
}

static void destroy_grid(Grid* grid)
{
	free(grid->cells);
}

static void display_grid(Grid* grid)
{
	for (int16_t i = 0; i < grid->width; i++) printf(" = ");
	puts("");
	for (int16_t y = 0; y < grid->height; y++) {
		for (int16_t x = 0; x < grid->width; x++) {
			if (x == grid->cursor.x && y == grid->cursor.y)
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
			if (x == grid->cursor.x && y == grid->cursor.y)
				printf("]");
			else
				printf(" ");
		}
		puts("");
	}
	for (int16_t i = 0; i < grid->width; i++) printf(" = ");
	puts("");
}

static void redisplay_grid(Grid* grid)
{
	printf("%c[%hdA", 27, grid->height + 2);
	printf("%c[%hdD", 27, grid->width);
	
	display_grid(grid);
}

static void open_cell_at_cursor(Grid* grid)
{
	CELL_AT(*grid, grid->cursor.x, grid->cursor.y).is_shown = true;
}

#define DEFAULT_GRID_WIDTH 10
#define DEFAULT_GRID_HEIGHT 10

static Grid grid = {0};
static struct termios savedtattr, tattr = {0};
static char cmd = '\0';
static bool running = true;

void sigint_handler(int dummy)
{
	(void)dummy;
    running = false;
    tcsetattr(STDIN_FILENO, TCSANOW, &savedtattr);
    destroy_grid(&grid);
    exit(0);
}

void sigcont_handler(int dummy)
{
	(void)dummy;
    tcsetattr(STDIN_FILENO, TCSANOW, &tattr);
    display_grid(&grid);
}

void sigstop_handler(int dummy)
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
				if (grid.cursor.y > 0) grid.cursor.y--;
				else grid.cursor.y = grid.height - 1;
				redisplay_grid(&grid);
				break;
			case 'a':
				if (grid.cursor.x > 0) grid.cursor.x--;
				else grid.cursor.x = grid.width - 1;
				redisplay_grid(&grid);
				break;
			case 's':
				if (grid.cursor.y < grid.height - 1) grid.cursor.y++;
				else grid.cursor.y = 0;
				redisplay_grid(&grid);
				break;
			case 'd':
				if (grid.cursor.x < grid.width - 1) grid.cursor.x++;
				else grid.cursor.x = 0;
				redisplay_grid(&grid);
				break;
			case ' ':
				open_cell_at_cursor(&grid);
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
