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
} Field;

#define CELL_AT(g, x, y) (g).cells[((y) * (g).width + (x))]

Field create_field(uint16_t width, uint16_t height)
{
	return (Field) {
			.width = width,
			.height = height,
			.cursor = { .x = 0, .y = 0 },
			.cells = (Cell*)calloc(width * height, sizeof(Cell))
		};
}

bool is_cell_in_bounds(Field* field, int16_t x, int16_t y)
{
	return ((0 <= x && x < field->width) && (0 <= y && y < field->height));
}

uint8_t count_neighbors(Field* field, int16_t x, int16_t y)
{
	uint8_t count = 0;

	if (is_cell_in_bounds(field, x - 1, y - 1))
		count += CELL_AT(*field, x - 1, y - 1).is_bomb;
	
	if (is_cell_in_bounds(field, x, y - 1))
		count += CELL_AT(*field, x, y - 1).is_bomb;
	
	if (is_cell_in_bounds(field, x + 1, y - 1))
		count += CELL_AT(*field, x + 1, y - 1).is_bomb;
	
	if (is_cell_in_bounds(field, x - 1, y))
		count += CELL_AT(*field, x - 1, y).is_bomb;
	
	if (is_cell_in_bounds(field, x + 1, y))
		count += CELL_AT(*field, x + 1, y).is_bomb;
	
	if (is_cell_in_bounds(field, x - 1, y + 1))
		count += CELL_AT(*field, x - 1, y + 1).is_bomb;
	
	if (is_cell_in_bounds(field, x, y + 1))
		count += CELL_AT(*field, x, y + 1).is_bomb;
	
	if (is_cell_in_bounds(field, x + 1, y + 1))
		count += CELL_AT(*field, x + 1, y + 1).is_bomb;

	return count;
}

void cache_all_neighbors(Field* field)
{
	for (int16_t y = 0; y < field->height; y++)
			for (int16_t x = 0; x < field->width; x++)
				CELL_AT(*field, x, y).dangerous_neighbors = count_neighbors(field, x, y);
}

void randomize_field(Field* field, float bomb_chance)
{
	for (uint16_t i = 0; i < field->height * field->width; i++) {
		field->cells[i].is_shown = false;
		field->cells[i].is_bomb = rand_bool(bomb_chance);
	}
	
	cache_all_neighbors(field);
}

void destroy_field(Field* field)
{
	free(field->cells);
}

void display_field(Field* field)
{
	for (int16_t i = 0; i < field->width; i++) printf(" = ");
	puts("");
	for (int16_t y = 0; y < field->height; y++) {
		for (int16_t x = 0; x < field->width; x++) {
			if (x == field->cursor.x && y == field->cursor.y)
				printf("[");
			else
				printf(" ");
			if (CELL_AT(*field, x, y).is_shown)
				if (CELL_AT(*field, x, y).is_bomb)
					printf("*");
				else
					printf("%hhu", CELL_AT(*field, x, y).dangerous_neighbors);
			else
				printf("#");
			if (x == field->cursor.x && y == field->cursor.y)
				printf("]");
			else
				printf(" ");
		}
		puts("");
	}
	for (int16_t i = 0; i < field->width; i++) printf(" = ");
	puts("");
}

void redisplay_field(Field* field)
{
	printf("%c[%hdA", 27, field->height + 2);
	printf("%c[%hdD", 27, field->width);
	
	display_field(field);
}

Cell open_cell_at_cursor(Field* field)
{
	CELL_AT(*field, field->cursor.x, field->cursor.y).is_shown = true;
	return CELL_AT(*field, field->cursor.x, field->cursor.y);
}

void open_all_cells(Field* field)
{
	for (uint16_t i = 0; i < field->height * field->width; i++) {
		field->cells[i].is_shown = true;
	}
}

#define DEFAULT_GRID_WIDTH 10
#define DEFAULT_GRID_HEIGHT 10

static Field field = {0};
static struct termios savedtattr, tattr = {0};
static char cmd = '\0';
static bool running = true;

static void game_over()
{
	open_all_cells(&field);
	redisplay_field(&field);
	printf("Boom! You lose!\n");
	running = false;
}

static void sigint_handler(int dummy)
{
	(void)dummy;
    running = false;
    tcsetattr(STDIN_FILENO, TCSANOW, &savedtattr);
    destroy_field(&field);
    exit(0);
}

static void sigcont_handler(int dummy)
{
	(void)dummy;
    tcsetattr(STDIN_FILENO, TCSANOW, &tattr);
    display_field(&field);
}

static void sigstop_handler(int dummy)
{
	(void)dummy;
	tcsetattr(STDIN_FILENO, TCSANOW, &savedtattr);
}

int main(void)
{
	srand(69);

	field = create_field(DEFAULT_GRID_WIDTH, DEFAULT_GRID_HEIGHT);
	
	if (isatty(STDIN_FILENO) == 0) {
		puts("This is not a terminal, the program must be run from a terminal");
		return 1;
	}
	
	tcgetattr(STDIN_FILENO, &tattr);
	tcgetattr(STDIN_FILENO, &savedtattr);
	tattr.c_lflag &= ~(ICANON | ECHO);
	tattr.c_cc[VMIN] = 1;
	tattr.c_cc[VTIME] = 0;
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &tattr);
	
	randomize_field(&field, 0.28f);
	display_field(&field);

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
				if (field.cursor.y > 0) field.cursor.y--;
				else field.cursor.y = field.height - 1;
				break;
			case 'a':
				if (field.cursor.x > 0) field.cursor.x--;
				else field.cursor.x = field.width - 1;
				break;
			case 's':
				if (field.cursor.y < field.height - 1) field.cursor.y++;
				else field.cursor.y = 0;
				break;
			case 'd':
				if (field.cursor.x < field.width - 1) field.cursor.x++;
				else field.cursor.x = 0;
				break;
			case ' ':
				if (open_cell_at_cursor(&field).is_bomb) {
					game_over();
					goto no_redisplay;
				}
				break;
			default:
				break;
		}
		redisplay_field(&field);
no_redisplay:
	}
	
	destroy_field(&field);
	tcsetattr(STDIN_FILENO, TCSANOW, &savedtattr);
	return 0;
}
