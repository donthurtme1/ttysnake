#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <time.h>
#include <termios.h>
#include <unistd.h>

void
death_animation()
{
	/* Flash twice then recede from the head to tail */
}

void
win_animation()
{
	/* The whole board disintegrates (using 🮐 dither unicode
	 * characters) and then print text "you win!" */

	/* Or the game could continue if there's an endless mode.
	 * The current snake colour becomes the background and
	 * a new snake appears */
}

static inline void
get_cursor_pos(int *restrict x, int *restrict y)
{
	fwrite("\e[6n", 1, 5, stdout);
	scanf("\e[%d;%dR", y, x);
}

int
main(int argc, char *argv[])
{
	/* Setup */
	struct winsize win;
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &win);

	struct termios save_attr, new_attr;
	tcgetattr(1, &save_attr);
	new_attr = save_attr;
	new_attr.c_lflag &= ~(ICANON | ECHO);
	tcsetattr(1, 0, &new_attr);

	srand(time(NULL));


	/* Print borders */
	const int game_width = 32, game_height = 24;
	const int startx = (win.ws_col - game_width * 2) / 2;

	printf("\e[%dG", startx - 1);
	for (int x = 0; x < (game_width + 1) * 2; x++)
		putc('#', stdout);
	putc('\n', stdout);

	for (int y = 0; y < game_height; y++) {
		printf("\e[%dG#", startx - 1);
		printf("\e[%dC#\n", game_width * 2);
	}

	printf("\e[%dG", startx - 1);
	for (int x = 0; x < (game_width + 1) * 2; x++)
		putc('#', stdout);
	putc('\n', stdout);

	/* Reset cursor position and store coordinates of top left corner */
	int tl_x, tl_y;
	printf("\e[?25l\e[%dA\e[%dG\e[41m\e[0m", game_height + 1, startx);
	get_cursor_pos(&tl_x, &tl_y);


	/* Game */
	int snake_x_pos = game_width / 2,
		snake_y_pos = game_height / 2;
	const int max_snake_size = game_width * game_height;
	struct _vec2 {
		int x, y;
	} snake_body_pos[max_snake_size];
	int snake_len = 3, snake_head_offset = 0;
	memset(snake_body_pos, 0, max_snake_size * sizeof(struct _vec2));

	int berry_x_pos = rand() % game_width,
		berry_y_pos = rand() % game_height;
	enum { NONE = -1, RIGHT, UP, LEFT, DOWN } move_dir = RIGHT,
		input_buffer[2] = { NONE, NONE };

	struct pollfd fds = { .fd = STDIN_FILENO, .events = POLLIN };
	struct timespec wait = { 0, 210000000 };
	struct timespec framestart;
	for (int poll_result = 0; ;
			poll_result = ppoll(&fds, 1, &wait, NULL))
	{
		if (poll_result > 0) {
			/* Handle input */
			int c = getc(stdin);
			if (c == 'q' || c == '')
				break;
			if (input_buffer[0] == NONE) {
buffer_to_idx_0:
				if (c == 'd' && move_dir != LEFT)
					input_buffer[0] = RIGHT;
				else if (c == 'w' && move_dir != DOWN)
					input_buffer[0] = UP;
				else if (c == 'a' && move_dir != RIGHT)
					input_buffer[0] = LEFT;
				else if (c == 's' && move_dir != UP)
					input_buffer[0] = DOWN;
			} else if (input_buffer[1] == NONE) {
buffer_to_idx_1:
				if (c == 'd' && input_buffer[0] != LEFT)
					input_buffer[1] = RIGHT;
				else if (c == 'w' && input_buffer[0] != DOWN)
					input_buffer[1] = UP;
				else if (c == 'a' && input_buffer[0] != RIGHT)
					input_buffer[1] = LEFT;
				else if (c == 's' && input_buffer[0] != UP)
					input_buffer[1] = DOWN;
				else
					goto buffer_to_idx_0;
			} else {
				input_buffer[0] = input_buffer[1];
				goto buffer_to_idx_1;
			}

			struct timespec currenttime;
			clock_gettime(CLOCK_MONOTONIC, &currenttime);
			long timediff = (currenttime.tv_nsec + (currenttime.tv_sec * 1000000000)) -
				(framestart.tv_nsec + (framestart.tv_sec * 1000000000));
			wait.tv_nsec = 210000000 - timediff;
			continue;
		} else if (poll_result == -1) {
			perror("poll");
			return 1;
		} /* poll result */

		/* Update snake head position */
		if (input_buffer[0] != NONE) {
			move_dir = input_buffer[0];
			input_buffer[0] = input_buffer[1];
			input_buffer[1] = NONE;
		}
		switch (move_dir) {
			case RIGHT:
				snake_x_pos++;
				printf("\e[2C");
				break;
			case UP:
				snake_y_pos--;
				printf("\e[A");
				break;
			case LEFT:
				snake_x_pos--;
				printf("\e[2D");
				break;
			case DOWN:
				snake_y_pos++;
				printf("\e[B");
				break;
			default:
				break;
		} /* switch (move_dir) */

		/* Update snake tail position */
		for (int i = snake_len; i > 0; i--) {
			snake_body_pos[i].x = snake_body_pos[i - 1].x;
			snake_body_pos[i].y = snake_body_pos[i - 1].y;
		}
		snake_body_pos[0].x = snake_x_pos;
		snake_body_pos[0].y = snake_y_pos;


		/* Collision checks */
		for (int i = 1; i < snake_len; i++) {
			if (snake_x_pos == snake_body_pos[i].x &&
					snake_y_pos == snake_body_pos[i].y) {
				goto exit;
			}
			if (snake_x_pos >= game_width ||
					snake_y_pos >= game_height) {
				goto exit;
			}
		}

		if (snake_x_pos == berry_x_pos &&
				snake_y_pos == berry_y_pos) {
			snake_len += 1;
			berry_x_pos = rand() % game_width;
			berry_y_pos = rand() % game_height;
		}


		/* Draw snake and berry */
		printf("\e[%d;%dH\e[41m  \e[%d;%dH\e[42m  \e[0m\e[2D",
				berry_y_pos + tl_y, berry_x_pos * 2 + tl_x,
				snake_y_pos + tl_y, snake_x_pos * 2 + tl_x);
		printf("\e[%d;%dH  ",
				snake_body_pos[snake_len].y + tl_y,
				snake_body_pos[snake_len].x * 2 + tl_x);
		fflush(stdout);

		/* Clock */
		wait.tv_nsec = 210000000;
		clock_gettime(CLOCK_MONOTONIC, &framestart);
	} /* for (int poll_result = 0; ;
		 poll_result = ppoll(&fds, 1, &wait, NULL)) */


exit:
	printf("\e[%d;1H\n\e[?25h", tl_y + game_height);
	tcsetattr(1, 0, &save_attr);
	return 0;
}
