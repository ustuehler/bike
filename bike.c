/*
 * bike - Ride your bike down the hill.
 *
 * Copyright (C) 2007 Stefan Sperling <stsp@stsp.name>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <curses.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>

#define VERSION		"1.3"
#define NUM_ENEMIES	400
#define MAX_HITS	5
#define PATH_WIDTH	30
#define PATH_LENGTH	25
#define SIDE_EDGE	((COLS - PATH_WIDTH) / 2)
#define TOP_EDGE	(LINES - PATH_LENGTH)
#define BIKE_CHAR	'8'
#define DELAY_USEC	50000L

struct enemy {
	bool used;
	int c;
	int x;
	int y;
	short color;
};

struct bike {
	int x;
	int y;
	int hits;
};

static int done;
static int steps;
static int slowness;
static struct enemy enemies[NUM_ENEMIES];
static struct timeval start_time;
static struct timeval flawless_until;
static bool use_colors;

#define COLOR_DEFAULT	0
#define COLOR_BIKE	1
#define COLOR_ENEMY_1	2
#define COLOR_ENEMY_2	3
#define COLOR_ENEMY_3	4
#define COLOR_STATUS	5
#define COLOR_PATH	6

struct colors {
	short x;
	short fg;
	short bg;
} colors[] = {
	{COLOR_BIKE,	COLOR_WHITE,	COLOR_BLACK},
	{COLOR_ENEMY_1,	COLOR_YELLOW,	COLOR_BLACK},
	{COLOR_ENEMY_2,	COLOR_RED,	COLOR_BLACK},
	{COLOR_ENEMY_3,	COLOR_GREEN,	COLOR_BLACK},
	{COLOR_STATUS,	COLOR_WHITE,	COLOR_BLUE},
	{COLOR_PATH,	COLOR_MAGENTA,	COLOR_BLACK}
};

static void title_screen(void);
static void game_over(void);
static void init_enemy(bool, struct enemy *);
static void draw_enemy(struct enemy *);
static void get_input(struct bike *);
static void advance_game(struct bike *);
static void detect_collisions(struct bike *);
static void init_state(struct bike *);
static void draw_enemies(void);
static void new_enemies(bool);
static void advance_enemies(void);
static void draw_bike(struct bike *);
static void draw_status_bar(struct bike *);
static void draw_path(void);
static void cleanup(void);
static void message(int, int, const char *, ...);
static void wait_for_key(int);

int main(void)
{
	struct bike bike;
	struct timeval last_time = {0L, 0L};
	struct timeval now = {0L, 0L};
	struct timeval res = {0L, 0L};

	(void)initscr();
	cbreak();
	noecho();
	nonl();
	intrflush(stdscr, FALSE);
	keypad(stdscr, TRUE);
	leaveok(stdscr, TRUE);
	nodelay(stdscr, TRUE);
	atexit(cleanup);
	curs_set(0);

	title_screen();

	init_state(&bike);
	if (use_colors) {
		color_set(COLOR_DEFAULT, NULL);
		clear();
	}
	new_enemies(TRUE);
	advance_game(&bike);

	(void)gettimeofday(&last_time, NULL);

	while (!done) {
		(void)gettimeofday(&now, NULL);

		get_input(&bike);
		advance_game(&bike);

		timersub(&now, &last_time, &res);

		if (res.tv_sec == 0 && res.tv_usec < DELAY_USEC)
			usleep(DELAY_USEC - res.tv_usec);

		/* make enemies faster every 10 sec */
		if (slowness > 0 && now.tv_sec > last_time.tv_sec &&
		    ((now.tv_sec - start_time.tv_sec) % 10) == 0)
			slowness--;

		last_time = now;

		if (bike.hits >= MAX_HITS)
			done = TRUE;
	}

	endwin();

	if (bike.hits >= MAX_HITS)
		game_over();
	/* else user pressed 'q' to quit. */

	return 0;
}

static void title_screen(void)
{
	int line = (((LINES)/2) - 5);
	erase();
	message(line, (COLS/2) - 7, " << BIKE %s >>", VERSION);
	message(line + 2, (COLS/2) - 24,
		"Objective: Ride your bike down the hill without");
	message(line + 3, (COLS/2) - 15,
		"hitting more than %i obstacles.", MAX_HITS);
	message(line + 4, (COLS/2) - 24 ,
		"Your bike is the little '%c' at the bottom of the screen.",
		BIKE_CHAR);
	message(line + 5, (COLS/2) - 25,
		"Use the left arrow key, or 'j', or 'h' to move left.");
	message(line + 6, (COLS/2) - 26,
		"Use the right arrow key, or 'k', or 'l' to move right.");
	message(line + 7, (COLS/2) - 14, "Hit the space bar to begin!");
	message(line + 8, (COLS/2) - 17,
		"Press 'q' to quit while in the game.");
	refresh();
	wait_for_key(' ');
}

static void game_over(void)
{
	struct timeval now = {0L, 0L};
	struct timeval total = {0L, 0L};
	struct timeval flawless = {0L, 0L};

	(void)gettimeofday(&now, NULL);
	timersub(&now, &start_time, &total);
	timersub(&flawless_until, &start_time, &flawless);
	printf("GAME OVER -- You lasted %llu seconds.\n", total.tv_sec);
	printf("Flawless (no hits) for %llu seconds!\n", flawless.tv_sec);
}

