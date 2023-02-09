#include "render.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

struct row {
  char *chars;
  int length;
};

struct file {
  struct row *rows;
  int length;
};

struct editor {
  char *filename;
  struct file file;
  struct render_buffer render_buffer;
  int screen_rows;
  int screen_cols;
};

void editor_open_file(struct editor *E, char *filename) {
  FILE *fp = fopen(filename, "r");

  char *line = NULL;
  size_t linecap = 0;
  ssize_t linelen;
  while ((linelen = getline(&line, &linecap, fp)) > 0) {
    if (linelen != -1) {
      while (linelen > 0 &&
             (line[linelen - 1] == '\n' || line[linelen - 1] == '\r'))
        linelen--;

      E->file.rows =
          realloc(E->file.rows, sizeof(struct row) * (E->file.length + 1));
      E->file.rows[E->file.length].chars = malloc(linelen + 1);
      E->file.rows[E->file.length].length = linelen;
      memcpy(E->file.rows[E->file.length].chars, line, linelen);
      E->file.rows[E->file.length].chars[linelen] = '\0';
      E->file.length++;
    }
  }

  E->filename = filename;
  fclose(fp);
}

void editor_close(struct editor *E) {
  render_clear_screen(&E->render_buffer);
  render_set_cursor_home(&E->render_buffer);
  render_buffer_write(&E->render_buffer);

  for (int i = 0; i < E->file.length; i++) {
    free(E->file.rows[i].chars);
  }
  free(E->file.rows);
  render_buffer_free(&E->render_buffer);
}

char editor_read_key() {
  char c;
  int nread;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1)
    if (nread == -1 && errno != EAGAIN)
      exit(1);

  return c;
}

int main(int argc, char *argv[]) {
  struct editor E = {NULL, {NULL, 0}, {NULL, 0}, 0, 0};

  if (argc < 2) {
    return 1;
  }

  editor_open_file(&E, argv[1]);

  struct termios orig_termios = render_termios_get();
  struct termios raw = orig_termios;
  render_termios_enable_raw_mode(&raw);
  render_termios_set(&raw);

  if (render_get_window_size(&E.screen_rows, &E.screen_cols) == -1) {
    render_termios_set(&orig_termios);
    return 1;
  }

  render_set_cursor_home(&E.render_buffer);

  for (int i = 0; i < E.screen_rows; i++) {
    render_buffer_append(&E.render_buffer, E.file.rows[i].chars,
                         E.file.rows[i].length);
    render_buffer_append(&E.render_buffer, "\033[K", 3);
    if (i < E.screen_rows - 1) {
      render_buffer_append(&E.render_buffer, "\r\n", 2);
    }
  }

  render_set_cursor_home(&E.render_buffer);

  render_buffer_write(&E.render_buffer);

  /* Main loop. */
  char c;
  while (1) {
    c = editor_read_key();
    switch (c) {
    case 'q':
      editor_close(&E);
      render_termios_set(&orig_termios);
      return 0;
    case 'k':
      render_buffer_append(&E.render_buffer, "\033[A", 3);
      break;
    case 'j':
      render_buffer_append(&E.render_buffer, "\033[B", 3);
      break;
    case 'l':
      render_buffer_append(&E.render_buffer, "\033[C", 3);
      break;
    case 'h':
      render_buffer_append(&E.render_buffer, "\033[D", 3);
      break;
    }

    render_buffer_write(&E.render_buffer);
  }

  /* Close editor and reset terminal. */
  editor_close(&E);

  render_termios_set(&orig_termios);
  return 0;
}