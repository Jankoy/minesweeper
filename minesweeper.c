#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>

typedef struct {
	bool is_shown;
	bool is_bomb;
	bool is_flagged;
	uint8_t dangerous_neighbors;
} Cell;

typedef struct {
	uint16_t width;
	uint16_t height;
	struct { uint16_t x; uint16_t y; } cursor;
	Cell* cells;
} Field;

#define CELL_AT(g, x, y) (g).cells[(y) * (g).width + (x)]

#define DEFAULT_GRID_WIDTH  18
#define DEFAULT_GRID_HEIGHT 20
#define DEFAULT_BOMB_CHANCE 50

static Field field = {0};
static struct termios savedtattr, tattr = {0};
static char cmd = '\0';
static bool running = true;
static bool first_click = true;
static uint16_t empty_cell_count = -1;
static uint16_t revealed_cell_count = 0;


float rand_normalized()
{
	return (float)rand() / RAND_MAX;
}

bool rand_bool(float percent_chance)
{
	return rand_normalized() < percent_chance;
}

uint16_t rand_max(uint16_t max_value)
{
	return rand() % max_value;
}

bool rand_cell(Field* field, uint16_t* x, uint16_t* y)
{
	*x = rand_max(field->width);
	*y = rand_max(field->height);
	return CELL_AT(*field, *x, *y).is_bomb;
}

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
	for (uint16_t y = 0; y < field->height; y++)
			for (uint16_t x = 0; x < field->width; x++)
				CELL_AT(*field, x, y).dangerous_neighbors = count_neighbors(field, x, y);
}

uint16_t randomize_field(Field* field, uint16_t bomb_chance)
{
	for (uint16_t i = 0; i < field->height * field->width; i++)
		field->cells[i].is_shown = false;
	
	if (bomb_chance > 100) bomb_chance = 100;
	uint16_t bomb_count = (field->height * field->width * bomb_chance + 99) / 100;

	uint16_t x, y;
	for (uint16_t i = 0; i < bomb_count; i++) {
		while (rand_cell(field, &x, &y)) {}
		CELL_AT(*field, x, y).is_bomb = true;
	}
	
	cache_all_neighbors(field);

	return field->height * field->width - bomb_count;
}

void destroy_field(Field* field)
{
	free(field->cells);
}

void display_field(Field* field)
{
	for (int16_t i = 0; i < field->width; i++) printf("   ");
	puts("");
	for (int16_t y = 0; y < field->height; y++) {
		for (int16_t x = 0; x < field->width; x++) {
			if (x == field->cursor.x && y == field->cursor.y)
				printf("[");
			else
				printf(" ");
			
			if (CELL_AT(*field, x, y).is_shown)
				if (CELL_AT(*field, x, y).is_bomb)
					printf("+");
				else
					if (CELL_AT(*field, x, y).dangerous_neighbors > 0)
						printf("%hhu", CELL_AT(*field, x, y).dangerous_neighbors);
					else
						printf(" ");
			else if (CELL_AT(*field, x, y).is_flagged)
				printf("?");
			else
				printf("_");
			
			if (x == field->cursor.x && y == field->cursor.y)
				printf("]");
			else
				printf(" ");
		}
		puts("");
	}
	for (int16_t i = 0; i < field->width; i++) printf("   ");
	puts("");
}

void redisplay_field(Field* field)
{
	printf("%c[%hdA", 27, field->height + 2);
	printf("%c[%hdD", 27, field->width);
	
	display_field(field);

	printf("          ");
	printf("%c[%hdD", 27, 10);
}

void open_cell_recursive(Field* field, int16_t x, int16_t y)
{
	if (CELL_AT(*field, x, y).is_bomb || CELL_AT(*field, x, y).is_shown || !is_cell_in_bounds(field, x, y)) return;
	else { CELL_AT(*field, x, y).is_shown = true; revealed_cell_count++; }
	
	open_cell_recursive(field, x - 1, y - 1);
	open_cell_recursive(field, x, y - 1);
	open_cell_recursive(field, x + 1, y - 1);
	open_cell_recursive(field, x - 1, y);
	open_cell_recursive(field, x + 1, y);
	open_cell_recursive(field, x - 1, y + 1);
	open_cell_recursive(field, x, y + 1);
	open_cell_recursive(field, x + 1, y + 1);
}

Cell open_cell_at_cursor(Field* field)
{
	if (!CELL_AT(*field, field->cursor.x, field->cursor.y).is_bomb)
		open_cell_recursive(field, field->cursor.x, field->cursor.y);
	else
		CELL_AT(*field, field->cursor.x, field->cursor.y).is_shown = true;
	return CELL_AT(*field, field->cursor.x, field->cursor.y);
}

void show_all_bombs(Field* field)
{
	for (uint16_t i = 0; i < field->height * field->width; i++) {
		if (field->cells[i].is_bomb)
			field->cells[i].is_shown = true;
	}
}

static void game_over()
{
	show_all_bombs(&field);
	redisplay_field(&field);
	printf("You lose");
	running = false;
}

static void quit()
{
	running = false;
	redisplay_field(&field);
	printf("Quit");
}

static void restart()
{
	empty_cell_count = randomize_field(&field, DEFAULT_BOMB_CHANCE);
	redisplay_field(&field);
	printf("Restarted");
}

static void victory()
{
	show_all_bombs(&field);
	redisplay_field(&field);
	printf("You win");
	running = false;
}

static void sigint_handler(int dummy)
{
	(void)dummy;
    quit();
    puts("");
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
	srand(time(0));

	field = create_field(DEFAULT_GRID_WIDTH, DEFAULT_GRID_HEIGHT);
	
	if (!isatty(STDIN_FILENO)) {
		puts("This is not a terminal, the program must be run from a terminal");
		return 1;
	}
	
	tcgetattr(STDIN_FILENO, &tattr);
	tcgetattr(STDIN_FILENO, &savedtattr);
	tattr.c_lflag &= ~(ICANON | ECHO);
	tattr.c_cc[VMIN] = 1;
	tattr.c_cc[VTIME] = 0;
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &tattr);
	
	empty_cell_count = randomize_field(&field, DEFAULT_BOMB_CHANCE);
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
				quit();
				goto no_redisplay;
				break;
			case 'r':
				restart();
				goto no_redisplay;
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
			case 'f':
				CELL_AT(field, field.cursor.x, field.cursor.y).is_flagged = !CELL_AT(field, field.cursor.x, field.cursor.y).is_flagged;
				break;
			case ' ':
				if (open_cell_at_cursor(&field).is_bomb) {
					if (first_click) {
						CELL_AT(field, field.cursor.x, field.cursor.y).is_bomb = false;
						uint16_t x, y = 0;
						while (!rand_cell(&field, &x, &y)) {}
						CELL_AT(field, x, y).is_bomb = true;
						cache_all_neighbors(&field);
					} else {
						game_over();
						goto no_redisplay;
					}
				}
				if (first_click) first_click = false;
				if (revealed_cell_count == empty_cell_count) {
					victory();
					goto no_redisplay;
				}
				break;
			default:
				break;
		}
		redisplay_field(&field);
no_redisplay:
	}
	
	puts("");
	destroy_field(&field);
	tcsetattr(STDIN_FILENO, TCSANOW, &savedtattr);
	return 0;
}
