#ifndef _RENDER_H_
#define _RENDER_H_

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

struct render_buffer {
  char *buf;
  int len;
};

/* Gets the terminal's current termios configuration. */
struct termios render_termios_get();

/* Sets the provided termios configuration on the terminal. */
void render_termios_set(struct termios *termios);

/* Enables raw mode on the provided termios. */
void render_termios_enable_raw_mode(struct termios *termios);

/* Sets the terminal window size in the provided parameters. Returns -1 if
 * unsuccessful. */
int render_get_window_size(int *rows, int *cols);

void render_line(int row, int start_col, int max_len, char *line, int clear);

void render_buffer_write(struct render_buffer *buf);

void render_buffer_append(struct render_buffer *buf, const char *s, int len);

void render_buffer_free(struct render_buffer *buf);

void render_set_cursor_home(struct render_buffer *buf);

void render_clear_screen(struct render_buffer *buf);

#endif /* _RENDER_H_ */
