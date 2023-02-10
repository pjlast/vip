#include "edit.h"
#include <stdlib.h>
#include <string.h>

void edit_insert_char(char **string, int *len, int pos, char c) {
  *string = realloc(*string, *len + 2);
  memcpy(*string + pos + 1, *string + pos, *len - pos);
  (*string)[pos] = c;
  (*string)[++(*len)] = '\0';
}
