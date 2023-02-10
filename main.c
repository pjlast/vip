#include "render.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define TAB_STOP 4

struct row {
  /* Actual file contents. */
  char *chars;
  int length;

  /* Contents to render. */
  char *rchars;
  int rlength;
};

struct file {
  struct row *rows;
  int length;
};

struct editor {
  char *filename;
  struct file file;
  struct render_buffer render_buffer;
  struct row *current_row;
  int screen_rows;
  int screen_cols;
  int file_cursor_row;
  int file_cursor_col;
  int render_cursor_col;
  int render_row_offset;
  int render_col_offset;
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
      int tabs = 0;
      for (int i = 0; i <= linelen; i++) {
        E->file.rows[E->file.length].chars[i] = line[i];
        if (line[i] == '\t')
          tabs++;
      }
      E->file.rows[E->file.length].chars[linelen] = '\0';

      E->file.rows[E->file.length].rchars =
          malloc(linelen + tabs * (TAB_STOP - 1) + 1);
      E->file.rows[E->file.length].rlength = linelen + tabs * (TAB_STOP - 1);

      int i = 0;
      for (int j = 0; j < linelen; j++) {
        if (line[j] == '\t') {
          for (int k = 0; k < TAB_STOP; k++)
            E->file.rows[E->file.length].rchars[i++] = ' ';
        } else {
          E->file.rows[E->file.length].rchars[i++] = line[j];
        }
      }
      E->file.rows[E->file.length].rchars[i] = '\0';

      E->file.length++;
    }
  }

  E->current_row = &E->file.rows[0];
  E->filename = filename;
  fclose(fp);
}

void editor_close(struct editor *E) {
  render_clear_screen(&E->render_buffer);
  render_set_cursor_home(&E->render_buffer);
  render_buffer_write(&E->render_buffer);

  for (int i = 0; i < E->file.length; i++) {
    free(E->file.rows[i].chars);
    free(E->file.rows[i].rchars);
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

void set_render_column(const char *chars, int length, int *file_col,
                       int *render_col, int preferred_col) {
  *render_col = 0;
  *file_col = 0;
  if (chars[0] == '\t')
    *render_col += TAB_STOP - 1;

  while (*render_col < preferred_col && *file_col < length - 1) {
    if (chars[++(*file_col)] == '\t')
      *render_col += TAB_STOP - 1;

    (*render_col)++;
  }
}

int editor_process_input(struct editor *E) {
  char c = editor_read_key();

  switch (c) {
  case '\033':
    if (read(STDIN_FILENO, &c, 1) != 1)
      break;
    if (read(STDIN_FILENO, &c, 1) != 1)
      break;
    switch (c) {
    case 'A':
      c = 'k';
      break;
    case 'B':
      c = 'j';
      break;
    }
    break;
  }

  switch (c) {
  case 'q':
    return 0;
  case 'k':
    if (E->file_cursor_row > 0) {
      E->current_row--;
      E->file_cursor_row--;
      render_buffer_append(&E->render_buffer, "\033M", 3);
      if (E->file_cursor_row < E->render_row_offset) {
        E->render_row_offset--;
        render_buffer_append(&E->render_buffer, "\033[s\r", 4);
        render_buffer_append(&E->render_buffer, E->current_row->rchars,
                             E->current_row->rlength);
        render_buffer_append(&E->render_buffer, "\033[K", 3);
        render_buffer_append(&E->render_buffer, "\033[u", 3);
      }

      int preferred_col = E->render_cursor_col;
      set_render_column(E->current_row->chars, E->current_row->length,
                        &E->file_cursor_col, &E->render_cursor_col,
                        preferred_col);

      render_set_cursor_position(&E->render_buffer,
                                 E->file_cursor_row - E->render_row_offset + 1,
                                 E->render_cursor_col + 1);
    }
    break;
  case 'j': {
    if (E->file_cursor_row < E->file.length - 1) {
      E->file_cursor_row++;
      E->current_row++;
      render_buffer_append(&E->render_buffer, "\033D", 3);
      if (E->file_cursor_row > E->render_row_offset + E->screen_rows - 1) {
        E->render_row_offset++;
        render_buffer_append(&E->render_buffer, "\033[s\r", 4);
        render_buffer_append(&E->render_buffer, E->current_row->rchars,
                             E->current_row->rlength);
        render_buffer_append(&E->render_buffer, "\033[K", 3);
        render_buffer_append(&E->render_buffer, "\033[u", 3);
      }

      int preferred_col = E->render_cursor_col;
      set_render_column(E->current_row->chars, E->current_row->length,
                        &E->file_cursor_col, &E->render_cursor_col,
                        preferred_col);

      render_set_cursor_position(&E->render_buffer,
                                 E->file_cursor_row - E->render_row_offset + 1,
                                 E->render_cursor_col + 1);
    }
    break;
  }
  case 'l':
    if (E->file_cursor_col < E->current_row->length - 1) {
      E->file_cursor_col++;
      if (E->current_row->chars[E->file_cursor_col] == '\t') {
        E->render_cursor_col += TAB_STOP;
        char term_command[16];
        int len =
            snprintf(term_command, sizeof(term_command), "\033[%dC", TAB_STOP);
        render_buffer_append(&E->render_buffer, term_command, len);
      } else {
        E->render_cursor_col++;
        render_buffer_append(&E->render_buffer, "\033[C", 3);
      }
    }
    break;
  case 'h':
    if (E->file_cursor_col > 0) {
      if (E->current_row->chars[E->file_cursor_col] == '\t') {
        E->render_cursor_col -= TAB_STOP;
        char term_command[16];
        int len =
            snprintf(term_command, sizeof(term_command), "\033[%dD", TAB_STOP);
        render_buffer_append(&E->render_buffer, term_command, len);
      } else {
        E->render_cursor_col--;
        render_buffer_append(&E->render_buffer, "\033[D", 3);
      }
      E->file_cursor_col--;
    }
    break;
  }

  return 1;
}

int main(int argc, char *argv[]) {
  struct editor E = {NULL, {NULL, 0}, {NULL, 0}, NULL, 0, 0, 0, 0, 0, 0, 0};

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

  render_buffer_append(&E.render_buffer, "\x1b[2J", 4);

  for (int i = 0; i < E.screen_rows && i < E.file.length; i++) {
    render_buffer_append(&E.render_buffer, E.file.rows[i].rchars,
                         E.file.rows[i].rlength);
    render_buffer_append(&E.render_buffer, "\033[K", 3);
    if (i < E.screen_rows - 1) {
      render_buffer_append(&E.render_buffer, "\r\n", 2);
    }
  }

  set_render_column(E.current_row->chars, E.current_row->length,
                    &E.file_cursor_col, &E.render_cursor_col, 0);
  render_set_cursor_position(&E.render_buffer,
                             E.file_cursor_row - E.render_row_offset + 1,
                             E.render_cursor_col + 1);

  render_buffer_write(&E.render_buffer);

  /* Main loop. */
  while (editor_process_input(&E)) {
    render_buffer_write(&E.render_buffer);
  }

  /* Close editor and reset terminal. */
  editor_close(&E);

  render_termios_set(&orig_termios);
  return 0;
}
