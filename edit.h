#ifndef _EDIT_H_
#define _EDIT_H_

#include <string.h>

/* Inserts a character at the provided position in the provided string and
 * updates the length. */
void edit_insert_char(char **string, size_t *len, int pos, char c);

void edit_delete_char(char **string, size_t *len, int pos);

char *edit_split_string(char **string, size_t *len, int pos);

void edit_append_string(char **string, size_t *len, char *astring, size_t alen);

#endif /* _EDIT_H_ */
