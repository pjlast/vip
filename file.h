#ifndef _FILE_H_
#define _FILE_H_

#include <string.h>

struct line {
  char *chars;
  size_t len;
};

struct file {
  struct line *lines;
  size_t len;
};

struct file *file_open(const char *path);

void file_close(struct file *f);

void file_save(struct file *f, const char *path);

#endif /* _FILE_H_ */
