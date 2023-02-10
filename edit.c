#include "edit.h"
#include <stdlib.h>
#include <string.h>

void edit_insert_char(char **string, int *len, int pos, char c) {
  *string = realloc(*string, *len + 2);
  memcpy(*string + pos + 1, *string + pos, *len - pos);
  (*string)[pos] = c;
  (*string)[++(*len)] = '\0';
}

void edit_delete_char(char **string, int *len, int pos) {
  memcpy(*string + pos, *string + pos + 1, *len - pos);
  (*string)[--(*len)] = '\0';
  *string = realloc(*string, *len + 1);
}

char *edit_split_string(char **string, int *len, int pos) {
  int new_len = *len - pos;
  char *new = malloc(new_len + 1);
  memcpy(new, *string + pos, new_len);
  new[new_len] = '\0';

  *string = realloc(*string, pos + 1);
  (*string)[pos] = '\0';
  *len = pos;
  return new;
}

void edit_append_string(char **string, int *len, char *astring, int alen) {
  *string = realloc(*string, *len + alen + 1);
  memcpy(*string + *len, astring, alen + 1);
  *len += alen;
}
