#include "file.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct file *file_open(const char *path) {
  FILE *fp = fopen(path, "r");
  if (!fp) {
    return NULL;
  }

  struct file *f = malloc(sizeof(struct file));
  *f = (struct file){NULL, 0};

  char *line = NULL;
  size_t len = 0;
  ssize_t read;

  while ((read = getline(&line, &len, fp)) != -1) {
    if (read != -1) {
      while (read > 0 && (line[read - 1] == '\r' || line[read - 1] == '\n'))
        read--;

      f->lines = realloc(f->lines, sizeof(struct line) * (f->len + 1));
      f->lines[f->len].chars = malloc(read + 1);
      memcpy(f->lines[f->len].chars, line, read);
      f->lines[f->len].chars[read] = '\0';
      f->lines[f->len++].len = read;
    }
  }

  free(line);
  fclose(fp);

  return f;
}

void file_close(struct file *f) {
  for (size_t i = 0; i < f->len; i++) {
    free(f->lines[i].chars);
  }
  free(f->lines);
  free(f);
}

void file_save(struct file *f, const char *path) {
  FILE *fp = fopen(path, "w");
  if (!fp) {
    return;
  }

  for (size_t i = 0; i < f->len; i++) {
    fwrite(f->lines[i].chars, f->lines[i].len, 1, fp);
    fwrite("\n", 1, 1, fp);
  }

  fclose(fp);
}
