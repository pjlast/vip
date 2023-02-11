#include "edit.h"
#include "file.h"
#include "render.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

/* Editor configuration. */
#define TAB_STOP 4

enum editor_mode {
  MODE_NORMAL,
  MODE_INSERT,
  MODE_COMMAND,
};

struct editor {
  char *filename;
  struct file *file;
  struct render_buffer render_buffer;

  enum editor_mode mode;

  /* Number of lines and columns on the screen. */
  int screen_lines;
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
  E->file = file_open(filename);
  E->filename = filename;
}

void editor_save_file(struct editor *E, char *filename) {
  file_save(E->file, filename);
}

void editor_close(struct editor *E) {
  render_clear_screen(&E->render_buffer);
  render_set_cursor_home(&E->render_buffer);
  render_buffer_write(&E->render_buffer);

  file_close(E->file);
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

void file_insert_row(struct file *file, int at, char *s, size_t len) {
  if (at < 0 || at > file->len)
    return;

  file->lines = realloc(file->lines, sizeof(struct line) * (file->len + 1));
  memmove(&file->lines[at + 1], &file->lines[at],
          sizeof(struct line) * (file->len - at));

  file->lines[at] = (struct line){NULL, 0};
  file->lines[at].chars = malloc(len + 1);
  memcpy(file->lines[at].chars, s, len);
  file->lines[at].chars[len] = '\0';
  file->lines[at].len = len;

  file->len++;
}

void file_delete_row(struct file *file, int at) {
  if (at < 0 || at > file->len)
    return;

  memmove(&file->lines[at], &file->lines[at + 1],
          sizeof(struct line) * (file->len - at - 1));
  file->lines = realloc(file->lines, sizeof(struct line) * (file->len - 1));
  file->len--;
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
    case 'k':
      if (E->file_cursor_row > 0) {
        E->file_cursor_row--;
        render_buffer_append(&E->render_buffer, "\033M", 2);
        if (E->file_cursor_row < E->render_row_offset) {
          E->render_row_offset--;
          render_buffer_append(&E->render_buffer, "\033[s\r", 4);
          render_buffer_append(&E->render_buffer,
                               E->file->lines[E->file_cursor_row].chars,
                               E->file->lines[E->file_cursor_row].len);
          render_buffer_append(&E->render_buffer, "\033[K", 3);
          render_buffer_append(&E->render_buffer, "\033[u", 3);
        }

        int preferred_col = E->render_cursor_col;
        E->file_cursor_col = 0;
        E->render_cursor_col = 0;
        while (1) {
          if (E->file->lines[E->file_cursor_row].chars[E->file_cursor_col] ==
              '\t')
            E->render_cursor_col += TAB_STOP - 1;
          if (E->render_cursor_col < preferred_col &&
              E->file_cursor_col + 1 < E->file->lines[E->file_cursor_row].len) {
            E->render_cursor_col++;
            E->file_cursor_col++;
          } else {
            break;
          }
        }

        render_set_cursor_position(
            &E->render_buffer, E->file_cursor_row - E->render_row_offset + 1,
            E->render_cursor_col + 1);
      }
      break;
    case 'j': {
      if (E->file_cursor_row + 1 < E->file->len) {
        E->file_cursor_row++;
        render_buffer_append(&E->render_buffer, "\n", 1);
        if (E->file_cursor_row > E->render_row_offset + E->screen_lines - 1) {
          E->render_row_offset++;
          render_buffer_append(&E->render_buffer, "\033[s\r", 4);
          render_buffer_append(&E->render_buffer,
                               E->file->lines[E->file_cursor_row].chars,
                               E->file->lines[E->file_cursor_row].len);
          render_buffer_append(&E->render_buffer, "\033[K", 3);
          render_buffer_append(&E->render_buffer, "\033[u", 3);
        }

        int preferred_col = E->render_cursor_col;
        E->file_cursor_col = 0;
        E->render_cursor_col = 0;
        while (1) {
          if (E->file->lines[E->file_cursor_row].chars[E->file_cursor_col] ==
              '\t')
            E->render_cursor_col += TAB_STOP - 1;
          if (E->render_cursor_col < preferred_col &&
              E->file_cursor_col + 1 < E->file->lines[E->file_cursor_row].len) {
            E->render_cursor_col++;
            E->file_cursor_col++;
          } else {
            break;
          }
        }

        render_set_cursor_position(
            &E->render_buffer, E->file_cursor_row - E->render_row_offset + 1,
            E->render_cursor_col + 1);
      }
      break;
    }
    case 'l':
      if (E->file_cursor_col + 1 < E->file->lines[E->file_cursor_row].len) {
        E->file_cursor_col++;
        if (E->file->lines[E->file_cursor_row].chars[E->file_cursor_col] ==
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
        if (E->file->lines[E->file_cursor_row].chars[E->file_cursor_col] ==
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
      if (E->file->lines[E->file_cursor_row].len > 0) {
        E->file_cursor_col++;
        E->render_cursor_col++;
        render_buffer_append(&E->render_buffer, "\033[C", 3);
      }
    case 'i':
      E->mode = MODE_INSERT;
      break;
    case 'A':
      if (E->file->lines[E->file_cursor_row].len > 0) {
        E->file_cursor_col = E->file->lines[E->file_cursor_row].len;
        set_render_column(E->file->lines[E->file_cursor_row].chars,
                          E->file->lines[E->file_cursor_row].len,
                          &E->file_cursor_col, &E->render_cursor_col, 999);
        E->render_cursor_col++;
        E->file_cursor_col++;
        render_set_cursor_position(
            &E->render_buffer, E->file_cursor_row - E->render_row_offset + 1,
            E->file_cursor_col + 1);
      }
      E->mode = MODE_INSERT;
      break;
    case 'x':
      if (E->file->lines[E->file_cursor_row].len > 0) {
        char del_char =
            E->file->lines[E->file_cursor_row].chars[E->file_cursor_col];
        edit_delete_char(&E->file->lines[E->file_cursor_row].chars,
                         &E->file->lines[E->file_cursor_row].len,
                         E->file_cursor_col);
        // editor_update_render_row(&E->file.lines[E->file_cursor_row]);
        render_row(&E->render_buffer, E->file->lines[E->file_cursor_row].chars,
                   E->file->lines[E->file_cursor_row].len, TAB_STOP);

        if (E->file_cursor_col >= E->file->lines[E->file_cursor_row].len) {
          E->file_cursor_col--;
          if (del_char == '\t') {
            E->render_cursor_col -= TAB_STOP;
            char term_command[16];
            int len = snprintf(term_command, sizeof(term_command), "\033[%dD",
                               TAB_STOP);
            render_buffer_append(&E->render_buffer, term_command, len);
          } else {
            E->render_cursor_col--;
            render_buffer_append(&E->render_buffer, "\033[D", 3);
          }
        } else {
          if (del_char == '\t') {
            E->render_cursor_col -= TAB_STOP - 1;
            char term_command[16];
            int len = snprintf(term_command, sizeof(term_command), "\033[%dD",
                               TAB_STOP - 1);
            render_buffer_append(&E->render_buffer, term_command, len);
          }
          if (E->file->lines[E->file_cursor_row].chars[E->file_cursor_col] ==
              '\t') {
            E->render_cursor_col += TAB_STOP - 1;
            char term_command[16];
            int len = snprintf(term_command, sizeof(term_command), "\033[%dC",
                               TAB_STOP - 1);
            render_buffer_append(&E->render_buffer, term_command, len);
          }
        }
      }
      break;
    case ':':
      E->mode = MODE_COMMAND;
      break;
    }
    break;
  case MODE_INSERT:
    switch (c) {
    case '\033':
      if (E->file_cursor_col == E->file->lines[E->file_cursor_row].len) {
        E->file_cursor_col--;
        E->render_cursor_col--;
        render_buffer_append(&E->render_buffer, "\033[D", 3);
      }
      E->mode = MODE_NORMAL;
      break;
    case 127: {
      if (E->file_cursor_col > 0) {
        char del_char =
            E->file->lines[E->file_cursor_row].chars[E->file_cursor_col - 1];
        edit_delete_char(&E->file->lines[E->file_cursor_row].chars,
                         &E->file->lines[E->file_cursor_row].len,
                         E->file_cursor_col - 1);
        render_row(&E->render_buffer, E->file->lines[E->file_cursor_row].chars,
                   E->file->lines[E->file_cursor_row].len, TAB_STOP);
        if (del_char == '\t') {
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
      } else {
        E->file_cursor_row--;
        set_render_column(E->file->lines[E->file_cursor_row].chars,
                          E->file->lines[E->file_cursor_row].len,
                          &E->file_cursor_col, &E->render_cursor_col, 999);
        int preferred_col = E->render_cursor_col;
        if (E->file->lines[E->file_cursor_row].len > 0)
          preferred_col++;
        edit_append_string(&E->file->lines[E->file_cursor_row].chars,
                           &E->file->lines[E->file_cursor_row].len,
                           E->file->lines[E->file_cursor_row + 1].chars,
                           E->file->lines[E->file_cursor_row + 1].len);
        file_delete_row(E->file, E->file_cursor_row + 1);
        render_buffer_append(&E->render_buffer, "\033M", 2);
        for (int i = E->file_cursor_row;
             i < E->screen_lines + E->render_row_offset - 1 && i < E->file->len;
             i++) {
          render_row(&E->render_buffer, E->file->lines[i].chars,
                     E->file->lines[i].len, TAB_STOP);
          if (i < E->screen_lines + E->render_row_offset - 2) {
            render_buffer_append(&E->render_buffer, "\r\n", 2);
          }
        }
        set_render_column(E->file->lines[E->file_cursor_row].chars,
                          E->file->lines[E->file_cursor_row].len,
                          &E->file_cursor_col, &E->render_cursor_col,
                          preferred_col);
        render_set_cursor_position(
            &E->render_buffer, E->file_cursor_row - E->render_row_offset + 1,
            E->render_cursor_col + 1);
      }
      break;
    }
    case '\r': {
      char *new_row = edit_split_string(
          &E->file->lines[E->file_cursor_row].chars,
          &E->file->lines[E->file_cursor_row].len, E->file_cursor_col);
      render_row(&E->render_buffer, E->file->lines[E->file_cursor_row].chars,
                 E->file->lines[E->file_cursor_row].len, TAB_STOP);
      file_insert_row(E->file, E->file_cursor_row + 1, new_row,
                      strlen(new_row));
      render_buffer_append(&E->render_buffer, "\r\n", 2);
      E->file_cursor_row++;
      E->file_cursor_col = 0;
      E->render_cursor_col = 0;
      if (E->file_cursor_row > E->render_row_offset + E->screen_lines - 1) {
        E->render_row_offset++;
      }

      char term_command[32];
      int len = snprintf(term_command, sizeof(term_command),
                         "\033[s\033[%d;%dr\033[u\033M\033[r\033u",
                         E->file_cursor_row - E->render_row_offset + 1,
                         E->screen_lines);
      render_buffer_append(&E->render_buffer, term_command, len);
      render_set_cursor_position(&E->render_buffer,
                                 E->file_cursor_row - E->render_row_offset + 1,
                                 E->render_cursor_col + 1);
      render_row(&E->render_buffer, E->file->lines[E->file_cursor_row].chars,
                 E->file->lines[E->file_cursor_row].len, TAB_STOP);
      set_render_column(E->file->lines[E->file_cursor_row].chars,
                        E->file->lines[E->file_cursor_row].len,
                        &E->file_cursor_col, &E->render_cursor_col, 0);
      render_set_cursor_position(&E->render_buffer,
                                 E->file_cursor_row - E->render_row_offset + 1,
                                 E->render_cursor_col + 1);
      break;
    }
    default:
      edit_insert_char(&E->file->lines[E->file_cursor_row].chars,
                       &E->file->lines[E->file_cursor_row].len,
                       E->file_cursor_col, c);
      render_row(&E->render_buffer, E->file->lines[E->file_cursor_row].chars,
                 E->file->lines[E->file_cursor_row].len, TAB_STOP);
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
    case 'w':
      editor_save_file(E, E->filename);
      E->mode = MODE_NORMAL;
      break;
    case 'q':
      return 0;
    case '\r':
      E->mode = MODE_NORMAL;
      break;
    }
    break;
  }

  return 1;
}

int main(int argc, char *argv[]) {
  struct editor E = {NULL, NULL, {NULL, 0}, MODE_NORMAL, 0, 0, 0, 0, 0, 0, 0};

  if (argc < 2) {
    return 1;
  }

  editor_open_file(&E, argv[1]);

  struct termios orig_termios = render_termios_get();
  struct termios raw = orig_termios;
  render_termios_enable_raw_mode(&raw);
  render_termios_set(&raw);

  if (render_get_window_size(&E.screen_lines, &E.screen_cols) == -1) {
    render_termios_set(&orig_termios);
    return 1;
  }

  render_set_cursor_home(&E.render_buffer);

  render_buffer_append(&E.render_buffer, "\x1b[2J", 4);

  for (int i = 0; i < E.screen_lines && i < E.file->len; i++) {
    render_row(&E.render_buffer, E.file->lines[i].chars, E.file->lines[i].len,
               TAB_STOP);
    if (i < E.screen_lines - 1) {
      render_buffer_append(&E.render_buffer, "\r\n", 2);
    }
  }

  render_set_cursor_position(&E.render_buffer,
                             E.file_cursor_row - E.render_row_offset + 1,
                             E.file_cursor_col + 1);

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
