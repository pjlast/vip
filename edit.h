#ifndef _EDIT_H_
#define _EDIT_H_

/* Inserts a character at the provided position in the provided string and
 * updates the length. */
void edit_insert_char(char **string, int *len, int pos, char c);

void edit_delete_char(char **string, int *len, int pos);

char *edit_split_string(char **string, int *len, int pos);

void edit_append_string(char **string, int *len, char *astring, int alen);

#endif /* _EDIT_H_ */
