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
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>

#define VERSION		"1.0"
#define NUM_ENEMIES	400	/* number of enemies. */
#define MAX_HITS	5	/* game is over when MAX_HITS is reached */
#define PATH_WIDTH	30	/* width of bike path */
#define PATH_LENGTH	25	/* length of bike path */
#define SIDE_EDGE	((COLS - PATH_WIDTH) / 2)
#define TOP_EDGE	(LINES - PATH_LENGTH)
#define BIKE_CHAR	'8'	/* the char the represents the bike */

struct enemy {
	bool used;
	int c;
	int x;
	int y;
};

static char *enemy_chars = "o#*";

struct state {
	int done;
	int x;
	int y;
	int bike;
	int delay;
	int steps;
	int speed;
	int hits;
	struct enemy enemies[NUM_ENEMIES];
	struct timeval start_time;
};

static void title_screen(void);
static void game_over(struct state *state);
static void init_enemy(struct enemy *enemy, bool init);
static void draw_enemy(struct enemy *enemy);
static void get_input(struct state *state);
static void advance_game(struct state *state);
static void detect_collisions(struct state *state);
static void init_state(struct state *state);
static void draw_enemies(struct state *state);
static void new_enemies(struct state *state, bool init);
static void advance_enemies(struct state *state);
static void draw_bike(struct state *state);
static void draw_status_bar(struct state *state);
static void draw_path(void);
static void cleanup(void);
static void message(int y, int x, const char *fmt, ...);
static void wait_for_key(int key);

int main(void)
{
	struct state state;
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

	title_screen();

	init_state(&state);
	new_enemies(&state, TRUE);
	advance_game(&state);

	(void)gettimeofday(&last_time, NULL);
	srandom(last_time.tv_sec);

	while (!state.done) {
		(void)gettimeofday(&now, NULL);

		get_input(&state);
		advance_game(&state);

		timersub(&now, &last_time, &res);
		if (res.tv_usec > state.delay)
			usleep(state.delay);
		else
			usleep(res.tv_usec);
		last_time.tv_sec = now.tv_sec;
		last_time.tv_usec = now.tv_usec;

		if (state.hits >= MAX_HITS)
			state.done = TRUE;
	}

	endwin();

	if (state.hits >= MAX_HITS)
		game_over(&state);
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

static void game_over(struct state *state)
{
	struct timeval now = {0L, 0L};
	struct timeval res = {0L, 0L};

	(void)gettimeofday(&now, NULL);
	timersub(&now, &state->start_time, &res);
	printf("GAME OVER -- You lasted %lu seconds.\n", res.tv_sec);
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

static void init_state(struct state *state)
{
	memset(state, 0, sizeof(struct state));
	state->x = COLS / 2;
	state->y = LINES - 2;
	state->bike = BIKE_CHAR;
	state->delay = 30000;
	state->steps = 0;
	state->speed = 5;
	(void)gettimeofday(&state->start_time, NULL);
}

static void get_input(struct state* state)
{
	int c = getch();
	switch (c) {
		case 'q':
			state->done = TRUE;
			break;
		case KEY_LEFT:
		case 'j':
		case 'h':
			if (state->x > SIDE_EDGE + 1)
				state->x--;
			break;
		case KEY_RIGHT:
		case 'k':
		case 'l':
			if (state->x < COLS - 1 - SIDE_EDGE)
				state->x++;
			break;
		default:
			break;
	}
}

static void advance_game(struct state* state)
{
	erase();
	draw_path();
	new_enemies(state, FALSE);
	advance_enemies(state);
	draw_enemies(state);
	draw_bike(state);
	detect_collisions(state);
	draw_status_bar(state);
	refresh();
}

static void detect_collisions(struct state *state)
{
	int i;
	for (i = 0; i < NUM_ENEMIES; i++) {
		struct enemy *enemy = &state->enemies[i];
		if (enemy->used
		    && enemy->x == state->x
		    && enemy->y == state->y) {
			state->hits++;
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
	clrtoeol();
	va_start(ap, fmt);
	vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);
	mvaddstr(y, x, msg);
}

static void draw_enemies(struct state *state)
{
	int i;
	for (i = 0; i < NUM_ENEMIES; i++) {
		struct enemy *enemy = &state->enemies[i];
		if (enemy->used) {
			draw_enemy(enemy);
			if (enemy->y > LINES - 1)
				enemy->used = FALSE;
		}
	}
}

static void advance_enemies(struct state *state)
{
	int i;

	if (state->steps < state->speed) {
		state->steps++;
	} else {
		state->steps = 0;
		for (i = 0; i < NUM_ENEMIES; i++)
			state->enemies[i].y++;
	}
}

static void new_enemies(struct state *state, bool init)
{
	int i;

	for (i = 0; i < NUM_ENEMIES; i++) {
		struct enemy *enemy = &state->enemies[i];
		if ((! enemy->used) && ((random() % 103) == 0)) {
			init_enemy(enemy, init);
			if (!init)
				break;
		}
	}
}

static void init_enemy(struct enemy *enemy, bool init)
{
	enemy->used = TRUE;
	enemy->x = (random() % (PATH_WIDTH - 1)) + SIDE_EDGE + 1;
	if (init)
		enemy->y = (random() % (PATH_LENGTH / 2)) + TOP_EDGE;
	else
		enemy->y = TOP_EDGE;
	enemy->c = enemy_chars[random() % (strlen(enemy_chars))];
}

static void draw_enemy(struct enemy *enemy)
{
	mvaddch(enemy->y, enemy->x, enemy->c);
}

static void draw_bike(struct state *state)
{
	mvaddch(state->y, state->x, state->bike);
}

static void draw_status_bar(struct state* state)
{
	attron(A_STANDOUT);
	message(LINES - 1, 0, "Pos: %.2i - Hits: %i", state->x, state->hits);
	attroff(A_STANDOUT);
}

static void cleanup(void)
{
	if (!isendwin())
		(void)endwin();
}

static void draw_path()
{
	int line;
	for (line = TOP_EDGE; line < LINES - 1; line++) {
		mvaddch(line, SIDE_EDGE, '|');
		mvaddch(line, COLS - SIDE_EDGE, '|');
	}
}