static void wait_for_key(int key)
{
	int c = key + 1;

	/* make getch() block for input */
	nodelay(stdscr, FALSE);

	while (c != key)
		c = getch();

	/* make getch() non-blocking again */
	nodelay(stdscr, TRUE);
}

static void init_state(struct bike *bike)
{
	int i;

	bike->x = COLS / 2;
	bike->y = LINES - 2;
	bike->hits = 0;
	steps = 0;
	slowness = 5;
	done = FALSE;
	use_colors = (has_colors() && (start_color() != ERR));
	if (use_colors) {
		for (i = 0; i < sizeof(colors); i++)
			init_pair(colors[i].x, colors[i].fg, colors[i].bg);
	}
	(void)gettimeofday(&start_time, NULL);
}

static void get_input(struct bike *bike)
{
	int c = getch();
	switch (c) {
		case 'q':
			done = TRUE;
			break;
		case KEY_LEFT:
		case 'j':
		case 'h':
			if (bike->x > SIDE_EDGE + 1)
				bike->x--;
			break;
		case KEY_RIGHT:
		case 'k':
		case 'l':
			if (bike->x < COLS - 1 - SIDE_EDGE)
				bike->x++;
			break;
		default:
			break;
	}
}

static void advance_game(struct bike *bike)
{
	erase();
	draw_path();
	new_enemies(FALSE);
	advance_enemies();
	draw_enemies();
	draw_bike(bike);
	detect_collisions(bike);
	draw_status_bar(bike);
	refresh();
}

static void detect_collisions(struct bike *bike)
{
	int i;

	for (i = 0; i < NUM_ENEMIES; i++) {
		struct enemy *enemy = &enemies[i];
		if (enemy->used && enemy->x == bike->x && enemy->y == bike->y) {
			if (bike->hits == 0)
				(void)gettimeofday(&flawless_until, NULL);
			bike->hits++;
			enemy->used = FALSE;
		}
	}
}

static void message(int y, int x, const char *fmt, ...)
{
	char msg[1000];
	va_list ap;

	memset(msg, 0, sizeof(msg));
	move(y, x);
	va_start(ap, fmt);
	vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);
	mvaddstr(y, x, msg);
}

static void draw_enemies(void)
{
	int i;

	for (i = 0; i < NUM_ENEMIES; i++) {
		struct enemy *enemy = &enemies[i];
		if (enemy->used) {
			draw_enemy(enemy);
			if (enemy->y > LINES - 1)
				enemy->used = FALSE;
		}
	}
}

static void advance_enemies(void)
{
	int i;

	if (steps < slowness) {
		steps++;
	} else {
		steps = 0;
		for (i = 0; i < NUM_ENEMIES; i++)
			enemies[i].y++;
	}
}

static void new_enemies(bool new_game)
{
	int i;

	for (i = 0; i < NUM_ENEMIES; i++) {
		struct enemy *enemy = &enemies[i];
		if ((!enemy->used) && ((arc4random_uniform(103)) == 0)) {
			init_enemy(new_game, enemy);
			if (!new_game)
				break;
		}
	}
}

static void init_enemy(bool new_game, struct enemy *enemy)
{
	static char *enemy_chars = "o#*";

	enemy->used = TRUE;
	enemy->x = (arc4random() % (PATH_WIDTH - 1)) + SIDE_EDGE + 1;
	if (new_game)
		enemy->y = (arc4random_uniform(PATH_LENGTH / 2)) + TOP_EDGE;
	else
		enemy->y = TOP_EDGE;
	enemy->c = enemy_chars[arc4random_uniform(strlen(enemy_chars))];
	if (use_colors) {
		switch (enemy->c) {
			case 'o':
				enemy->color = COLOR_ENEMY_1;
				break;
			case '#':
				enemy->color = COLOR_ENEMY_2;
				break;
			case '*':
				enemy->color = COLOR_ENEMY_3;
				break;
			default:
				abort();
		}
	}
}

static void draw_enemy(struct enemy *enemy)
{
	if (use_colors)
		color_set(enemy->color, NULL);
	mvaddch(enemy->y, enemy->x, enemy->c);
	if (use_colors)
		color_set(COLOR_DEFAULT, NULL);
}

static void draw_bike(struct bike *bike)
{
	if (use_colors)
		color_set(COLOR_BIKE, NULL);
	mvaddch(bike->y, bike->x, BIKE_CHAR);
	if (use_colors)
		color_set(COLOR_DEFAULT, NULL);
}

static void draw_status_bar(struct bike *bike)
{
	int i;

	if (use_colors)
		color_set(COLOR_STATUS, NULL);
	else
		attron(A_STANDOUT);
	for (i = 0; i < (MAX_HITS - bike->hits); i++) 
		message(LINES - 3 - (i << 1), 3, "%c", BIKE_CHAR);
	message(LINES - 1, 0, "Pos: %.2i - Hits: %i", bike->x, bike->hits);
	if (use_colors)
		color_set(COLOR_DEFAULT, NULL);
	else
		attroff(A_STANDOUT);
}

static void cleanup(void)
{
	if (!isendwin())
		(void)endwin();
	curs_set(1);
}

static void draw_path()
{
	int line;

	if (use_colors)
		color_set(COLOR_PATH, NULL);
	for (line = TOP_EDGE; line < LINES - 1; line++) {
		mvaddch(line, SIDE_EDGE, '|');
		mvaddch(line, COLS - SIDE_EDGE, '|');
	}
	if (use_colors)
		color_set(COLOR_DEFAULT, NULL);
}
