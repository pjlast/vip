#include "render.h"

/* Gets the terminal's current termios configuration. */
struct termios render_termios_get() {
  struct termios termios;
  tcgetattr(STDIN_FILENO, &termios);

  return termios;
}

/* Sets the provided termios configuration on the terminal. */
void render_termios_set(struct termios *termios) {
  tcsetattr(STDIN_FILENO, TCSAFLUSH, termios);
}

/* Enable raw mode on the provided struct by setting the required flags. */
void render_termios_enable_raw_mode(struct termios *termios) {
  termios->c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  termios->c_oflag &= ~(OPOST);
  termios->c_cflag |= (CS8);
  termios->c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  termios->c_cc[VMIN] = 0;
  termios->c_cc[VTIME] = 1;
}

int get_cursor_position(int *rows, int *cols) {
  char buf[32];
  unsigned int i = 0;
  if (write(STDOUT_FILENO, "\033[6n", 4) != 4)
    return -1;
  while (i < sizeof(buf) - 1) {
    if (read(STDIN_FILENO, &buf[i], 1) != 1)
      break;
    if (buf[i] == 'R')
      break;
    i++;
  }
  buf[i] = '\0';

  if (buf[0] != '\033' || buf[1] != '[')
    return -1;
  if (sscanf(&buf[2], "%d;%d", rows, cols) != 2)
    return -1;
  return 0;
}

/* Sets the terminal window size in the provided parameters. Returns -1 if
 * unsuccessful. */
int render_get_window_size(int *rows, int *cols) {
  struct winsize ws;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    /* If ioctl fails, try to get the cursor position with brute force. */
    if (write(STDOUT_FILENO, "\033[999C\033[999B", 12) != 12)
      return -1;
    return get_cursor_position(rows, cols);
  } else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

/* Returns the total length of a string, not exceeding the provided maximum
 * length. Escape sequences are ignored when counting towards the maximum
 * length. */
int render_strlen(char *s, int max_len) {
  int len = 0;
  int ctrl_seq = 0;
  while (*s != '\0') {
    if (*s == '\033') {
      ctrl_seq = 1;
    } else if (ctrl_seq && isalpha(*s)) {
      ctrl_seq = 0;
    } else if (!ctrl_seq) {
      max_len--;
      if (max_len < 0)
        break;
    }
    len++;
    s++;
  }
  return len;
}

void render_buffer_append(struct render_buffer *buf, const char *s, int len) {
  char *new = realloc(buf->buf, buf->len + len);
  if (new == NULL)
    return;
  memcpy(&new[buf->len], s, len);
  buf->buf = new;
  buf->len += len;
}

void render_buffer_free(struct render_buffer *buf) {
  free(buf->buf);
  buf->buf = NULL;
  buf->len = 0;
}

void render_buffer_write(struct render_buffer *buf) {
  write(STDOUT_FILENO, buf->buf, buf->len);
  render_buffer_free(buf);
}

void render_set_cursor_home(struct render_buffer *buf) {
  render_buffer_append(buf, "\033[H", 3);
}

void render_set_cursor_position(struct render_buffer *buf, int row, int col) {
  char cursor_pos[16];
  int len = snprintf(cursor_pos, 16, "\033[%d;%dH", row, col);
  render_buffer_append(buf, cursor_pos, len);
}

void render_clear_screen(struct render_buffer *buf) {
  render_buffer_append(buf, "\033[2J", 4);
};

/* Renders the provided row at the current cursor position. */
void render_row(struct render_buffer *buf, const char *row, int len) {
  render_buffer_append(buf, "\033[s\r", 4);
  render_buffer_append(buf, row, len);
  render_buffer_append(buf, "\033[K", 3);
  render_buffer_append(buf, "\033[u", 3);
}
