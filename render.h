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

/* Writes the contents of the provided buffer to the terminal. */
void render_buffer_write(struct render_buffer *buf);

/* Appends the first len characters from the provided string to the provided
 * buffer. */
void render_buffer_append(struct render_buffer *buf, const char *s, int len);

/* Frees the memory allocated for the provided buffer and sets the length to
 * zero. */
void render_buffer_free(struct render_buffer *buf);

/* Sets the cursor position to the top left corner. */
void render_set_cursor_home(struct render_buffer *buf);

/* Clears the terminal screen */
void render_clear_screen(struct render_buffer *buf);

/* Renders the provided row at the current cursor position. */
void render_row(struct render_buffer *buf, const char *row, int len, int tab_stop);

/* Sets the terminal cursor to the provided row and column. Position index
 * starts at 1. */
void render_set_cursor_position(struct render_buffer *buf, int row, int col);

#endif /* _RENDER_H_ */
