#include "edit.h"
#include "render.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

/* Editor configuration. */
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
  char *filename;
  struct row *rows;
  int length;
};

enum editor_mode {
  MODE_NORMAL,
  MODE_INSERT,
  MODE_COMMAND,
};

struct editor {
  struct file file;
  struct render_buffer render_buffer;

  enum editor_mode mode;

  /* Number of rows and columns on the screen. */
  int screen_rows;
  int screen_cols;

  /* Actual position of the cursor in the file. */
  int file_cursor_row;
  int file_cursor_col;

  /* Required for tabs. The column where the cursor renders is not the same as
   * the column where the cursor is in the file. */
  int render_cursor_col;

  /* Offsets for scrolling. */
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

  E->file.filename = filename;
  fclose(fp);
}

void editor_update_render_row(struct row *row) {
  int tabs = 0;
  for (int i = 0; i < row->length; i++)
    if (row->chars[i] == '\t')
      tabs++;

  row->rchars = realloc(row->rchars, row->length + tabs * (TAB_STOP - 1) + 1);
  row->rlength = row->length + tabs * (TAB_STOP - 1);

  int i = 0;
  for (int j = 0; j < row->length; j++) {
    if (row->chars[j] == '\t') {
      for (int k = 0; k < TAB_STOP; k++)
        row->rchars[i++] = ' ';
    } else {
      row->rchars[i++] = row->chars[j];
    }
  }
  row->rchars[i] = '\0';
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

  switch (E->mode) {
  case MODE_NORMAL:
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
        E->file_cursor_row--;
        render_buffer_append(&E->render_buffer, "\033M", 3);
        if (E->file_cursor_row < E->render_row_offset) {
          E->render_row_offset--;
          render_buffer_append(&E->render_buffer, "\033[s\r", 4);
          render_buffer_append(&E->render_buffer,
                               E->file.rows[E->file_cursor_row].rchars,
                               E->file.rows[E->file_cursor_row].rlength);
          render_buffer_append(&E->render_buffer, "\033[K", 3);
          render_buffer_append(&E->render_buffer, "\033[u", 3);
        }

        int preferred_col = E->render_cursor_col;
        set_render_column(E->file.rows[E->file_cursor_row].chars,
                          E->file.rows[E->file_cursor_row].length,
                          &E->file_cursor_col, &E->render_cursor_col,
                          preferred_col);

        render_set_cursor_position(
            &E->render_buffer, E->file_cursor_row - E->render_row_offset + 1,
            E->render_cursor_col + 1);
      }
      break;
    case 'j': {
      if (E->file_cursor_row < E->file.length - 1) {
        E->file_cursor_row++;
        render_buffer_append(&E->render_buffer, "\033D", 3);
        if (E->file_cursor_row > E->render_row_offset + E->screen_rows - 1) {
          E->render_row_offset++;
          render_buffer_append(&E->render_buffer, "\033[s\r", 4);
          render_buffer_append(&E->render_buffer,
                               E->file.rows[E->file_cursor_row].rchars,
                               E->file.rows[E->file_cursor_row].rlength);
          render_buffer_append(&E->render_buffer, "\033[K", 3);
          render_buffer_append(&E->render_buffer, "\033[u", 3);
        }

        int preferred_col = E->render_cursor_col;
        set_render_column(E->file.rows[E->file_cursor_row].chars,
                          E->file.rows[E->file_cursor_row].length,
                          &E->file_cursor_col, &E->render_cursor_col,
                          preferred_col);

        render_set_cursor_position(
            &E->render_buffer, E->file_cursor_row - E->render_row_offset + 1,
            E->render_cursor_col + 1);
      }
      break;
    }
    case 'l':
      if (E->file_cursor_col < E->file.rows[E->file_cursor_row].length - 1) {
        E->file_cursor_col++;
        if (E->file.rows[E->file_cursor_row].chars[E->file_cursor_col] ==
            '\t') {
          E->render_cursor_col += TAB_STOP;
          char term_command[16];
          int len = snprintf(term_command, sizeof(term_command), "\033[%dC",
                             TAB_STOP);
          render_buffer_append(&E->render_buffer, term_command, len);
        } else {
          E->render_cursor_col++;
          render_buffer_append(&E->render_buffer, "\033[C", 3);
        }
      }
      break;
    case 'h':
      if (E->file_cursor_col > 0) {
        if (E->file.rows[E->file_cursor_row].chars[E->file_cursor_col] ==
            '\t') {
          E->render_cursor_col -= TAB_STOP;
          char term_command[16];
          int len = snprintf(term_command, sizeof(term_command), "\033[%dD",
                             TAB_STOP);
          render_buffer_append(&E->render_buffer, term_command, len);
        } else {
          E->render_cursor_col--;
          render_buffer_append(&E->render_buffer, "\033[D", 3);
        }
        E->file_cursor_col--;
      }
      break;
    case 'a':
      E->file_cursor_col++;
      E->render_cursor_col++;
      render_buffer_append(&E->render_buffer, "\033[C", 3);
    case 'i':
      E->mode = MODE_INSERT;
      break;
    case 'A':
      E->file_cursor_col = E->file.rows[E->file_cursor_row].length;
      set_render_column(E->file.rows[E->file_cursor_row].chars,
                        E->file.rows[E->file_cursor_row].length,
                        &E->file_cursor_col, &E->render_cursor_col, 999);
      E->render_cursor_col++;
      E->file_cursor_col++;
      render_set_cursor_position(&E->render_buffer,
                                 E->file_cursor_row - E->render_row_offset + 1,
                                 E->render_cursor_col + 1);
      E->mode = MODE_INSERT;
      break;
    case ':':
      E->mode = MODE_COMMAND;
      break;
    }
    break;
  case MODE_INSERT:
    switch (c) {
    case '\033':
      if (E->file_cursor_col == E->file.rows[E->file_cursor_row].length) {
        E->file_cursor_col--;
        E->render_cursor_col--;
        render_buffer_append(&E->render_buffer, "\033[D", 3);
      }
      E->mode = MODE_NORMAL;
      break;
    default:
      edit_insert_char(&E->file.rows[E->file_cursor_row].chars,
                       &E->file.rows[E->file_cursor_row].length,
                       E->file_cursor_col, c);
      editor_update_render_row(&E->file.rows[E->file_cursor_row]);
      render_row(&E->render_buffer, E->file.rows[E->file_cursor_row].rchars,
                 E->file.rows[E->file_cursor_row].rlength);
      if (c == '\t') {
        E->render_cursor_col += TAB_STOP;
        char term_command[16];
        int len =
            snprintf(term_command, sizeof(term_command), "\033[%dC", TAB_STOP);
        render_buffer_append(&E->render_buffer, term_command, len);
      } else {
        E->render_cursor_col++;
        render_buffer_append(&E->render_buffer, "\033[C", 3);
      }
      E->file_cursor_col++;
      break;
    }
    break;
  case MODE_COMMAND:
    switch (c) {
    case '\033':
      E->mode = MODE_NORMAL;
      break;
    case '\r':
      E->mode = MODE_NORMAL;
      break;
    }
    break;
  }

  return 1;
}

int main(int argc, char *argv[]) {
  struct editor E = {
      {NULL, NULL, 0}, {NULL, 0}, MODE_NORMAL, 0, 0, 0, 0, 0, 0, 0};

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

  set_render_column(E.file.rows[E.file_cursor_row].chars,
                    E.file.rows[E.file_cursor_row].length, &E.file_cursor_col,
                    &E.render_cursor_col, 0);
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
